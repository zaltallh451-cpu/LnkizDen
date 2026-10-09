/* Copyright (C) 2025 OnionHEN / LightningMods — OnPress misc (kstuff, account, credits) */
#include "onpress.hpp"
#include "account_activator.h"
#include "progress_dialog.hpp"
#include "onion_cjson.hpp"

#include <onion/fs.h>
#include <onion/settings.hpp>

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

OnPressResult onpress_kstuff_autoload(OnPressContext &ctx) {
  const bool enabled = value_as_int(ctx);
  if (enabled == g_settings.kstuff_autoload)
    return OnPressResult::EarlyReturn;

  g_settings.kstuff_autoload = enabled;
  if (enabled) {
    unlink("/user/data/LnkizDen/no_kstuff");
    unlink("/data/LnkizDen/no_kstuff");
    notify("notify.kstuff.next_boot_on");
  } else {
    touch_file("/user/data/LnkizDen/no_kstuff");
    touch_file("/data/LnkizDen/no_kstuff");
    notify("notify.kstuff.next_boot_off");
  }
  return OnPressResult::Handled;
}

static OnPressResult id_download_cheats(OnPressContext &ctx) {
  ctx.dirty = false;
  std::string reply;
  if (!IPC_Client::getInstance(true).DownloadCheats(nullptr, nullptr, reply)) {
    notify("notify.cheats.sync.error", "ipc");
    return OnPressResult::Consumed;
  }
  onion_cjson::Root response(reply);
  const char *state = response && cJSON_IsObject(response.get())
                          ? onion_cjson::string_item(response.get(), "state")
                          : nullptr;
  if (!state) {
    notify("notify.cheats.sync.error", "ipc_response");
    return OnPressResult::Consumed;
  }
  const int task_id = onion_cjson::int_item(response.get(), "task_id", 0);
  if (std::strcmp(state, "already_running") == 0) {
    notify("notify.cheats.sync.busy");
  }
  /* Button confirmation has completed before this handler runs. */
  cheat_progress_show(task_id > 0 ? static_cast<uint32_t>(task_id) : 0);
  if (!cheat_progress_open_page()) {
    notify("notify.cheats.sync.error", "navigation");
  }
  return OnPressResult::Consumed;
}

OnPressResult onpress_delete_kstuff(OnPressContext &ctx) {
  (void)ctx;
  unlink("/user/data/LnkizDen/kstuff.elf");
  notify("notify.kstuff.deleted");
  return OnPressResult::Handled;
}

static bool ensure_settings_backup_directory(const std::string &directory) {
  if (mkdir(directory.c_str(), 0777) == 0)
    return true;
  if (errno != EEXIST)
    return false;

  struct stat status {};
  return lstat(directory.c_str(), &status) == 0 && S_ISDIR(status.st_mode);
}

static std::string settings_backup_path(int usb, const char *filename) {
  return "/usb" + std::to_string(usb) + "/LnkizDen/" + filename;
}

static OnPressResult id_export_settings(OnPressContext &ctx) {
  ctx.dirty = false;
  const int usb = usbpath();
  if (usb < 0) {
    notify("Connect a USB storage device before exporting settings.");
    return OnPressResult::Consumed;
  }

  const std::string directory =
      "/usb" + std::to_string(usb) + "/LnkizDen";
  if (!ensure_settings_backup_directory(directory)) {
    LOG_ERROR("Unable to create or inspect settings backup directory: %s",
              directory.c_str());
    notify("Could not create the settings backup folder on USB.");
    return OnPressResult::Consumed;
  }

  const std::string backup_path =
      settings_backup_path(usb, "settings-backup.ini");
  const std::string contents = onion::settings_serialize(g_settings);
  if (!write_file_atomic(backup_path.c_str(), contents.data(),
                         contents.size())) {
    LOG_ERROR("Unable to export settings to %s", backup_path.c_str());
    notify("Settings export failed. Check USB storage and free space.");
    return OnPressResult::Consumed;
  }

  LOG_INFO("Exported settings backup to %s", backup_path.c_str());
  notify("LnkizDen settings backup saved to USB.");
  return OnPressResult::Consumed;
}

