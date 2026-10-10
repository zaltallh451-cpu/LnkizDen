/* Copyright (C) 2025 OnionHEN / LightningMods */

#include "daemon_ops.hpp"
#include "globalconf.hpp"

#include <onion/net.h>
#include <onion/notify.h>
#include <onion/ready.h>
#include <onion/settings.hpp>
#include <onion/version.h>
#include <cJSON.h>

#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
extern const char companion_index_start[];
extern const char companion_index_end[];
extern const char companion_css_start[];
extern const char companion_css_end[];
extern const char companion_js_start[];
extern const char companion_js_end[];
}

namespace {

constexpr uint16_t kPort = 9049;
constexpr size_t kMaxHeaderBytes = 4096;
constexpr size_t kMaxBodyBytes = 8192;
constexpr size_t kMaxRequestBytes = kMaxHeaderBytes + kMaxBodyBytes;
constexpr std::chrono::seconds kSessionLifetime(3600);
constexpr std::chrono::seconds kRequestLifetime(10);
constexpr char kRecoveryDirectory[] = "/data/LnkizDen/companion/recovery";
constexpr size_t kMaxRecoveryPoints = 16;
constexpr size_t kTimelineCapacity = 32;

struct HttpRequest {
  std::string method;
  std::string target;
  std::string body;
  std::vector<std::pair<std::string, std::string>> headers;

  const std::string *header(std::string_view name) const {
    for (const auto &entry : headers) {
      if (entry.first == name)
        return &entry.second;
    }
    return nullptr;
  }
};

struct TimelineEvent {
  std::time_t time = 0;
  std::string stage;
  std::string outcome;
  std::string detail;
};

struct Session {
  std::string token;
  std::string peer_ip;
  std::chrono::steady_clock::time_point last_used{};
};

struct PairingState {
  std::string code;
  std::chrono::steady_clock::time_point code_created{};
  Session session;
  unsigned failed_attempts = 0;
  std::chrono::steady_clock::time_point attempt_window{};
  std::chrono::steady_clock::time_point blocked_until{};
};

std::mutex g_timeline_mutex;
std::array<TimelineEvent, kTimelineCapacity> g_timeline{};
size_t g_timeline_start = 0;
size_t g_timeline_count = 0;
std::atomic<uint32_t> g_firmware_version{0};
std::atomic_bool g_listener_ready{false};

std::string lower_ascii(std::string value) {
  for (char &ch : value)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return value;
}

std::string trim(std::string value) {
  const size_t first = value.find_first_not_of(" \t");
  if (first == std::string::npos)
    return {};
  const size_t last = value.find_last_not_of(" \t");
  return value.substr(first, last - first + 1);
}

std::string json_escape(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const unsigned char ch : value) {
    switch (ch) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (ch < 0x20) {
        char escaped[7];
        std::snprintf(escaped, sizeof(escaped), "\\u%04x", ch);
        out += escaped;
      } else {
        out += static_cast<char>(ch);
      }
    }
  }
  return out;
}

bool read_random_bytes(uint8_t *bytes, size_t size) {
  if (!bytes || size == 0)
    return false;
  const int fd = open("/dev/urandom", O_RDONLY);
  if (fd < 0)
    return false;
  size_t offset = 0;
  while (offset < size) {
    const ssize_t count = read(fd, bytes + offset, size - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0) {
      close(fd);
      return false;
    }
    offset += static_cast<size_t>(count);
  }
  return close(fd) == 0;
}

bool secure_random_token(std::string *out, size_t bytes) {
  if (!out || bytes < 16 || bytes > 64)
    return false;
  std::array<uint8_t, 64> raw{};
  if (!read_random_bytes(raw.data(), bytes))
    return false;
  static constexpr char digits[] = "0123456789abcdef";
  out->clear();
  out->reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) {
    out->push_back(digits[raw[i] >> 4]);
    out->push_back(digits[raw[i] & 0x0f]);
  }
  return true;
}

bool secure_pairing_code(std::string *out) {
  if (!out)
    return false;
  std::array<uint8_t, 8> raw{};
  if (!read_random_bytes(raw.data(), raw.size()))
    return false;
  uint64_t number = 0;
  for (const uint8_t byte : raw)
    number = (number << 8) | byte;
  number %= 1000000000000ULL;
  char code[13];
  std::snprintf(code, sizeof(code), "%012llu",
                static_cast<unsigned long long>(number));
  *out = code;
  return true;
}

bool constant_time_equal(std::string_view a, std::string_view b) {
  if (a.size() != b.size())
    return false;
  unsigned char difference = 0;
  for (size_t i = 0; i < a.size(); ++i)
    difference |= static_cast<unsigned char>(a[i] ^ b[i]);
  return difference == 0;
}

bool is_private_peer(const sockaddr_in &address) {
  const uint32_t host = ntohl(address.sin_addr.s_addr);
  return (host >> 24) == 10 ||
         (host >> 20) == ((172U << 4) | 1U) ||
         (host >> 16) == ((192U << 8) | 168U) ||
         (host >> 16) == ((169U << 8) | 254U) ||
         (host >> 24) == 127;
}

