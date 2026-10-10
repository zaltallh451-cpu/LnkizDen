/* Copyright (C) 2025 OnionHEN / LightningMods
 *
 * Strict user Payload ELF loader (PID files + private :9020 elfldr).
 */

#include <onion/payload.h>
#include <onion/elf_name.h>

#include <elfldr_remote.h>
#include <onion/fs.h>
#include <onion/log.h>
#include <onion/notify.h>
#include <onion/proc_query.h>
#include <onion/system_tmp.h>

#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

extern int sceKernelGetProcessName(int pid, char *name);

bool onion_payload_is_elf(const void *buf, size_t size) {
  if (!buf || size < 4)
    return false;
  static const unsigned char kMagic[] = {0x7F, 'E', 'L', 'F'};
  return memcmp(buf, kMagic, 4) == 0;
}

bool onion_payload_elf_key_from_name(const char *name, char *out, size_t out_sz) {
  if (!name || !out || out_sz < 2)
    return false;

  const char *base = strrchr(name, '/');
  base = base ? base + 1 : name;
  if (!base[0] || strcmp(base, ".") == 0 || strcmp(base, "..") == 0)
    return false;

  size_t n = onion_elf_name_stem_n(base, strlen(base));
  if (n == 0)
    return false; /* bare ".elf" */
  if (n >= out_sz)
    n = out_sz - 1;

  memcpy(out, base, n);
  out[n] = '\0';
  if (out[0] == '\0' || strchr(out, '/') != NULL || strchr(out, '\\') != NULL)
    return false;
  return true;
}

void onion_payload_pid_path(char *out, size_t out_sz, const char *title_id) {
  (void)onion_system_tmp_payload_pid_path(out, out_sz, title_id);
}

static bool valid_payload_key(const char *title_id) {
  if (!title_id || !title_id[0] || strcmp(title_id, ".") == 0 ||
      strcmp(title_id, "..") == 0 || strlen(title_id) >= 64) {
    return false;
  }
  for (const unsigned char *cursor =
           (const unsigned char *)title_id; *cursor; ++cursor) {
    if (*cursor < 0x20 || *cursor == 0x7f || *cursor == '/' ||
        *cursor == '\\') {
      return false;
    }
  }
  return true;
}

static bool valid_payload_pid_path(const char *pid_path) {
  if (!pid_path) {
    return false;
  }
  const size_t root_length = strlen(ONION_SYSTEM_TMP_PAYLOAD_PID_ROOT);
  if (strncmp(pid_path, ONION_SYSTEM_TMP_PAYLOAD_PID_ROOT, root_length) != 0 ||
      pid_path[root_length] != '/') {
    return false;
  }

  const char *key = pid_path + root_length + 1;
  const char *suffix = strrchr(key, '.');
  if (!suffix || strcmp(suffix, ".PID") != 0 || suffix == key ||
      (size_t)(suffix - key) >= 64) {
    return false;
  }
  char key_copy[64];
  const size_t key_length = (size_t)(suffix - key);
  memcpy(key_copy, key, key_length);
  key_copy[key_length] = '\0';
  return valid_payload_key(key_copy);
}

pid_t onion_payload_read_pid_file(const char *pid_path) {
  const int f = open(pid_path, O_RDONLY);
  if (f < 0)
    return -1;
  char t[32];
  const int r = (int)read(f, t, sizeof(t) - 1);
  close(f);
  if (r <= 0)
    return -1;
  t[r] = '\0';
  return (pid_t)atoi(t);
}

uint64_t onion_payload_read_pid_generation(const char *pid_path) {
  if (!pid_path) {
    return 0;
  }
  const int f = open(pid_path, O_RDONLY);
  if (f < 0) {
    return 0;
  }
  char t[64];
  const int r = (int)read(f, t, sizeof(t) - 1);
  close(f);
  if (r <= 0) {
    return 0;
  }
  t[r] = '\0';
  char *end = NULL;
  errno = 0;
  const long pid = strtol(t, &end, 10);
  if (errno == ERANGE || pid <= 1 || pid > INT_MAX || !end ||
      *end != '\n') {
    return 0;
  }
  errno = 0;
  const unsigned long long generation = strtoull(end + 1, &end, 10);
  if (errno == ERANGE || !end || (*end != '\0' && *end != '\n') ||
      generation == 0) {
    return 0;
  }
  return (uint64_t)generation;
}