static OnPressResult id_import_settings(OnPressContext &ctx) {
  ctx.dirty = false;
  const int usb = usbpath();
  if (usb < 0) {
    notify("Connect the USB storage device containing the settings backup.");
    return OnPressResult::Consumed;
  }

  const std::string backup_path =
      settings_backup_path(usb, "settings-backup.ini");
  onion::Settings restored{};
  if (!onion::settings_load_backup_file(backup_path.c_str(), &restored)) {
    LOG_ERROR("Settings backup is missing, invalid, or unsupported: %s",
              backup_path.c_str());
    notify("Settings restore failed. The USB backup is invalid or unsupported.");
    return OnPressResult::Consumed;
  }

  const std::string recovery_path =
      settings_backup_path(usb, "settings-before-restore.ini");
  const std::string current = onion::settings_serialize(g_settings);
  if (!write_file_atomic(recovery_path.c_str(), current.data(),
                         current.size())) {
    LOG_ERROR("Unable to write pre-restore settings backup to %s",
              recovery_path.c_str());
    notify("Restore stopped: could not save a backup of current settings.");
    return OnPressResult::Consumed;
  }

  if (!onion::settings_save(restored)) {
    LOG_ERROR("Unable to persist restored settings from %s",
              backup_path.c_str());
    notify("Settings restore failed while saving. The recovery backup is on USB.");
    return OnPressResult::Consumed;
  }

  g_settings = restored;
  (void)LoadSettings();
  IPC_Client::getInstance(false).Reload_Daemon_Settings();
  IPC_Client::getInstance(true).Reload_Daemon_Settings();
  LOG_INFO("Restored settings from %s; previous settings saved to %s",
           backup_path.c_str(), recovery_path.c_str());
  notify("LnkizDen settings restored. Previous settings are backed up on USB.");
  return OnPressResult::Consumed;
}

static OnPressResult id_activate_account(OnPressContext &ctx) {
  ctx.dirty = false;
  Activator activator(true);
  if (!activator.Valid()) {
    notify("notify.account.invalid");
    return OnPressResult::Consumed;
  }
  if (!activator.IsNotActivated()) {
    notify("notify.account.already");
    return OnPressResult::Consumed;
  }
  if (!activator.Activate()) {
    notify("notify.account.failed");
    return OnPressResult::Consumed;
  }
  notify("notify.account.activated");
  return OnPressResult::Consumed;
}

static OnPressResult id_lm_test(OnPressContext &ctx) {
  (void)ctx;
  LOG_DEBUG("LM's Test Button Pressed");
  return OnPressResult::Handled;
}

static OnPressResult id_onionhen_credits(OnPressContext &ctx) {
  (void)ctx;
  return OnPressResult::EarlyReturn;
}

static OnPressResult id_presentation_card(OnPressContext &ctx) {
  // Author and donator cards in the dynamic Toolbox XML are display-only.
  ctx.dirty = false;
  return OnPressResult::Consumed;
}

static const OnPressExactEntry kRootExact[] = {
    {"id_download_cheats", id_download_cheats},
    {"id_settings_backup_export", id_export_settings},
    {"id_settings_backup_import", id_import_settings},
    {"id_lm_test", id_lm_test},
    {"id_onionhen_credits", id_onionhen_credits},
    {"id_author_0xp0co", id_presentation_card},
    {"id_donator_aglx", id_presentation_card},
    {"id_donator_ljf", id_presentation_card},
    {"id_donator_szx", id_presentation_card},
    {"id_donator_xiaomao", id_presentation_card},
    {"id_donator_xie", id_presentation_card},
};

const OnPressExactEntry *onpress_misc_root_exact(size_t *count) {
  *count = sizeof(kRootExact) / sizeof(kRootExact[0]);
  return kRootExact;
}

static const OnPressExactEntry kAccountExact[] = {
    {"id_activate_account", id_activate_account},
};

const OnPressExactEntry *onpress_account_exact(size_t *count) {
  *count = sizeof(kAccountExact) / sizeof(kAccountExact[0]);
  return kAccountExact;
}