ssize_t receive_before(int fd, char *buffer, size_t capacity,
                       std::chrono::steady_clock::time_point deadline) {
  if (fd < 0 || fd >= FD_SETSIZE) {
    errno = EINVAL;
    return -1;
  }
  const auto remaining = deadline - std::chrono::steady_clock::now();
  if (remaining <= std::chrono::steady_clock::duration::zero()) {
    errno = ETIMEDOUT;
    return -1;
  }
  const auto micros =
      std::chrono::duration_cast<std::chrono::microseconds>(remaining);
  timeval wait{};
  wait.tv_sec = static_cast<time_t>(micros.count() / 1000000);
  wait.tv_usec = static_cast<suseconds_t>(micros.count() % 1000000);
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(fd, &read_fds);
  const int ready = select(fd + 1, &read_fds, nullptr, nullptr, &wait);
  if (ready == 0) {
    errno = ETIMEDOUT;
    return -1;
  }
  if (ready < 0)
    return -1;
  return recv(fd, buffer, capacity, 0);
}

bool parse_request(int fd, HttpRequest *request) {
  if (!request)
    return false;
  std::string raw;
  raw.reserve(2048);
  size_t header_end = std::string::npos;
  char buffer[1024];
  const auto deadline =
      std::chrono::steady_clock::now() + kRequestLifetime;
  while ((header_end = raw.find("\r\n\r\n")) == std::string::npos) {
    if (raw.size() >= kMaxHeaderBytes)
      return false;
    const ssize_t count = receive_before(fd, buffer, sizeof(buffer), deadline);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return false;
    raw.append(buffer, static_cast<size_t>(count));
    if (raw.size() > kMaxRequestBytes)
      return false;
  }
  if (header_end + 4 > kMaxHeaderBytes)
    return false;

  const size_t first_end = raw.find("\r\n");
  if (first_end == std::string::npos || first_end > header_end)
    return false;
  const std::string first_line = raw.substr(0, first_end);
  const size_t first_space = first_line.find(' ');
  const size_t second_space =
      first_space == std::string::npos
          ? std::string::npos
          : first_line.find(' ', first_space + 1);
  if (first_space == std::string::npos || second_space == std::string::npos ||
      first_line.find(' ', second_space + 1) != std::string::npos ||
      first_line.substr(second_space + 1) != "HTTP/1.1")
    return false;
  request->method = first_line.substr(0, first_space);
  request->target =
      first_line.substr(first_space + 1, second_space - first_space - 1);
  if ((request->method != "GET" && request->method != "POST") ||
      request->target.empty() || request->target.front() != '/' ||
      request->target.find("://") != std::string::npos ||
      request->target.find_first_of(" \t\r\n") != std::string::npos)
    return false;

  size_t line_start = first_end + 2;
  while (line_start < header_end) {
    const size_t line_end = raw.find("\r\n", line_start);
    if (line_end == std::string::npos || line_end > header_end)
      return false;
    const std::string_view line(raw.data() + line_start, line_end - line_start);
    const size_t colon = line.find(':');
    if (colon == std::string_view::npos || colon == 0 ||
        line.front() == ' ' || line.front() == '\t')
      return false;
    std::string name = lower_ascii(std::string(line.substr(0, colon)));
    std::string value = trim(std::string(line.substr(colon + 1)));
    if (name.empty() || value.find_first_of("\r\n") != std::string::npos)
      return false;
    if (std::any_of(request->headers.begin(), request->headers.end(),
                    [&name](const auto &entry) {
                      return entry.first == name;
                    }))
      return false;
    request->headers.emplace_back(std::move(name), std::move(value));
    line_start = line_end + 2;
  }

  const std::string *host = request->header("host");
  if (!host || host->empty() || request->header("transfer-encoding"))
    return false;
  size_t content_length = 0;
  if (const std::string *length = request->header("content-length")) {
    if (length->empty())
      return false;
    for (const char ch : *length) {
      if (!std::isdigit(static_cast<unsigned char>(ch)) ||
          content_length > kMaxBodyBytes)
        return false;
      content_length = content_length * 10 +
                       static_cast<size_t>(ch - '0');
    }
  } else if (request->method == "POST") {
    return false;
  }
  if (content_length > kMaxBodyBytes ||
      (request->method == "GET" && content_length != 0))
    return false;

  const size_t body_start = header_end + 4;
  if (raw.size() - body_start > content_length)
    return false;
  request->body.assign(raw.data() + body_start, raw.size() - body_start);
  while (request->body.size() < content_length) {
    const size_t remaining = content_length - request->body.size();
    const ssize_t count = receive_before(
        fd, buffer, std::min(remaining, sizeof(buffer)), deadline);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return false;
    request->body.append(buffer, static_cast<size_t>(count));
  }
  return true;
}

