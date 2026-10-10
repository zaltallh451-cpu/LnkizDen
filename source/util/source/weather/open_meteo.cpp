#include "weather/open_meteo.hpp"

#include "cheats/sync/i_http_transport.hpp"
#include "cJSON.hpp"
#include "onion_cjson.hpp"

#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <functional>
#include <locale>
#include <sstream>
#include <string>

namespace onion::weather {
namespace {
using onion::cheats::sync::HttpRequest;
using onion::cheats::sync::IHttpTransport;
using onion::cheats::sync::SyncStatus;
using onion::weather::WeatherResultCode;

constexpr size_t kMaxResponseBytes = 64 * 1024;

std::string trim(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

WeatherResultCode require_string(const cJSON *object, const char *key,
                                 std::string &out) {
  if (!object || !key) {
    return WeatherResultCode::MissingField;
  }
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!item) {
    return WeatherResultCode::MissingField;
  }
  if (!cJSON_IsString(item) || !item->valuestring) {
    return WeatherResultCode::InvalidType;
  }
  out = item->valuestring;
  return out.empty() ? WeatherResultCode::MissingField : WeatherResultCode::Ok;
}

WeatherResultCode require_double(const cJSON *object, const char *key,
                                 double &out) {
  if (!object || !key) {
    return WeatherResultCode::MissingField;
  }
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!item) {
    return WeatherResultCode::MissingField;
  }
  if (!cJSON_IsNumber(item)) {
    return WeatherResultCode::InvalidType;
  }
  out = item->valuedouble;
  return std::isfinite(out) ? WeatherResultCode::Ok
                            : WeatherResultCode::InvalidRange;
}

WeatherResultCode require_int(const cJSON *object, const char *key, int &out) {
  if (!object || !key) {
    return WeatherResultCode::MissingField;
  }
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, key);
  if (!item) {
    return WeatherResultCode::MissingField;
  }
  if (!cJSON_IsNumber(item)) {
    return WeatherResultCode::InvalidType;
  }
  const double value = item->valuedouble;
  if (!std::isfinite(value) || std::floor(value) != value) {
    return WeatherResultCode::InvalidType;
  }
  if (value < 0 || value > 99) {
    return WeatherResultCode::InvalidRange;
  }
  out = static_cast<int>(value);
  return WeatherResultCode::Ok;
}

bool is_valid_range(double value, double min_value, double max_value) {
  return std::isfinite(value) && value >= min_value && value <= max_value;
}

bool is_valid_utf8(const std::string &value) {
  for (size_t i = 0; i < value.size();) {
    const auto byte = static_cast<unsigned char>(value[i]);
    if (byte <= 0x7f) {
      ++i;
      continue;
    }

    size_t length = 0;
    unsigned char second_min = 0x80;
    unsigned char second_max = 0xbf;
    if (byte >= 0xc2 && byte <= 0xdf) {
      length = 2;
    } else if (byte >= 0xe0 && byte <= 0xef) {
      length = 3;
      if (byte == 0xe0) {
        second_min = 0xa0;
      } else if (byte == 0xed) {
        second_max = 0x9f;
      }
    } else if (byte >= 0xf0 && byte <= 0xf4) {
      length = 4;
      if (byte == 0xf0) {
        second_min = 0x90;
      } else if (byte == 0xf4) {
        second_max = 0x8f;
      }
    } else {
      return false;
    }

    if (length > value.size() - i) {
      return false;
    }
    const auto second = static_cast<unsigned char>(value[i + 1]);
    if (second < second_min || second > second_max) {
      return false;
    }
    for (size_t offset = 2; offset < length; ++offset) {
      const auto continuation = static_cast<unsigned char>(value[i + offset]);
      if (continuation < 0x80 || continuation > 0xbf) {
        return false;
      }
    }
    i += length;
  }
  return true;
}