bool onion_payload_write_pid_file(const char *pid_path, pid_t pid) {
  if (!valid_payload_pid_path(pid_path)) {
    LOG_ERROR("Refusing invalid payload PID path: %s",
              pid_path ? pid_path : "(null)");
    return false;
  }
  if (pid <= 1) {
    return unlink(pid_path) == 0 || errno == ENOENT;
  }

  mkdir(ONION_SYSTEM_TMP_ROOT, 0777);
  if (mkdir(ONION_SYSTEM_TMP_PAYLOAD_PID_ROOT, 0700) != 0 &&
      errno != EEXIST) {
    LOG_ERROR("Unable to create protected payload PID directory: %s",
              strerror(errno));
    return false;
  }
  struct stat directory_stat;
  if (lstat(ONION_SYSTEM_TMP_PAYLOAD_PID_ROOT, &directory_stat) != 0 ||
      !S_ISDIR(directory_stat.st_mode) ||
      directory_stat.st_uid != geteuid() ||
      chmod(ONION_SYSTEM_TMP_PAYLOAD_PID_ROOT, 0700) != 0) {
    LOG_ERROR("Refusing unsafe payload PID directory");
    return false;
  }
  const int f = open(pid_path, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW,
                     0600);
  if (f < 0) {
    LOG_ERROR("Unable to open protected payload PID file %s: %s", pid_path,
              strerror(errno));
    return false;
  }
  struct stat file_stat;
  if (fstat(f, &file_stat) != 0 || !S_ISREG(file_stat.st_mode) ||
      file_stat.st_uid != geteuid() || fchmod(f, 0600) != 0) {
    LOG_ERROR("Refusing unsafe payload PID file: %s", pid_path);
    close(f);
    unlink(pid_path);
    return false;
  }
  const uint64_t generation = onion_proc_process_generation(pid);
  if (generation == 0) {
    LOG_ERROR("Unable to verify process generation for payload pid=%d",
              (int)pid);
    close(f);
    unlink(pid_path);
    return false;
  }
  char t[64];
  const int len = snprintf(t, sizeof(t), "%d\n%llu", pid,
                           (unsigned long long)generation);
  if (len <= 0 || (size_t)len >= sizeof(t)) {
    close(f);
    unlink(pid_path);
    return false;
  }
  const ssize_t written = write(f, t, (size_t)len);
  const int close_result = close(f);
  if (written != (ssize_t)len) {
    LOG_ERROR("Incomplete write for payload PID file: %s", pid_path);
    unlink(pid_path);
    return false;
  }
  if (close_result != 0) {
    LOG_ERROR("Unable to close payload PID file: %s", pid_path);
    unlink(pid_path);
    return false;
  }
  return true;
}

bool onion_payload_running(const char *title_id) {
  if (!title_id || !title_id[0])
    return false;
  char path[256];
  onion_payload_pid_path(path, sizeof(path), title_id);
  pid_t pid = onion_payload_read_pid_file(path);
  bool legacy_record = false;
  if (pid <= 1) {
    if (pid == 1)
      unlink(path);
    if (!onion_system_tmp_pid_path(path, sizeof(path), title_id))
      return false;
    pid = onion_payload_read_pid_file(path);
    legacy_record = true;
  }
  if (pid <= 1) {
    if (pid == 1) {
      LOG_WARN("Ignoring bogus payload PID 1 for %s", title_id);
      unlink(path);
    }
    return false;
  }
  if (!onion_proc_is_alive(pid)) {
    LOG_WARN("Stale payload PID file for %s (pid=%d dead), removing",
             title_id, (int)pid);
    unlink(path);
    return false;
  }
  const uint64_t generation =
      legacy_record ? 0 : onion_payload_read_pid_generation(path);
  if (generation != 0) {
    const uint64_t current_generation = onion_proc_process_generation(pid);
    if (current_generation != 0 && current_generation != generation) {
      LOG_WARN("Stale payload PID file for %s (pid=%d reused), removing",
               title_id, (int)pid);
      unlink(path);
      return false;
    }
  }
  return true;
}