bool send_all(int fd, std::string_view content) {
  size_t sent = 0;
  while (sent < content.size()) {
    const ssize_t count = send(fd, content.data() + sent,
                               content.size() - sent, 0);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      return false;
    sent += static_cast<size_t>(count);
  }
  return true;
}

bool respond(int fd, int status, std::string_view reason,
             std::string_view type, std::string_view body,
             std::string_view extra_headers = {}) {
  char header[512];
  const int count = std::snprintf(
      header, sizeof(header),
      "HTTP/1.1 %d %.*s\r\n"
      "Content-Type: %.*s\r\n"
      "Content-Length: %zu\r\n"
      "Connection: close\r\n"
      "Cache-Control: no-store\r\n"
      "X-Content-Type-Options: nosniff\r\n"
      "Referrer-Policy: no-referrer\r\n"
      "Content-Security-Policy: default-src 'self'; object-src 'none'; "
      "base-uri 'none'; frame-ancestors 'none'\r\n"
      "%.*s\r\n",
      status, static_cast<int>(reason.size()), reason.data(),
      static_cast<int>(type.size()), type.data(), body.size(),
      static_cast<int>(extra_headers.size()), extra_headers.data());
  if (count < 0 || static_cast<size_t>(count) >= sizeof(header))
    return false;
  return send_all(fd, std::string_view(header, static_cast<size_t>(count))) &&
         send_all(fd, body);
}

bool parse_content_length_json(const HttpRequest &request, cJSON **root) {
  if (!root)
    return false;
  *root = nullptr;
  const std::string *content_type = request.header("content-type");
  if (!content_type)
    return false;
  const size_t separator = content_type->find(';');
  const std::string media_type =
      lower_ascii(trim(content_type->substr(0, separator)));
  if (media_type != "application/json")
    return false;
  const char *parse_end = nullptr;
  cJSON *parsed = cJSON_ParseWithLengthOpts(
      request.body.data(), request.body.size(), &parse_end, false);
  const char *body_end = request.body.data() + request.body.size();
  while (parse_end && parse_end < body_end &&
         std::isspace(static_cast<unsigned char>(*parse_end)))
    ++parse_end;
  if (!parsed || !cJSON_IsObject(parsed) || parse_end != body_end) {
    cJSON_Delete(parsed);
    return false;
  }
  *root = parsed;
  return true;
}

const cJSON *json_string(const cJSON *object, const char *name) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  return cJSON_IsString(item) && item->valuestring ? item : nullptr;
}

bool json_bool(const cJSON *object, const char *name, bool *out) {
  const cJSON *item = cJSON_GetObjectItemCaseSensitive(object, name);
  if (!out || !cJSON_IsBool(item))
    return false;
  *out = cJSON_IsTrue(item);
  return true;
}

bool keys_exactly(const cJSON *object,
                  const std::vector<std::string_view> &allowed) {
  for (const cJSON *item = object->child; item; item = item->next) {
    if (!item->string)
      return false;
    const bool found =
        std::find(allowed.begin(), allowed.end(), item->string) !=
        allowed.end();
    if (!found)
      return false;
    for (const cJSON *other = item->next; other; other = other->next) {
      if (other->string && std::strcmp(item->string, other->string) == 0)
        return false;
    }
  }
  return true;
}

std::string firmware_string() {
  char text[16];
  std::snprintf(text, sizeof(text), "0x%08X",
                g_firmware_version.load(std::memory_order_relaxed));
  return text;
}

std::string services_json() {
  const bool util_ready = onion_ready_is_set(ONION_READY_UTIL);
  std::string out = "[";
  out += "{\"name\":\"Daemon\",\"state\":\"ready\"},";
  out += "{\"name\":\"Toolbox\",\"state\":\"unknown\"},";
  out += "{\"name\":\"Utility\",\"state\":\"";
  out += util_ready ? "ready" : "unknown";
  out += "\"}";
  out += "]";
  return out;
}

std::string status_json() {
  const std::string firmware = firmware_string();
  std::string out = "{\"version\":\"" + json_escape(ONIONHEN_VERSION) + "\",";
  out += "\"firmware\":\"" + json_escape(firmware) + "\",";
  out += "\"firmware_evidence\":\"Detected system version field; this is "
         "not compatibility evidence.\",";
  out += "\"runtime\":{\"daemon\":\"ready\",\"toolbox\":\"unknown\",\"util\":\"";
  out += onion_ready_is_set(ONION_READY_UTIL) ? "ready" : "unknown";
  out += "\"},\"services\":" + services_json() + "}";
  return out;
}

std::string timeline_json() {
  std::lock_guard<std::mutex> lock(g_timeline_mutex);
  std::string out = "{\"events\":[";
  for (size_t i = 0; i < g_timeline_count; ++i) {
    const TimelineEvent &event =
        g_timeline[(g_timeline_start + i) % kTimelineCapacity];
    std::tm tm{};
    gmtime_r(&event.time, &tm);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tm);
    if (i != 0)
      out += ',';
    out += "{\"time\":\"" + std::string(stamp) + "\",";
    out += "\"stage\":\"" + json_escape(event.stage) + "\",";
    out += "\"outcome\":\"" + json_escape(event.outcome) + "\",";
    out += "\"detail\":\"" + json_escape(event.detail) + "\"}";
  }
  out += "]}";
  return out;
}