std::string url_encode_component(const std::string &value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (unsigned char ch : value) {
    const bool safe = (ch >= 'A' && ch <= 'Z') ||
                      (ch >= 'a' && ch <= 'z') ||
                      (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                      ch == '.' || ch == '~';
    if (safe) {
      out.push_back(static_cast<char>(ch));
    } else {
      char buffer[4];
      std::snprintf(buffer, sizeof(buffer), "%%%02X", ch);
      out.append(buffer);
    }
  }
  return out;
}

std::pair<SyncStatus, std::string> fetch_json(IHttpTransport &http,
                                             const std::string &url,
                                             const std::string &host) {
  HttpRequest req;
  req.url = url.c_str();
  req.method = "GET";
  req.user_agent = "LnkizDen";
  req.host_allow = host.c_str();
  req.exact_host_match = true;
  req.follow_redirects = false;
  req.status_min = 200;
  req.status_max = 299;
  req.timeout_ms = 5000;
  req.max_body_bytes = kMaxResponseBytes;

  std::string body;
  const SyncStatus status = http.perform(
      req, [&body](const void *data, size_t len) {
        if (len > 0 && !data) {
          return SyncStatus::Protocol;
        }
        if (data && len > 0) {
          if (body.size() > kMaxResponseBytes ||
              len > kMaxResponseBytes - body.size()) {
            return SyncStatus::Protocol;
          }
          const char *raw = static_cast<const char *>(data);
          body.append(raw, len);
        }
        return SyncStatus::Ok;
      });
  return {status, body};
}

WeatherResult make_error(WeatherResultCode code, const std::string &message) {
  WeatherResult result;
  result.code = code;
  result.error = message;
  return result;
}

} // namespace

OpenMeteoClient::OpenMeteoClient(IHttpTransport &http) : http_(http) {}

bool OpenMeteoClient::isValidCityName(const std::string &value) {
  const std::string cleaned = trim(value);
  if (cleaned.empty() || cleaned.size() > 80) {
    return false;
  }
  if (!is_valid_utf8(cleaned)) {
    return false;
  }
  for (unsigned char ch : cleaned) {
    if (ch >= 0x80 || std::isalnum(ch) || ch == ' ' || ch == '-' ||
        ch == '.' || ch == '\'' || ch == ',' || ch == '(' || ch == ')') {
      continue;
    }
    return false;
  }
  return true;
}

std::string OpenMeteoClient::urlEncode(const std::string &value) {
  return url_encode_component(value);
}

