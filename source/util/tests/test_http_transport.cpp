#include "cheats/sync/http_transport_ps5.hpp"
#include "test_harness.h"

#include <functional>

using onion::cheats::sync::HttpRequest;
using onion::cheats::sync::Ps5HttpTransport;
using onion::cheats::sync::SyncStatus;
using onion::cheats::sync::http_request_follows_redirects;

namespace {

SyncStatus perform(const char *url, const char *allow, bool exact_match) {
  Ps5HttpTransport transport;
  HttpRequest request;
  request.url = url;
  request.host_allow = allow;
  request.exact_host_match = exact_match;
  return transport.perform(request, {});
}

int test_legacy_subdomain_allowlist(void) {
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Unavailable),
                     static_cast<int>(perform(
                         "https://sub.api.example.com/data", "api.example.com",
                         false)));
  return 0;
}

int test_exact_host_allowlist(void) {
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Unavailable),
                     static_cast<int>(perform(
                         "https://API.Example.com/data", "api.example.com",
                         true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(perform(
                         "https://sub.api.example.com/data", "api.example.com",
                         true)));
  return 0;
}

int test_invalid_url_and_allow_hosts(void) {
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(
                         perform("ftp://api.example.com/data",
                                 "api.example.com", true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(
                         perform("https:///data", "api.example.com", true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(
                         perform("https://user@api.example.com/data",
                                 "api.example.com", true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(
                         perform("https://api_example.com/data",
                                 "api.example.com", true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(SyncStatus::Rejected),
                     static_cast<int>(
                         perform("https://api.example.com:bad/data",
                                 "api.example.com", true)));
  TEST_ASSERT_EQ_INT(static_cast<int>(
                         SyncStatus::Rejected),
                     static_cast<int>(
                         perform("https://api.example.com/data",
                                 "api..example.com", true)));
  return 0;
}

int test_redirect_policy(void) {
  HttpRequest request;
  TEST_ASSERT_TRUE(http_request_follows_redirects(request));
  request.exact_host_match = true;
  TEST_ASSERT_TRUE(!http_request_follows_redirects(request));
  request.follow_redirects = false;
  request.exact_host_match = false;
  TEST_ASSERT_TRUE(!http_request_follows_redirects(request));
  return 0;
}

} // namespace

extern "C" int test_http_transport_suite(void) {
  int failures = 0;
  failures += onion_test_run("http_transport_legacy_subdomains",
                             test_legacy_subdomain_allowlist);
  failures += onion_test_run("http_transport_exact_host",
                             test_exact_host_allowlist);
  failures += onion_test_run("http_transport_invalid_hosts",
                             test_invalid_url_and_allow_hosts);
  failures += onion_test_run("http_transport_redirect_policy",
                             test_redirect_policy);
  return failures;
}