const char *edge_name(int position) {
  return position == 2 || position == 3 ? "bottom" : "top";
}

const char *align_name(int alignment) {
  switch (alignment) {
  case onion::kOverlayAlignLeft:
    return "left";
  case onion::kOverlayAlignRight:
    return "right";
  default:
    return "center";
  }
}

std::string overlay_json(const onion::Settings &settings) {
  std::string out = "{\"enabled\":";
  out += settings.overlay_enabled ? "true" : "false";
  out += ",\"background\":";
  out += settings.overlay_background ? "true" : "false";
  out += ",\"cpu\":";
  out += settings.overlay_cpu ? "true" : "false";
  out += ",\"gpu\":";
  out += settings.overlay_gpu ? "true" : "false";
  out += ",\"memory\":";
  out += settings.overlay_ram ? "true" : "false";
  out += ",\"fps\":";
  out += settings.overlay_fps ? "true" : "false";
  out += ",\"edge\":\"" + std::string(edge_name(settings.overlay_pos)) + "\"";
  out += ",\"align\":\"" + std::string(align_name(settings.overlay_align)) +
         "\"";
  out += ",\"font_size\":\"";
  out += onion::overlay_font_size_name(settings.overlay_font_size);
  out += "\"}";
  return out;
}

bool ensure_recovery_directory() {
  (void)mkdir("/data/LnkizDen/companion", 0755);
  (void)mkdir(kRecoveryDirectory, 0755);
  struct stat status {};
  return lstat(kRecoveryDirectory, &status) == 0 &&
         S_ISDIR(status.st_mode);
}

bool is_recovery_id(std::string_view id) {
  if (id.empty() || id.size() > 48)
    return false;
  for (const char ch : id) {
    if (!std::isdigit(static_cast<unsigned char>(ch)) &&
        (ch < 'a' || ch > 'f') && ch != '-')
      return false;
  }
  return true;
}

struct RecoveryPoint {
  std::string id;
  std::time_t created = 0;
};

std::vector<RecoveryPoint> recovery_points() {
  std::vector<RecoveryPoint> points;
  if (!ensure_recovery_directory())
    return points;
  DIR *dir = opendir(kRecoveryDirectory);
  if (!dir)
    return points;
  while (dirent *entry = readdir(dir)) {
    const std::string_view filename(entry->d_name);
    constexpr std::string_view prefix = "recovery-";
    constexpr std::string_view suffix = ".ini";
    if (filename.size() <= prefix.size() + suffix.size() ||
        filename.substr(0, prefix.size()) != prefix ||
        filename.substr(filename.size() - suffix.size()) != suffix)
      continue;
    const std::string id(filename.substr(
        prefix.size(), filename.size() - prefix.size() - suffix.size()));
    if (!is_recovery_id(id))
      continue;
    const std::string path = std::string(kRecoveryDirectory) + "/" +
                             std::string(filename);
    struct stat status {};
    if (lstat(path.c_str(), &status) != 0 || !S_ISREG(status.st_mode))
      continue;
    points.push_back({id, status.st_mtime});
  }
  closedir(dir);
  std::sort(points.begin(), points.end(),
            [](const RecoveryPoint &a, const RecoveryPoint &b) {
              return a.created > b.created;
            });
  return points;
}

std::string recovery_json() {
  const std::vector<RecoveryPoint> points = recovery_points();
  std::string out = "[";
  const size_t displayed = std::min(points.size(), kMaxRecoveryPoints);
  for (size_t i = 0; i < displayed; ++i) {
    std::tm tm{};
    gmtime_r(&points[i].created, &tm);
    char stamp[32];
    std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &tm);
    if (i != 0)
      out += ',';
    out += "{\"id\":\"" + json_escape(points[i].id) + "\",";
    out += "\"label\":\"Recovery point\",";
    out += "\"created_at\":\"" + std::string(stamp) + "\"}";
  }
  out += "]";
  return out;
}

bool create_recovery_point() {
  if (!ensure_recovery_directory())
    return false;
  std::vector<RecoveryPoint> existing = recovery_points();
  std::string random_suffix;
  if (!secure_random_token(&random_suffix, 16))
    return false;
  char timestamp[24];
  const auto now = static_cast<unsigned long long>(std::time(nullptr));
  std::snprintf(timestamp, sizeof(timestamp), "%llu", now);
  const std::string id = std::string(timestamp) + "-" + random_suffix;
  const std::string path = std::string(kRecoveryDirectory) + "/recovery-" +
                           id + ".ini";
  if (!onion::settings_save_file(path.c_str(), g_settings.snapshot()))
    return false;
  while (existing.size() >= kMaxRecoveryPoints) {
    const RecoveryPoint &oldest = existing.back();
    const std::string old_path =
        std::string(kRecoveryDirectory) + "/recovery-" + oldest.id + ".ini";
    if (unlink(old_path.c_str()) != 0) {
      (void)unlink(path.c_str());
      return false;
    }
    existing.pop_back();
  }
  return true;
}

