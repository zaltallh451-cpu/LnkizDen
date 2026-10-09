#pragma once

#include "cheats/sync/i_http_transport.hpp"

#include <string>

namespace onion::weather {

enum class WeatherResultCode {
  Ok = 0,
  InvalidCity,
  TransportFailure,
  HttpRejected,
  MalformedJson,
  MissingField,
  InvalidType,
  InvalidRange,
};

struct WeatherLocation {
  std::string city;
  std::string region;
  std::string country;
  std::string timezone;
  std::string local_time;
};

struct WeatherConditions {
  double temperature_c = 0.0;
  double apparent_temperature_c = 0.0;
  int weather_code = 0;
  double wind_speed_kmh = 0.0;
  double wind_gust_kmh = 0.0;
};

struct WeatherResult {
  WeatherResultCode code = WeatherResultCode::Ok;
  std::string error;
  WeatherLocation location;
  WeatherConditions conditions;
};

class OpenMeteoClient final {
public:
  explicit OpenMeteoClient(onion::cheats::sync::IHttpTransport &http);

  WeatherResult fetchByCity(const std::string &city) const;

  static std::string urlEncode(const std::string &value);
  static bool isValidCityName(const std::string &value);

private:
  onion::cheats::sync::IHttpTransport &http_;
};

} // namespace onion::weather
