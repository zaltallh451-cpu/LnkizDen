#include "weather/open_meteo.hpp"

#include "test_harness.h"

#include "cheats/sync/i_http_transport.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <utility>
#include <vector>

using onion::cheats::sync::HttpRequest;
using onion::cheats::sync::IHttpTransport;
using onion::cheats::sync::SyncStatus;
using onion::weather::OpenMeteoClient;
using onion::weather::WeatherResult;
using onion::weather::WeatherResultCode;

namespace {

class FakeTransport final : public IHttpTransport {
public:
  explicit FakeTransport(std::vector<std::string> bodies,
                         std::vector<SyncStatus> statuses = {})
      : bodies_(std::move(bodies)), statuses_(std::move(statuses)) {}

  SyncStatus perform(const HttpRequest &req,
                     const std::function<SyncStatus(const void *, size_t)> &on_data)
      override {
    requested_urls_.push_back(req.url ? req.url : "");
    requested_hosts_.push_back(req.host_allow ? req.host_allow : "");
    exact_host_matches_.push_back(req.exact_host_match);
    redirect_policies_.push_back(req.follow_redirects);
    timeouts_.push_back(req.timeout_ms);
    response_limits_.push_back(req.max_body_bytes);

    const size_t idx = requested_urls_.size() - 1;
    const SyncStatus status =
        !statuses_.empty() ? statuses_[std::min(idx, statuses_.size() - 1)]
                           : SyncStatus::Ok;
    if (status != SyncStatus::Ok) {
      return status;
    }

    if (idx < bodies_.size() && on_data) {
      const std::string &body = bodies_[idx];
      if (!body.empty()) {
        return on_data(body.data(), body.size());
      }
    }
    return SyncStatus::Ok;
  }

  const std::vector<std::string> &requested_urls() const { return requested_urls_; }
  const std::vector<std::string> &requested_hosts() const { return requested_hosts_; }
  const std::vector<bool> &exact_host_matches() const {
    return exact_host_matches_;
  }
  const std::vector<bool> &redirect_policies() const {
    return redirect_policies_;
  }
  const std::vector<int> &timeouts() const { return timeouts_; }
  const std::vector<size_t> &response_limits() const {
    return response_limits_;
  }

private:
  std::vector<std::string> bodies_;
  std::vector<SyncStatus> statuses_;
  std::vector<std::string> requested_urls_;
  std::vector<std::string> requested_hosts_;
  std::vector<bool> exact_host_matches_;
  std::vector<bool> redirect_policies_;
  std::vector<int> timeouts_;
  std::vector<size_t> response_limits_;
};

int test_success_flow(void) {
  const std::string geocoding =
      R"({"results":[{"name":"Riyadh","admin1":"Riyadh Province","country":"Saudi Arabia","latitude":24.7136,"longitude":46.6753}]})";
  const std::string forecast =
      R"({"timezone":"Asia/Riyadh","current":{"time":"2026-10-10T12:00","temperature_2m":31.5,"apparent_temperature":32.1,"weather_code":1,"wind_speed_10m":11.0,"wind_gusts_10m":16.5}})";

  FakeTransport http({geocoding, forecast});
  OpenMeteoClient client(http);
  const WeatherResult result = client.fetchByCity("Riyadh");

  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::Ok), static_cast<int>(result.code));
  TEST_ASSERT_STREQ("Riyadh", result.location.city.c_str());
  TEST_ASSERT_STREQ("Riyadh Province", result.location.region.c_str());
  TEST_ASSERT_STREQ("Saudi Arabia", result.location.country.c_str());
  TEST_ASSERT_STREQ("Asia/Riyadh", result.location.timezone.c_str());
  TEST_ASSERT_STREQ("2026-10-10T12:00", result.location.local_time.c_str());
  TEST_ASSERT_TRUE(std::fabs(result.conditions.temperature_c - 31.5) < 0.0001);
  TEST_ASSERT_TRUE(std::fabs(result.conditions.apparent_temperature_c - 32.1) < 0.0001);
  TEST_ASSERT_EQ_INT(1, result.conditions.weather_code);
  TEST_ASSERT_TRUE(std::fabs(result.conditions.wind_speed_kmh - 11.0) < 0.0001);
  TEST_ASSERT_TRUE(std::fabs(result.conditions.wind_gust_kmh - 16.5) < 0.0001);
  TEST_ASSERT_EQ_INT(2, static_cast<int>(http.requested_urls().size()));
  TEST_ASSERT_TRUE(http.requested_urls()[0].find("name=Riyadh") != std::string::npos);
  TEST_ASSERT_TRUE(http.requested_hosts()[0] == "geocoding-api.open-meteo.com");
  TEST_ASSERT_TRUE(http.requested_hosts()[1] == "api.open-meteo.com");
  TEST_ASSERT_TRUE(http.exact_host_matches()[0]);
  TEST_ASSERT_TRUE(http.exact_host_matches()[1]);
  TEST_ASSERT_TRUE(!http.redirect_policies()[0]);
  TEST_ASSERT_TRUE(!http.redirect_policies()[1]);
  TEST_ASSERT_EQ_INT(5000, http.timeouts()[0]);
  TEST_ASSERT_EQ_INT(64 * 1024,
                     static_cast<int>(http.response_limits()[0]));
  return 0;
}