bool restore_recovery_point(std::string_view id) {
  if (!is_recovery_id(id) || !ensure_recovery_directory())
    return false;
  const std::string path = std::string(kRecoveryDirectory) + "/recovery-" +
                           std::string(id) + ".ini";
  onion::Settings restored{};
  if (!onion::settings_load_backup_file(path.c_str(), &restored))
    return false;
  if (!PersistSettings([&restored](onion::Settings &current) {
        current = restored;
      }))
    return false;
  return LoadSettings(/*force=*/true);
}

bool apply_overlay_request(const cJSON *root) {
  const std::vector<std::string_view> allowed = {
      "enabled", "background", "cpu", "gpu", "memory", "fps", "edge",
      "align", "font_size"};
  if (!keys_exactly(root, allowed))
    return false;
  bool enabled = false;
  bool background = false;
  bool cpu = false;
  bool gpu = false;
  bool memory = false;
  bool fps = false;
  if (!json_bool(root, "enabled", &enabled) ||
      !json_bool(root, "background", &background) ||
      !json_bool(root, "cpu", &cpu) || !json_bool(root, "gpu", &gpu) ||
      !json_bool(root, "memory", &memory) ||
      !json_bool(root, "fps", &fps))
    return false;
  const cJSON *edge = json_string(root, "edge");
  const cJSON *align = json_string(root, "align");
  const cJSON *font = json_string(root, "font_size");
  if (!edge || !align || !font)
    return false;
  const std::string edge_value(edge->valuestring);
  int overlay_pos = g_settings.snapshot().overlay_pos;
  if (edge_value == "top")
    overlay_pos = overlay_pos == 1 ? 1 : 0;
  else if (edge_value == "bottom")
    overlay_pos = overlay_pos == 3 ? 3 : 2;
  else
    return false;
  int overlay_align = onion::kOverlayAlignCenter;
  const std::string align_value(align->valuestring);
  if (align_value == "left")
    overlay_align = onion::kOverlayAlignLeft;
  else if (align_value == "center")
    overlay_align = onion::kOverlayAlignCenter;
  else if (align_value == "right")
    overlay_align = onion::kOverlayAlignRight;
  else
    return false;
  int overlay_font_size = onion::kOverlayFontMedium;
  const std::string font_value(font->valuestring);
  if (font_value == "small")
    overlay_font_size = onion::kOverlayFontSmall;
  else if (font_value == "medium")
    overlay_font_size = onion::kOverlayFontMedium;
  else if (font_value == "large")
    overlay_font_size = onion::kOverlayFontLarge;
  else
    return false;
  if (!PersistSettings([=](onion::Settings &settings) {
        settings.overlay_enabled = enabled;
        settings.overlay_background = background;
        settings.overlay_cpu = cpu;
        settings.overlay_gpu = gpu;
        settings.overlay_ram = memory;
        settings.overlay_fps = fps;
        settings.overlay_pos = overlay_pos;
        settings.overlay_align = overlay_align;
        settings.overlay_font_size = overlay_font_size;
      }))
    return false;
  return LoadSettings(/*force=*/true);
}

bool authenticated(const HttpRequest &request, const std::string &peer_ip,
                   PairingState *pairing) {
  if (!pairing)
    return false;
  const std::string *cookie = request.header("cookie");
  if (!cookie)
    return false;
  constexpr std::string_view prefix = "LnkizDenSession=";
  size_t start = 0;
  while (start < cookie->size()) {
    const size_t end = cookie->find(';', start);
    const std::string item =
        trim(cookie->substr(start, end == std::string::npos
                                      ? std::string::npos
                                      : end - start));
    if (item.compare(0, prefix.size(), prefix) == 0) {
      const std::string_view token(item.data() + prefix.size(),
                                   item.size() - prefix.size());
      const auto now = std::chrono::steady_clock::now();
      if (!pairing->session.token.empty() &&
          constant_time_equal(token, pairing->session.token) &&
          peer_ip == pairing->session.peer_ip &&
          now - pairing->session.last_used < kSessionLifetime) {
        pairing->session.last_used = now;
        return true;
      }
      return false;
    }
    if (end == std::string::npos)
      break;
    start = end + 1;
  }
  return false;
}

