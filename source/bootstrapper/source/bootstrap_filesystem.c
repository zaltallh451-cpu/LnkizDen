/* Copyright (C) 2026 OnionHEN / LightningMods */

#include "bootstrap_filesystem.h"

#include "bootstrap_notify.h"

#include <onion/log.h>

#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/_iovec.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct BootstrapIovec {
  const void *iov_base;
  size_t iov_length;
} BootstrapIovec;

#define BUILD_IOVEC(value)                                                     \
  { .iov_base = (value), .iov_length = __builtin_strlen(value) + 1 }

static bool remount(const char *device, const char *path) {
  BootstrapIovec iov[] = {
      BUILD_IOVEC("fstype"),    BUILD_IOVEC("exfatfs"),
      BUILD_IOVEC("fspath"),    BUILD_IOVEC(path),
      BUILD_IOVEC("from"),      BUILD_IOVEC(device),
      BUILD_IOVEC("large"),     BUILD_IOVEC("yes"),
      BUILD_IOVEC("timezone"),  BUILD_IOVEC("static"),
      BUILD_IOVEC("async"),     {NULL, 0},
      BUILD_IOVEC("ignoreacl"), {NULL, 0},
  };
  return nmount((struct iovec *)iov, sizeof(iov) / sizeof(iov[0]),
                MNT_UPDATE) == 0;
}

static bool ensure_directory(const char *path) {
  if (mkdir(path, 0777) == 0)
    return true;
  if (errno != EEXIST) {
    LOG_ERROR("could not create directory %s: %s", path, strerror(errno));
    return false;
  }

  struct stat status;
  if (lstat(path, &status) != 0 || !S_ISDIR(status.st_mode)) {
    LOG_ERROR("expected a directory at %s", path);
    return false;
  }
  return true;
}

bool bootstrap_filesystem_create_directories(void) {
  return ensure_directory("/data/LnkizDen") &&
         ensure_directory("/data/LnkizDen/payloads") &&
         ensure_directory("/data/LnkizDen/plugins") &&
         ensure_directory("/data/LnkizDen/assets") &&
         ensure_directory("/data/LnkizDen/games") &&
         ensure_directory("/user/data") &&
         ensure_directory("/user/data/LnkizDen");
}

static bool directory_state(const char *path, bool *exists) {
  struct stat status;
  if (lstat(path, &status) == 0) {
    if (!S_ISDIR(status.st_mode)) {
      LOG_ERROR("data migration refused: %s is not a directory", path);
      return false;
    }
    *exists = true;
    return true;
  }
  if (errno == ENOENT) {
    *exists = false;
    return true;
  }
  LOG_ERROR("data migration could not inspect %s: %s", path, strerror(errno));
  return false;
}

bool bootstrap_filesystem_migrate_data(void) {
  const char *legacy_data = "/data/OnionHEN";
  const char *current_data = "/data/LnkizDen";
  const char *legacy_shellui = "/user/data/OnionHEN";
  const char *current_shellui = "/user/data/LnkizDen";
  bool has_legacy_data = false;
  bool has_current_data = false;
  bool has_legacy_shellui = false;
  bool has_current_shellui = false;

  if (!directory_state(legacy_data, &has_legacy_data) ||
      !directory_state(current_data, &has_current_data) ||
      !directory_state(legacy_shellui, &has_legacy_shellui) ||
      !directory_state(current_shellui, &has_current_shellui)) {
    return false;
  }

  if ((has_legacy_data && has_current_data) ||
      (has_legacy_shellui && has_current_shellui)) {
    LOG_ERROR("data migration refused: both legacy and LnkizDen directories "
              "exist; preserve both and resolve the conflict manually");
    return false;
  }

  bool moved_data = false;
  if (has_legacy_data) {
    if (rename(legacy_data, current_data) != 0) {
      LOG_ERROR("data migration could not rename %s to %s: %s", legacy_data,
                current_data, strerror(errno));
      return false;
    }
    moved_data = true;
  }

  if (has_legacy_shellui &&
      rename(legacy_shellui, current_shellui) != 0) {
    const int migration_error = errno;
    LOG_ERROR("data migration could not rename %s to %s: %s",
              legacy_shellui, current_shellui, strerror(migration_error));
    if (moved_data && rename(current_data, legacy_data) != 0) {
      LOG_ERROR("data migration rollback failed; keep both directories "
                "untouched and recover from %s and %s",
                current_data, legacy_shellui);
    }
    return false;
  }

  if (moved_data || has_legacy_shellui) {
    LOG_INFO("migrated legacy OnionHEN data directories to LnkizDen; "
             "source contents were preserved by same-filesystem rename");
  }
  return true;
}

bool bootstrap_filesystem_mount_system(void) {
  LOG_DEBUG("Remounting system partitions ...");
  if (!remount("/dev/ssd0.system_ex", "/system_ex")) {
    perror("failed to mount /system_ex\nif you see this reboot");
    bootstrap_notify("notify.mount.system_ex");
    return false;
  }
  if (!remount("/dev/ssd0.system", "/system")) {
    perror("failed to mount /system_\nif you see this reboot");
    bootstrap_notify("notify.mount.system");
    return false;
  }
  LOG_DEBUG("   Success!");
  return true;
}

void bootstrap_filesystem_disable_updates(void) {
  LOG_DEBUG("Unmounting /update forcefully ...");
  unlink("/update/PS5UPDATE.PUP");
  unlink("/update/PS5UPDATE.PUP.net.temp");
  if (unmount("/update", 0x80000LL) < 0)
    (void)unmount("/update", 0);
  LOG_DEBUG("   Success!");
}