WeatherResult OpenMeteoClient::fetchByCity(const std::string &city) const {
  const std::string cleaned = trim(city);
  if (!isValidCityName(cleaned)) {
    return make_error(WeatherResultCode::InvalidCity,
                     "city must be non-empty and contain letters or spaces");
  }

  const std::string geocoding_url =
      "https://geocoding-api.open-meteo.com/v1/search?name=" +
      urlEncode(cleaned) + "&count=1&language=en&format=json";
  const auto geocoding =
      fetch_json(http_, geocoding_url, "geocoding-api.open-meteo.com");
  if (geocoding.first != SyncStatus::Ok) {
    return make_error(
        geocoding.first == SyncStatus::Rejected ? WeatherResultCode::HttpRejected
                                               : WeatherResultCode::TransportFailure,
        geocoding.first == SyncStatus::Rejected ? "geocoding request rejected"
                                               : "geocoding request failed");
  }

  cJSON *geocode_root = onion_cjson::parse_exact(
      geocoding.second.data(), geocoding.second.size());
  if (!geocode_root) {
    return make_error(WeatherResultCode::MalformedJson, "geocoding response is not valid JSON");
  }
  cJSON *results = cJSON_GetObjectItemCaseSensitive(geocode_root, "results");
  if (!results || !cJSON_IsArray(results) || cJSON_GetArraySize(results) <= 0) {
    cJSON_Delete(geocode_root);
    return make_error(WeatherResultCode::MissingField, "city lookup returned no results");
  }

  cJSON *location = cJSON_GetArrayItem(results, 0);
  std::string city_name, region, country;
  WeatherResultCode field_result =
      require_string(location, "name", city_name);
  if (field_result == WeatherResultCode::Ok) {
    field_result = require_string(location, "country", country);
  }
  if (field_result != WeatherResultCode::Ok) {
    cJSON_Delete(geocode_root);
    return make_error(field_result, "city lookup has invalid required fields");
  }
  const cJSON *admin1 = cJSON_GetObjectItemCaseSensitive(location, "admin1");
  if (admin1 && !cJSON_IsString(admin1)) {
    cJSON_Delete(geocode_root);
    return make_error(WeatherResultCode::InvalidType,
                      "city lookup region has an invalid type");
  }
  if (admin1 && admin1->valuestring) {
    region = admin1->valuestring;
  }

  double latitude = 0.0;
  double longitude = 0.0;
  field_result = require_double(location, "latitude", latitude);
  if (field_result == WeatherResultCode::Ok) {
    field_result = require_double(location, "longitude", longitude);
  }
  if (field_result != WeatherResultCode::Ok) {
    cJSON_Delete(geocode_root);
    return make_error(field_result, "city lookup has invalid coordinates");
  }
  if (!is_valid_range(latitude, -90.0, 90.0) ||
      !is_valid_range(longitude, -180.0, 180.0)) {
    cJSON_Delete(geocode_root);
    return make_error(WeatherResultCode::InvalidRange,
                     "city lookup coordinates are out of range");
  }
  cJSON_Delete(geocode_root);

  std::ostringstream lat_stream;
  lat_stream.imbue(std::locale::classic());
  lat_stream << latitude;
  std::ostringstream lon_stream;
  lon_stream.imbue(std::locale::classic());
  lon_stream << longitude;

  const std::string forecast_url =
      "https://api.open-meteo.com/v1/forecast?latitude=" + lat_stream.str() +
      "&longitude=" + lon_stream.str() +
      "&current=temperature_2m,apparent_temperature,weather_code,wind_speed_10m,wind_gusts_10m&timezone=auto&forecast_days=1";
  const auto forecast = fetch_json(http_, forecast_url, "api.open-meteo.com");
  if (forecast.first != SyncStatus::Ok) {
    return make_error(
        forecast.first == SyncStatus::Rejected ? WeatherResultCode::HttpRejected
                                              : WeatherResultCode::TransportFailure,
        forecast.first == SyncStatus::Rejected ? "forecast request rejected"
                                              : "forecast request failed");
  }

  cJSON *forecast_root = onion_cjson::parse_exact(
      forecast.second.data(), forecast.second.size());
  if (!forecast_root) {
    return make_error(WeatherResultCode::MalformedJson,
                     "forecast response is not valid JSON");
  }

  WeatherResult result;
  result.code = WeatherResultCode::Ok;
  result.location.city = city_name;
  result.location.region = region;
  result.location.country = country;

  field_result = require_string(forecast_root, "timezone",
                                result.location.timezone);
  if (field_result != WeatherResultCode::Ok) {
    cJSON_Delete(forecast_root);
    return make_error(field_result, "forecast has invalid timezone");
  }

  const cJSON *current = cJSON_GetObjectItemCaseSensitive(forecast_root, "current");
  if (!current || !cJSON_IsObject(current)) {
    cJSON_Delete(forecast_root);
    return make_error(WeatherResultCode::MissingField, "forecast missing current conditions");
  }

  field_result =
      require_string(current, "time", result.location.local_time);
  if (field_result != WeatherResultCode::Ok) {
    cJSON_Delete(forecast_root);
    return make_error(field_result, "forecast has invalid local time");
  }

  double temperature = 0.0;
  double apparent = 0.0;
  double wind_speed = 0.0;
  double wind_gust = 0.0;
  int weather_code = 0;
  field_result = require_double(current, "temperature_2m", temperature);
  if (field_result == WeatherResultCode::Ok) {
    field_result =
        require_double(current, "apparent_temperature", apparent);
  }
  if (field_result == WeatherResultCode::Ok) {
    field_result = require_double(current, "wind_speed_10m", wind_speed);
  }
  if (field_result == WeatherResultCode::Ok) {
    field_result = require_double(current, "wind_gusts_10m", wind_gust);
  }
  if (field_result == WeatherResultCode::Ok) {
    field_result = require_int(current, "weather_code", weather_code);
  }
  if (field_result != WeatherResultCode::Ok) {
    cJSON_Delete(forecast_root);
    return make_error(field_result,
                      "forecast has invalid current weather fields");
  }
  if (!is_valid_range(temperature, -100.0, 100.0) ||
      !is_valid_range(apparent, -100.0, 100.0) ||
      !is_valid_range(wind_speed, 0.0, 200.0) ||
      !is_valid_range(wind_gust, 0.0, 300.0) ||
      weather_code < 0 || weather_code > 99) {
    cJSON_Delete(forecast_root);
    return make_error(WeatherResultCode::InvalidRange,
                     "forecast weather values are out of range");
  }

  result.conditions.temperature_c = temperature;
  result.conditions.apparent_temperature_c = apparent;
  result.conditions.weather_code = weather_code;
  result.conditions.wind_speed_kmh = wind_speed;
  result.conditions.wind_gust_kmh = wind_gust;

  cJSON_Delete(forecast_root);
  return result;
}

} // namespace onion::weather