bool handle_pair(int fd, const HttpRequest &request, const std::string &peer_ip,
                 const std::string &origin, PairingState *pairing) {
  cJSON *root = nullptr;
  if (!parse_content_length_json(request, &root))
    return respond(fd, 400, "Bad Request", "text/plain", "Invalid JSON");
  const std::vector<std::string_view> allowed = {"code"};
  const cJSON *code = json_string(root, "code");
  const auto now = std::chrono::steady_clock::now();
  const auto *origin_header = request.header("origin");
  const bool same_origin = origin_header && *origin_header == origin;
  const bool in_window =
      pairing->attempt_window.time_since_epoch().count() != 0 &&
      now - pairing->attempt_window < std::chrono::minutes(1);
  if (!in_window) {
    pairing->attempt_window = now;
    pairing->failed_attempts = 0;
  }
  if (!same_origin || !keys_exactly(root, allowed) || !code ||
      pairing->code.empty() ||
      now - pairing->code_created >= std::chrono::minutes(10) ||
      pairing->blocked_until > now ||
      !constant_time_equal(code->valuestring, pairing->code)) {
    if (++pairing->failed_attempts >= 5) {
      pairing->blocked_until = now + std::chrono::minutes(1);
      pairing->failed_attempts = 0;
    }
    cJSON_Delete(root);
    return respond(fd, 401, "Unauthorized", "text/plain",
                   "Pairing code invalid or temporarily rate-limited");
  }
  cJSON_Delete(root);
  std::string token;
  if (!secure_random_token(&token, 32)) {
    return respond(fd, 503, "Service Unavailable", "text/plain",
                   "Secure session generation failed");
  }
  pairing->code.clear();
  pairing->failed_attempts = 0;
  pairing->session.token = std::move(token);
  pairing->session.peer_ip = peer_ip;
  pairing->session.last_used = now;
  const std::string headers =
      "Set-Cookie: LnkizDenSession=" + pairing->session.token +
      "; HttpOnly; SameSite=Strict; Path=/\r\n";
  (void)origin;
  return respond(fd, 200, "OK", "application/json", "{\"paired\":true}",
                 headers);
}

bool handle_api(int fd, const HttpRequest &request, const std::string &peer_ip,
                PairingState *pairing, const std::string &origin) {
  const std::string path = request.target;
  if (path == "/api/v1/pair" && request.method == "POST")
    return handle_pair(fd, request, peer_ip, origin, pairing);
  if (!authenticated(request, peer_ip, pairing))
    return respond(fd, 401, "Unauthorized", "text/plain", "Pairing required");

  if (request.method == "GET" && path == "/api/v1/status")
    return respond(fd, 200, "OK", "application/json", status_json());
  if (request.method == "GET" && path == "/api/v1/compatibility")
    return respond(fd, 200, "OK", "application/json",
                   "{\"records\":[]}");
  if (request.method == "GET" && path == "/api/v1/timeline")
    return respond(fd, 200, "OK", "application/json", timeline_json());
  if (request.method == "GET" && path == "/api/v1/cheats/conflicts")
    return respond(fd, 200, "OK", "application/json",
                   "{\"available\":false,\"findings\":[]}");
  if (request.method == "GET" && path == "/api/v1/settings") {
    const onion::Settings settings = g_settings.snapshot();
    const std::string body =
        "{\"overlay\":" + overlay_json(settings) +
        ",\"recovery_points\":" + recovery_json() + "}";
    return respond(fd, 200, "OK", "application/json", body);
  }
  if (request.method == "GET" && path == "/api/v1/diagnostics") {
    const std::string body =
        "{\"firmware\":\"" + json_escape(firmware_string()) +
        "\",\"firmware_evidence\":\"Detected system version field; "
        "not compatibility evidence.\",\"services\":" + services_json() +
        ",\"timeline\":" + timeline_json() + "}";
    return respond(fd, 200, "OK", "application/json", body);
  }
  if (request.method == "POST" && path == "/api/v1/settings/overlay") {
    cJSON *root = nullptr;
    if (!parse_content_length_json(request, &root))
      return respond(fd, 400, "Bad Request", "text/plain", "Invalid JSON");
    const bool saved = apply_overlay_request(root);
    cJSON_Delete(root);
    if (!saved)
      return respond(fd, 400, "Bad Request", "text/plain",
                     "Overlay settings were invalid or could not be saved");
    return respond(fd, 200, "OK", "application/json", "{\"saved\":true}");
  }
  if (request.method == "POST" && path == "/api/v1/recovery/points") {
    cJSON *root = nullptr;
    if (!parse_content_length_json(request, &root) ||
        !keys_exactly(root, {})) {
      cJSON_Delete(root);
      return respond(fd, 400, "Bad Request", "text/plain", "Invalid request");
    }
    cJSON_Delete(root);
    if (!create_recovery_point())
      return respond(fd, 500, "Internal Server Error", "text/plain",
                     "Recovery point could not be created");
    return respond(fd, 201, "Created", "application/json",
                   "{\"created\":true}");
  }
  if (request.method == "POST" && path == "/api/v1/recovery/restore") {
    cJSON *root = nullptr;
    if (!parse_content_length_json(request, &root)) {
      return respond(fd, 400, "Bad Request", "text/plain", "Invalid JSON");
    }
    const std::vector<std::string_view> allowed = {"id"};
    const cJSON *id = json_string(root, "id");
    const bool shape_ok = keys_exactly(root, allowed) && id;
    const std::string id_value = shape_ok ? id->valuestring : "";
    cJSON_Delete(root);
    if (!shape_ok || !restore_recovery_point(id_value))
      return respond(fd, 400, "Bad Request", "text/plain",
                     "Recovery point is invalid or could not be restored");
    return respond(fd, 200, "OK", "application/json", "{\"restored\":true}");
  }
  return respond(fd, 404, "Not Found", "text/plain", "Not found");
}

