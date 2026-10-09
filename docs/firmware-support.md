# Firmware support and first-run migration

## 13.60

The upstream `v0.9.0-beta1` release identifies firmware 4.03–13.60 as
**under test**, says the 13.x support is new, and asks for real-console testing.
The tagged source contains exact HomeUI profiles through 13.6, including a
separate `13.600.007` OTA bundle profile. This establishes that the tag has
13.60-specific patch data; it does not establish that every feature, plugin,
exploit host, or console state is safe or stable.

Release notes:

- [v0.9.0-beta1](https://github.com/aydencharles/onionHEN/releases/tag/v0.9.0-beta1)
- [Tagged HomeUI profile table](https://github.com/aydencharles/onionHEN/blob/v0.9.0-beta1/source/shellui/src/homeui_top_nav_profiles.inc)

The release warns that this beta can fail to inject, miss the home-screen
shortcut, or regress firmware that v0.0.13 handled. It explicitly recommends
a cold boot followed by one payload load, and warns that loading into a
ShellUI session that already ran the payload is unsupported. Treat those
warnings as required precautions, not optional troubleshooting suggestions.

## Related-project comparison

- An [unofficial etaHEN-derived 11.00–13.60 port](https://github.com/GronedWaffel/etahen-11.00-13.60/blob/main/PORT-STATUS.md)
  documents selected firmware-specific tests and limitations. That is a
  project-reported validation record, not independent certification and not
  evidence for this fork's payloads or plugins.
- The [official etaHEN repository](https://github.com/etaHEN/etaHEN) and
  [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) do not, by
  themselves, establish this build's 13.60 compatibility.
- [GoldHEN](https://github.com/GoldHEN/GoldHEN) is a PS4 project and is not a
  PS5 13.60 compatibility reference. Its public project documents
  experimental-feature warnings; adopt the practice of clearly labeling
  unverified features, but not its PS4-specific code or firmware assumptions.

## Data-directory migration

LnkizDen now uses `/data/LnkizDen` and `/user/data/LnkizDen`. Before the first
startup, back up the existing OnionHEN directories using a supported file
transfer method. The bootstrapper then:

1. Checks both legacy and destination paths before changing either.
2. Refuses migration if an old and new directory both exist; it never merges
   uncertain data or overwrites a destination.
3. Uses same-filesystem directory renames, preserving file contents and
   metadata without copying files or deleting their contents.
4. Rolls back the `/data` rename if the separate `/user/data` rename fails.
5. Stops before system-partition operations if migration or directory setup
   fails.

The original directory *name* changes on a successful rename, so retain the
external backup until the new build has been verified. If migration reports a
conflict or rollback failure, do not retry blindly; keep both directories and
resolve them manually.

## Validation boundary

The beta's firmware-specific profiles are not a substitute for testing the
complete startup chain on the exact console. Avoid duplicate HEN/kstuff
instances and third-party plugins while diagnosing failures. No homebrew
payload can promise to prevent all crashes, system corruption, or the need for
console recovery.

## Safe mode

Create `/data/LnkizDen/safe_mode` or `/mnt/usb0/LnkizDen/safe_mode` on a mounted
USB drive before startup to skip kstuff autoload, user-payload autostart,
external plugin autostart/manual start/reload, and title-bound SPRX module
loading. Remove the marker and cold-boot to resume normal startup. Core ShellUI
integration, system-partition mounts, and update-partition handling still run;
safe mode is diagnostic isolation, not a recovery environment or a guarantee
against damage.