int test_city_validation_and_url_encoding(void) {
  TEST_ASSERT_TRUE(OpenMeteoClient::isValidCityName("Riyadh"));
  TEST_ASSERT_TRUE(OpenMeteoClient::isValidCityName("New York"));
  TEST_ASSERT_TRUE(OpenMeteoClient::isValidCityName("Z\xc3\xbcrich"));
  TEST_ASSERT_TRUE(!OpenMeteoClient::isValidCityName(""));
  TEST_ASSERT_TRUE(!OpenMeteoClient::isValidCityName("Riyadh!!!"));
  TEST_ASSERT_TRUE(!OpenMeteoClient::isValidCityName(std::string("\xc3", 1)));
  TEST_ASSERT_TRUE(!OpenMeteoClient::isValidCityName("\xed\xa0\x80"));
  const std::string encoded_city = OpenMeteoClient::urlEncode("New York");
  TEST_ASSERT_STREQ("New%20York", encoded_city.c_str());
  const std::string encoded_unicode =
      OpenMeteoClient::urlEncode("Z\xc3\xbcrich");
  TEST_ASSERT_STREQ("Z%C3%BCrich", encoded_unicode.c_str());

  FakeTransport invalid_http({});
  OpenMeteoClient invalid_client(invalid_http);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::InvalidCity),
      static_cast<int>(invalid_client.fetchByCity(" ").code));
  TEST_ASSERT_EQ_INT(0, static_cast<int>(invalid_http.requested_urls().size()));

  const std::string geocoding =
      R"({"results":[{"name":"New York","country":"United States","latitude":40.7,"longitude":-74.0}]})";
  const std::string forecast =
      R"({"timezone":"America/New_York","current":{"time":"2026-10-10T12:00","temperature_2m":20,"apparent_temperature":20,"weather_code":0,"wind_speed_10m":0,"wind_gusts_10m":0}})";
  FakeTransport http({geocoding, forecast});
  OpenMeteoClient client(http);
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::Ok),
                     static_cast<int>(client.fetchByCity("New York").code));
  TEST_ASSERT_TRUE(http.requested_urls()[0].find("name=New%20York") !=
                   std::string::npos);
  return 0;
}

int test_transport_failure(void) {
  FakeTransport transport({R"({"results":[{"name":"Riyadh","country":"Saudi Arabia","latitude":24.7,"longitude":46.6}]})"}, {SyncStatus::Network});
  OpenMeteoClient client(transport);
  const WeatherResult result = client.fetchByCity("Riyadh");
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::TransportFailure), static_cast<int>(result.code));
  return 0;
}