bool serve_static(int fd, const std::string &target) {
  const char *asset = nullptr;
  const char *end = nullptr;
  const char *type = nullptr;
  if (target == "/") {
    asset = companion_index_start;
    end = companion_index_end;
    type = "text/html; charset=utf-8";
  } else if (target == "/companion.css") {
    asset = companion_css_start;
    end = companion_css_end;
    type = "text/css; charset=utf-8";
  } else if (target == "/companion.js") {
    asset = companion_js_start;
    end = companion_js_end;
    type = "application/javascript; charset=utf-8";
  } else {
    return respond(fd, 404, "Not Found", "text/plain", "Not found");
  }
  if (!asset || !end || end < asset)
    return respond(fd, 500, "Internal Server Error", "text/plain",
                   "Dashboard assets are unavailable");
  return respond(fd, 200, "OK", type,
                 std::string_view(asset, static_cast<size_t>(end - asset)));
}

void serve_client(int fd, const sockaddr_in &peer,
                  const std::string &expected_host,
                  const std::string &expected_origin,
                  PairingState *pairing) {
  timeval timeout{2, 0};
  (void)setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  (void)setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
  int no_sigpipe = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe,
                   sizeof(no_sigpipe));
  if (!is_private_peer(peer))
    return (void)respond(fd, 403, "Forbidden", "text/plain", "LAN only");
  HttpRequest request;
  if (!parse_request(fd, &request))
    return (void)respond(fd, 400, "Bad Request", "text/plain",
                         "Invalid or oversized request");
  if (request.method == "GET" && request.target.front() != '/' &&
      request.target.find("/api/") != 0)
    return (void)respond(fd, 400, "Bad Request", "text/plain", "Invalid path");

  const std::string *request_host = request.header("host");
  if (!request_host || *request_host != expected_host)
    return (void)respond(fd, 400, "Bad Request", "text/plain", "Invalid Host");
  const bool is_api = request.target.find("/api/") == 0;
  if (is_api && request.method == "POST") {
    const std::string *origin = request.header("origin");
    if (!origin || *origin != expected_origin)
      return (void)respond(fd, 403, "Forbidden", "text/plain",
                           "Same-origin request required");
  }
  if (is_api) {
    char peer_ip_buffer[INET_ADDRSTRLEN]{};
    if (!inet_ntop(AF_INET, &peer.sin_addr, peer_ip_buffer,
                   sizeof(peer_ip_buffer)))
      return (void)respond(fd, 400, "Bad Request", "text/plain",
                           "Invalid peer address");
    const std::string peer_ip(peer_ip_buffer);
    (void)handle_api(fd, request, peer_ip, pairing, expected_origin);
    return;
  }
  if (request.method != "GET")
    return (void)respond(fd, 405, "Method Not Allowed", "text/plain",
                         "Method not allowed");
  (void)serve_static(fd, request.target);
}

int start_listener(const std::string &ip, std::string *host,
                   std::string *origin) {
  if (ip.empty() || ip == ONION_NET_IP_UNKNOWN)
    return -1;
  in_addr parsed{};
  if (inet_pton(AF_INET, ip.c_str(), &parsed) != 1)
    return -1;
  const int fd = socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0 || fd >= FD_SETSIZE) {
    if (fd >= 0)
      close(fd);
    return -1;
  }
  int reuse = 1;
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in address{};
  address.sin_len = sizeof(address);
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = htonl(INADDR_ANY);
  address.sin_port = htons(kPort);
  if (bind(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) != 0 ||
      listen(fd, 4) != 0) {
    close(fd);
    return -1;
  }
  *host = ip + ":" + std::to_string(kPort);
  *origin = "http://" + *host;
  return fd;
}

} // namespace

void companion_dashboard_set_firmware(uint32_t version) noexcept {
  g_firmware_version.store(version, std::memory_order_relaxed);
}

void companion_dashboard_record_event(const char *stage, const char *outcome,
                                      const char *detail) noexcept {
  std::lock_guard<std::mutex> lock(g_timeline_mutex);
  const size_t index =
      (g_timeline_start + g_timeline_count) % kTimelineCapacity;
  TimelineEvent event;
  event.time = std::time(nullptr);
  event.stage = stage ? std::string(stage).substr(0, 80) : "Runtime";
  event.outcome = outcome ? std::string(outcome).substr(0, 16) : "unknown";
  event.detail = detail ? std::string(detail).substr(0, 160) : "";
  if (g_timeline_count == kTimelineCapacity) {
    g_timeline_start = (g_timeline_start + 1) % kTimelineCapacity;
  } else {
    ++g_timeline_count;
  }
  g_timeline[index] = std::move(event);
}