static pid_t launch_user_payload(const char *title_id, const char *abs_path) {
  if (!elfldr_remote_onion_available()) {
    LOG_ERROR("Private elfldr :%u unavailable; launch failed for %s",
              ONION_ELFLDR_PORT, title_id);
    return -1;
  }

  const pid_t pid = elfldr_remote_onion_launch_file_get_pid(abs_path, NULL);
  if (pid <= 1) {
    LOG_ERROR("Private elfldr :%u returned no valid PID for %s (pid=%d)",
              ONION_ELFLDR_PORT, title_id, (int)pid);
    return -1;
  }

  char pname[32] = {0};
  if (sceKernelGetProcessName(pid, pname) == 0)
    LOG_INFO("Launched via %u (pid=%d name=%s)", ONION_ELFLDR_PORT, (int)pid,
             pname);
  else
    LOG_INFO("Launched via %u (pid=%d)", ONION_ELFLDR_PORT, (int)pid);
  return pid;
}

pid_t onion_payload_launch_elfldr(const char *title_id, const char *path) {
  if (!valid_payload_key(title_id) || !path || path[0] != '/') {
    LOG_ERROR("launch_elfldr: invalid args title=%s path=%s",
              title_id ? title_id : "(null)", path ? path : "(null)");
    return -1;
  }

  LOG_INFO("loading payload via elfldr key=%s path=%s", title_id, path);
  return launch_user_payload(title_id, path);
}

uint8_t *onion_payload_read_file(const char *path, size_t *out_size) {
  uint8_t *buf = NULL;
  size_t size = 0;

  if (!path || !out_size)
    return NULL;

  if (!read_file_alloc(path, 0, &buf, &size)) {
    LOG_ERROR("Failed to read payload file %s", path);
    return NULL;
  }

  *out_size = size;
  return buf;
}

bool onion_payload_load_with_key(const char *path, const char *filename,
                                 const char *requested_key) {
  size_t size = 0;
  uint8_t *buf = onion_payload_read_file(path, &size);
  if (!buf)
    return false;

  char name_buf[256];
  const char *base = filename;
  if (!base || !base[0]) {
    snprintf(name_buf, sizeof(name_buf), "%s", path);
    base = basename(name_buf);
  }

  const size_t base_len = strlen(base);
  if (!onion_elf_name_has_suffix(base, base_len) ||
      onion_elf_name_stem_n(base, base_len) == 0) {
    LOG_WARN("Not a .elf payload: %s", base);
    onion_notify(1, "notify.payload.elf_only", base);
    free(buf);
    return false;
  }

  if (!onion_payload_is_elf(buf, size)) {
    LOG_ERROR("Invalid ELF file: %s", base);
    onion_notify(1, "notify.payload.invalid_elf", base);
    free(buf);
    return false;
  }

  char key[64];
  if (requested_key && valid_payload_key(requested_key)) {
    snprintf(key, sizeof(key), "%s", requested_key);
  } else if (!onion_payload_elf_key_from_name(base, key, sizeof(key))) {
    LOG_ERROR("Invalid ELF basename (empty stem): %s", base);
    onion_notify(1, "notify.payload.invalid_name", base);
    free(buf);
    return false;
  }

  LOG_DEBUG("payload launch key=%s (from %s)", key, base);
  char pid_path[256];
  onion_payload_pid_path(pid_path, sizeof(pid_path), key);

  if (onion_payload_running(key)) {
    LOG_INFO("Payload %s is already running; keeping the recorded instance",
             key);
    free(buf);
    return true;
  }

  if (!elfldr_remote_onion_available()) {
    LOG_WARN("Private elfldr :%u unavailable; payload %s not started",
             ONION_ELFLDR_PORT, key);
    free(buf);
    return false;
  }

  const pid_t pid = onion_payload_launch_elfldr(key, path);
  free(buf);
  /* Only persist real pids; never write 0/1 (PID 1 would hit system init). */
  if (pid <= 1) {
    onion_payload_write_pid_file(pid_path, -1);
    return false;
  }
  if (!onion_payload_write_pid_file(pid_path, pid)) {
    LOG_ERROR("Payload %s launched as pid=%d but its safe runtime record "
              "could not be persisted",
              key, (int)pid);
    return false;
  }
  return true;
}

bool onion_payload_load(const char *path, const char *filename) {
  return onion_payload_load_with_key(path, filename, NULL);
}