int test_malformed_json_and_missing_fields(void) {
  FakeTransport malformed({"{bad json"});
  OpenMeteoClient malformed_client(malformed);
  WeatherResult malformed_result = malformed_client.fetchByCity("Riyadh");
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::MalformedJson), static_cast<int>(malformed_result.code));

  FakeTransport truncated({R"({"results":[)"});
  OpenMeteoClient truncated_client(truncated);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::MalformedJson),
      static_cast<int>(truncated_client.fetchByCity("Riyadh").code));

  FakeTransport trailing({R"({"results":[]} trailing)"});
  OpenMeteoClient trailing_client(trailing);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::MalformedJson),
      static_cast<int>(trailing_client.fetchByCity("Riyadh").code));

  const std::string empty_results = R"({"results":[]})";
  FakeTransport missing({empty_results});
  OpenMeteoClient missing_client(missing);
  WeatherResult missing_result = missing_client.fetchByCity("Riyadh");
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::MissingField), static_cast<int>(missing_result.code));

  FakeTransport api_error({R"({"error":true,"reason":"bad request"})"});
  OpenMeteoClient api_error_client(api_error);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::MissingField),
      static_cast<int>(api_error_client.fetchByCity("Riyadh").code));

  const std::string geocoding =
      R"({"results":[{"name":"Riyadh","country":"Saudi Arabia","latitude":24.7,"longitude":46.6}]})";
  const std::string missing_timezone =
      R"({"current":{"time":"2026-10-10T12:00","temperature_2m":31.5,"apparent_temperature":32.1,"weather_code":1,"wind_speed_10m":11,"wind_gusts_10m":16.5}})";
  FakeTransport missing_forecast_field({geocoding, missing_timezone});
  OpenMeteoClient missing_forecast_client(missing_forecast_field);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::MissingField),
      static_cast<int>(missing_forecast_client.fetchByCity("Riyadh").code));
  return 0;
}

int test_range_validation(void) {
  const std::string geocoding =
      R"({"results":[{"name":"Riyadh","country":"Saudi Arabia","latitude":24.7136,"longitude":46.6753}]})";
  const std::string forecast =
      R"({"timezone":"Asia/Riyadh","current":{"time":"2026-10-10T12:00","temperature_2m":200.0,"apparent_temperature":32.1,"weather_code":1,"wind_speed_10m":11.0,"wind_gusts_10m":16.5}})";

  FakeTransport transport({geocoding, forecast});
  OpenMeteoClient client(transport);
  const WeatherResult result = client.fetchByCity("Riyadh");
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::InvalidRange), static_cast<int>(result.code));

  const std::string geocoding_ok =
      R"({"results":[{"name":"Riyadh","country":"Saudi Arabia","latitude":24.7136,"longitude":46.6753}]})";
  const std::string forecast_bad =
      R"({"timezone":"Asia/Riyadh","current":{"time":"2026-10-10T12:00","temperature_2m":31.5,"apparent_temperature":32.1,"weather_code":-1,"wind_speed_10m":11.0,"wind_gusts_10m":16.5}})";
  FakeTransport transport2({geocoding_ok, forecast_bad});
  OpenMeteoClient client2(transport2);
  const WeatherResult result2 = client2.fetchByCity("Riyadh");
  TEST_ASSERT_EQ_INT(static_cast<int>(WeatherResultCode::InvalidRange), static_cast<int>(result2.code));

  const std::string forecast_wrong_type =
      R"({"timezone":"Asia/Riyadh","current":{"time":"2026-10-10T12:00","temperature_2m":"31.5","apparent_temperature":32.1,"weather_code":1,"wind_speed_10m":11.0,"wind_gusts_10m":16.5}})";
  FakeTransport wrong_type({geocoding_ok, forecast_wrong_type});
  OpenMeteoClient wrong_type_client(wrong_type);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::InvalidType),
      static_cast<int>(wrong_type_client.fetchByCity("Riyadh").code));

  const std::string bad_coordinates =
      R"({"results":[{"name":"Riyadh","country":"Saudi Arabia","latitude":91,"longitude":46.6}]})";
  FakeTransport out_of_range({bad_coordinates});
  OpenMeteoClient out_of_range_client(out_of_range);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::InvalidRange),
      static_cast<int>(out_of_range_client.fetchByCity("Riyadh").code));
  return 0;
}

int test_response_limit(void) {
  FakeTransport transport({std::string(64 * 1024 + 1, 'x')});
  OpenMeteoClient client(transport);
  TEST_ASSERT_EQ_INT(
      static_cast<int>(WeatherResultCode::TransportFailure),
      static_cast<int>(client.fetchByCity("Riyadh").code));
  return 0;
}

} // namespace

extern "C" int test_weather_suite(void) {
  int failures = 0;
  failures += onion_test_run("open_meteo_success", test_success_flow);
  failures += onion_test_run("open_meteo_city_validation", test_city_validation_and_url_encoding);
  failures += onion_test_run("open_meteo_transport_failure", test_transport_failure);
  failures += onion_test_run("open_meteo_malformed_json", test_malformed_json_and_missing_fields);
  failures += onion_test_run("open_meteo_range_validation", test_range_validation);
  failures += onion_test_run("open_meteo_response_limit", test_response_limit);
  return failures;
}