void *companion_dashboard_thread(void *args) noexcept {
  (void)args;
  PairingState pairing;
  std::string console_ip;
  std::string expected_host;
  std::string expected_origin;
  int listener = -1;
  while (!g_stack_shutting_down.load(std::memory_order_acquire)) {
    const bool enabled = g_settings.snapshot().companion_dashboard_enabled;
    if (!enabled) {
      if (listener >= 0) {
        close(listener);
        listener = -1;
        g_listener_ready.store(false, std::memory_order_release);
      }
      pairing = {};
      usleep(500 * 1000);
      continue;
    }

    if (listener < 0) {
      char ip[ONION_NET_IP_ADDRESS_SIZE]{};
      if (onion_net_get_ip_address(ip, sizeof(ip)) != ONION_NET_IP_OK) {
        LOG_WARN("[companion] Dashboard enabled but network IP is unavailable");
        usleep(1000 * 1000);
        continue;
      }
      if (!secure_pairing_code(&pairing.code)) {
        LOG_ERROR("[companion] Secure pairing code generation failed");
        usleep(1000 * 1000);
        continue;
      }
      pairing.code_created = std::chrono::steady_clock::now();
      listener = start_listener(ip, &expected_host, &expected_origin);
      if (listener < 0) {
        pairing.code.clear();
        LOG_ERROR("[companion] Could not bind local dashboard port %u",
                  static_cast<unsigned>(kPort));
        usleep(3000 * 1000);
        continue;
      }
      console_ip = ip;
      pairing.session = {};
      pairing.failed_attempts = 0;
      pairing.attempt_window = {};
      pairing.blocked_until = {};
      g_listener_ready.store(true, std::memory_order_release);
      LOG_INFO("[companion] Dashboard listening at http://%s", expected_host.c_str());
      onion_notify(1, "Open http://%s and enter code %s", expected_host.c_str(),
                   pairing.code.c_str());
      companion_dashboard_record_event(
          "Companion dashboard", "ready",
          "Paired local dashboard is listening on TCP 9049");
    }

    const auto now = std::chrono::steady_clock::now();
    if (!pairing.session.token.empty() &&
        now - pairing.session.last_used >= kSessionLifetime)
      pairing.session = {};
    if (pairing.code.empty() && pairing.session.token.empty()) {
      if (secure_pairing_code(&pairing.code)) {
        pairing.code_created = std::chrono::steady_clock::now();
        onion_notify(1, "Open http://%s and enter code %s",
                     expected_host.c_str(), pairing.code.c_str());
      } else {
        LOG_ERROR("[companion] Pairing code generation failed");
        close(listener);
        listener = -1;
        g_listener_ready.store(false, std::memory_order_release);
        continue;
      }
    }
    if (!pairing.code.empty() &&
        now - pairing.code_created >= std::chrono::minutes(10)) {
      pairing.code.clear();
      if (secure_pairing_code(&pairing.code)) {
        pairing.code_created = std::chrono::steady_clock::now();
        onion_notify(1, "Open http://%s and enter code %s",
                     expected_host.c_str(), pairing.code.c_str());
      } else {
        LOG_ERROR("[companion] Pairing code refresh failed");
        close(listener);
        listener = -1;
        g_listener_ready.store(false, std::memory_order_release);
        continue;
      }
    }

    char current_ip[ONION_NET_IP_ADDRESS_SIZE]{};
    if (onion_net_get_ip_address(current_ip, sizeof(current_ip)) ==
            ONION_NET_IP_OK &&
        console_ip != current_ip) {
      close(listener);
      listener = -1;
      g_listener_ready.store(false, std::memory_order_release);
      pairing = {};
      continue;
    }

    fd_set read_fds;
    FD_ZERO(&read_fds);
    FD_SET(listener, &read_fds);
    timeval wait{1, 0};
    const int ready = select(listener + 1, &read_fds, nullptr, nullptr, &wait);
    if (ready < 0 && errno != EINTR) {
      close(listener);
      listener = -1;
      g_listener_ready.store(false, std::memory_order_release);
      LOG_ERROR("[companion] Listener select failed: %d", errno);
      continue;
    }
    if (ready <= 0)
      continue;

    sockaddr_in peer{};
    socklen_t peer_size = sizeof(peer);
    const int client = accept(listener, reinterpret_cast<sockaddr *>(&peer),
                              &peer_size);
    if (client < 0) {
      if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)
        LOG_WARN("[companion] Client accept failed: %d", errno);
      continue;
    }
    serve_client(client, peer, expected_host, expected_origin, &pairing);
    close(client);

    const onion::Settings refreshed = g_settings.snapshot();
    if (!refreshed.companion_dashboard_enabled) {
      close(listener);
      listener = -1;
      g_listener_ready.store(false, std::memory_order_release);
      pairing = {};
    }
  }
  if (listener >= 0)
    close(listener);
  g_listener_ready.store(false, std::memory_order_release);
  return nullptr;
}
