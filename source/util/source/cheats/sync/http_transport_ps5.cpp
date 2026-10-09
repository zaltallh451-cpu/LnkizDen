#include "cheats/sync/http_transport_ps5.hpp"

#include <onion/log.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#if !defined(ONION_HOST_TEST)
#include <curl/curl.h>

#include <functional>

extern "C" {
__asm__(
    ".pushsection .rodata\n"
    ".balign 1\n"
    ".global onion_curl_ca_bundle_start\n"
    "onion_curl_ca_bundle_start:\n"
    ".incbin \"" ONION_CURL_CA_BUNDLE "\"\n"
    ".global onion_curl_ca_bundle_end\n"
    "onion_curl_ca_bundle_end:\n"
    ".popsection\n");
extern const unsigned char onion_curl_ca_bundle_start[];
extern const unsigned char onion_curl_ca_bundle_end[];
}
#endif

namespace onion::cheats::sync {
namespace {

const char *scheme_host(const char *url) {
  if (!url) {
    return nullptr;
  }
  if (std::strncmp(url, "https://", 8) == 0) {
    return url + 8;
  }
  if (std::strncmp(url, "http://", 7) == 0) {
    return url + 7;
  }
  return nullptr;
}

bool valid_hostname(std::string_view host) {
  if (host.empty() || host.front() == '.' || host.back() == '.') {
    return false;
  }
  bool previous_dot = false;
  bool label_starts_with_hyphen = false;
  for (size_t i = 0; i < host.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(host[i]);
    if (ch == '.') {
      if (previous_dot || label_starts_with_hyphen || host[i - 1] == '-') {
        return false;
      }
      previous_dot = true;
      label_starts_with_hyphen = false;
    } else {
      const bool is_ascii_alnum =
          (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
          (ch >= '0' && ch <= '9');
      if (!is_ascii_alnum && ch != '-') {
        return false;
      }
      if (!previous_dot && i == 0) {
        label_starts_with_hyphen = ch == '-';
      } else if (previous_dot) {
        label_starts_with_hyphen = ch == '-';
      }
      previous_dot = false;
    }
  }
  return !label_starts_with_hyphen && host.back() != '-';
}

char ascii_lower(char ch) {
  const unsigned char value = static_cast<unsigned char>(ch);
  return static_cast<char>(std::tolower(value));
}

bool equal_ascii_case_insensitive(std::string_view lhs,
                                  std::string_view rhs) {
  return lhs.size() == rhs.size() &&
         std::equal(lhs.begin(), lhs.end(), rhs.begin(),
                    [](char a, char b) {
                      return ascii_lower(a) == ascii_lower(b);
                    });
}

bool extract_url_host(const char *url, std::string_view &host_out) {
  const char *authority = scheme_host(url);
  if (!authority || !authority[0]) {
    return false;
  }

  const std::string_view remainder(authority);
  const size_t authority_end = remainder.find_first_of("/?#");
  const std::string_view authority_view =
      remainder.substr(0, authority_end);
  if (authority_view.empty() ||
      authority_view.find('@') != std::string_view::npos) {
    return false;
  }

  const size_t port_pos = authority_view.find(':');
  const std::string_view host =
      authority_view.substr(0, port_pos);
  if (!valid_hostname(host)) {
    return false;
  }
  if (port_pos != std::string_view::npos) {
    const std::string_view port = authority_view.substr(port_pos + 1);
    if (port.empty()) {
      return false;
    }
    unsigned int port_number = 0;
    for (unsigned char ch : port) {
      if (!std::isdigit(ch)) {
        return false;
      }
      port_number = port_number * 10 + static_cast<unsigned int>(ch - '0');
      if (port_number > 65535) {
        return false;
      }
    }
    if (port_number == 0) {
      return false;
    }
  }
  host_out = host;
  return true;
}

bool host_allowed(const char *url, const char *allow, bool exact_match) {
  std::string_view host;
  if (!extract_url_host(url, host)) {
    return false;
  }
  if (!allow || !allow[0]) {
    return !exact_match;
  }

  const std::string_view allowed(allow);
  if (!valid_hostname(allowed)) {
    return false;
  }
  if (equal_ascii_case_insensitive(host, allowed)) {
    return true;
  }
  if (exact_match || host.size() <= allowed.size() + 1) {
    return false;
  }
  const size_t suffix_start = host.size() - allowed.size();
  return host[suffix_start - 1] == '.' &&
         equal_ascii_case_insensitive(host.substr(suffix_start), allowed);
}

#if !defined(ONION_HOST_TEST)
// OpenSSL X509 verify codes surfaced via CURLINFO_SSL_VERIFYRESULT.
constexpr long kX509CertNotYetValid = 9;  // X509_V_ERR_CERT_NOT_YET_VALID
constexpr long kX509CertHasExpired = 10;  // X509_V_ERR_CERT_HAS_EXPIRED

extern "C" int sceNetInit(void);

struct CurlXfer {
  const std::function<SyncStatus(const void *, size_t)> *on_data = nullptr;
  HttpProgressFn on_progress = nullptr;
  void *progress_user = nullptr;
  SyncCancelFn should_cancel = nullptr;
  void *cancel_user = nullptr;
  SyncStatus st = SyncStatus::Ok;
  size_t bytes = 0;
  size_t max_body_bytes = 0;
  size_t last_progress_bytes = 0;
  int last_progress_percent = -2;
};

size_t curl_size(curl_off_t value) {
  if (value <= 0) {
    return 0;
  }
  return static_cast<size_t>(value);
}

extern "C" int onion_curl_xferinfo(void *userdata, curl_off_t download_total,
                                    curl_off_t download_now, curl_off_t,
                                    curl_off_t) {
  auto *xfer = static_cast<CurlXfer *>(userdata);
  if (!xfer) {
    return 0;
  }
  if (xfer->should_cancel && xfer->should_cancel(xfer->cancel_user)) {
    xfer->st = SyncStatus::Cancelled;
    return 1;
  }
  if (!xfer->on_progress)
    return 0;

  const size_t received = curl_size(download_now);
  const size_t total = curl_size(download_total);
  const int percent = total > 0
                          ? static_cast<int>(std::min<size_t>(
                                100, (received * 100) / total))
                          : -1;
  const bool report_percent =
      percent >= 0 && percent != xfer->last_progress_percent;
  const bool report_bytes =
      percent < 0 &&
      (xfer->last_progress_percent == -2 ||
       received >= xfer->last_progress_bytes + 1024 * 1024);
  if (report_percent || report_bytes) {
    xfer->last_progress_percent = percent;
    xfer->last_progress_bytes = received;
    xfer->on_progress(received, total, xfer->progress_user);
  }
  return 0;
}

extern "C" size_t onion_curl_write(char *ptr, size_t size, size_t nmemb,
                                   void *userdata) {
  auto *xfer = static_cast<CurlXfer *>(userdata);
  const size_t n = size * nmemb;
  if (!xfer) {
    return n;
  }
  if (xfer->max_body_bytes != 0 &&
      (xfer->bytes > xfer->max_body_bytes ||
       n > xfer->max_body_bytes - xfer->bytes)) {
    xfer->st = SyncStatus::Protocol;
    return 0;
  }
  xfer->bytes += n;
  if (xfer->on_data && n > 0) {
    xfer->st = (*xfer->on_data)(ptr, n);
    if (xfer->st != SyncStatus::Ok) {
      return 0;
    }
  }
  return n;
}

bool ensure_curl() {
  static int state = 0;
  if (state != 0) {
    return state > 0;
  }

  const int net = sceNetInit();
  LOG_DEBUG("http curl sceNetInit rc=%d", net);

  const CURLcode rc = curl_global_init(CURL_GLOBAL_DEFAULT);
  if (rc != CURLE_OK) {
    LOG_ERROR("http curl_global_init failed: %s (%d)", curl_easy_strerror(rc),
              static_cast<int>(rc));
    state = -1;
    return false;
  }
  LOG_DEBUG("http curl ready version=%s", curl_version());
  const size_t ca_bundle_size = static_cast<size_t>(
      reinterpret_cast<uintptr_t>(onion_curl_ca_bundle_end) -
      reinterpret_cast<uintptr_t>(onion_curl_ca_bundle_start));
  LOG_DEBUG("http curl ca bundle embedded bytes=%zu verify=peer+host",
            ca_bundle_size);
  state = 1;
  return true;
}
#endif

} // namespace

SyncStatus Ps5HttpTransport::perform(
    const HttpRequest &req,
    const std::function<SyncStatus(const void *, size_t)> &on_data) {
  const char *url = req.url ? req.url : "";
  const char *method = req.method ? req.method : "GET";
  LOG_DEBUG("http perform timeout_ms=%d status=%d-%d exact_host=%d "
            "follow_redirects=%d",
            req.timeout_ms, req.status_min, req.status_max,
            req.exact_host_match ? 1 : 0, req.follow_redirects ? 1 : 0);

  if (!host_allowed(req.url, req.host_allow, req.exact_host_match)) {
    LOG_ERROR("http request rejected: invalid scheme, host, or host policy");
    return SyncStatus::Rejected;
  }
#if defined(ONION_HOST_TEST)
  (void)on_data;
  return SyncStatus::Unavailable;
#else
  if (!ensure_curl()) {
    return SyncStatus::Unavailable;
  }

  CURL *curl = curl_easy_init();
  if (!curl) {
    LOG_ERROR("http curl_easy_init failed");
    return SyncStatus::Unavailable;
  }

  CurlXfer xfer;
  xfer.on_data = on_data ? &on_data : nullptr;
  xfer.on_progress = req.on_progress;
  xfer.progress_user = req.progress_user;
  xfer.should_cancel = req.should_cancel;
  xfer.cancel_user = req.cancel_user;
  xfer.max_body_bytes = req.max_body_bytes;

  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_USERAGENT,
                   req.user_agent && req.user_agent[0] ? req.user_agent
                                                       : "LnkizDen");
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION,
                   http_request_follows_redirects(req) ? 1L : 0L);
  curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 3L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  const size_t ca_bundle_size = static_cast<size_t>(
      reinterpret_cast<uintptr_t>(onion_curl_ca_bundle_end) -
      reinterpret_cast<uintptr_t>(onion_curl_ca_bundle_start));
  struct curl_blob ca_bundle {
    const_cast<unsigned char *>(onion_curl_ca_bundle_start), ca_bundle_size,
        CURL_BLOB_NOCOPY
  };
  curl_easy_setopt(curl, CURLOPT_CAINFO_BLOB, &ca_bundle);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
  curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, onion_curl_write);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &xfer);
  if (req.on_progress || req.should_cancel) {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, onion_curl_xferinfo);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &xfer);
  }
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   req.timeout_ms > 0 && req.timeout_ms < 30000
                       ? static_cast<long>(req.timeout_ms)
                       : 30000L);
  if (req.timeout_ms > 0) {
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(req.timeout_ms));
  }
  if (std::strcmp(method, "POST") == 0) {
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.body ? req.body : "");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                     static_cast<curl_off_t>(req.body_len));
  }
  struct curl_slist *headers = nullptr;
  if (req.content_type && req.content_type[0]) {
    std::string line = "Content-Type: ";
    line += req.content_type;
    headers = curl_slist_append(headers, line.c_str());
  }
  if (req.accept && req.accept[0]) {
    std::string line = "Accept: ";
    line += req.accept;
    headers = curl_slist_append(headers, line.c_str());
  }
  if (headers) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  }
  const CURLcode rc = curl_easy_perform(curl);
  long http_code = 0;
  double namelookup = 0;
  double connect = 0;
  double total = 0;
  long ssl_verify = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
  curl_easy_getinfo(curl, CURLINFO_NAMELOOKUP_TIME, &namelookup);
  curl_easy_getinfo(curl, CURLINFO_CONNECT_TIME, &connect);
  curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &total);
  if (rc == CURLE_PEER_FAILED_VERIFICATION) {
    curl_easy_getinfo(curl, CURLINFO_SSL_VERIFYRESULT, &ssl_verify);
  }

  LOG_DEBUG("http result curl=%s(%d) http=%ld dns=%.3fs "
            "connect=%.3fs total=%.3fs bytes=%zu xfer=%s",
            curl_easy_strerror(rc), static_cast<int>(rc), http_code,
            namelookup, connect, total, xfer.bytes, sync_status_name(xfer.st));

  if (headers) {
    curl_slist_free_all(headers);
  }
  curl_easy_cleanup(curl);

  if (xfer.st != SyncStatus::Ok) {
    return xfer.st;
  }
  if (rc != CURLE_OK) {
    if (rc == CURLE_PEER_FAILED_VERIFICATION) {
      LOG_ERROR("http tls verify failed verify=%ld", ssl_verify);
      if (ssl_verify == kX509CertNotYetValid ||
          ssl_verify == kX509CertHasExpired) {
        return SyncStatus::Clock;
      }
      return SyncStatus::Tls;
    }
    return SyncStatus::Network;
  }
  const int min_code = req.status_min > 0 ? req.status_min : 200;
  const int max_code = req.status_max > 0 ? req.status_max : 299;
  if (http_code < min_code || http_code > max_code) {
    LOG_ERROR("http status out of range code=%ld want=%d-%d", http_code,
              min_code, max_code);
    return SyncStatus::Network;
  }
  return SyncStatus::Ok;
#endif
}

} // namespace onion::cheats::sync
