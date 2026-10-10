<p align="center">
  <img src="assets/logo.png" alt="LnkizDen" height="128" width="128"/>
</p>

<p align="center">
  <b>LnkizDen</b><br/>
  An all-in-one HEN and Toolbox for PlayStation 5
</p>

<p align="center">
  <a href="README_ZH.md">简体中文</a>
  ·
  <b>English</b>
</p>

<p align="center">
  <b><a href="#features">Features</a></b>
  ·
  <b><a href="#run">Run</a></b>
  ·
  <b><a href="#build">Build</a></b>
  ·
  <b><a href="#configuration">Configuration</a></b>
  ·
  <b><a href="#support">Support</a></b>
  ·
  <b><a href="#credits">Credits</a></b>
</p>

---

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-GPLv3-blue.svg" alt="license"/></a>
  <img src="https://img.shields.io/badge/Platform-PlayStation%205-003791?style=flat&logo=playstation" alt="PlayStation 5"/>
  <img src="https://img.shields.io/badge/C-00599C?style=flat&logo=c&logoColor=white" alt="C"/>
  <img src="https://img.shields.io/badge/C++-00599C?style=flat&logo=cplusplus&logoColor=white" alt="C++"/>
  <img src="https://img.shields.io/badge/Build-CMake-064F8C?style=flat&logo=cmake" alt="CMake"/>
</p>

<p align="center">
  LnkizDen is a modular payload stack for the PS5. One entry ELF prepares
  the system, injects a ShellUI Toolbox,<br/>and then runs overlays, cheats,
  payloads, and background services.
</p>

<p align="center">
  <img src="assets/screenshot_01.png" alt="LnkizDen home-screen entry"/>
</p>

<p align="center">
  <img src="assets/screenshot_02.png" alt="LnkizDen in-game monitor bar"/>
</p>

<p align="center">
  <img src="assets/screenshot_03.png" alt="LnkizDen Toolbox"/>
</p>

<br>

## Firmware and first-run safety

This fork is based on upstream **v0.9.0-beta1**, whose release lists firmware
**4.03–13.60 under test** and specifically says the 13.x work needs real-console
testing. The 13.60 profiles are present in the tagged source, but that beta
status is not a stability guarantee. This development checkout is not a
stable release: host CI cannot replace per-firmware PS5 testing of startup,
rest-mode, shutdown, and individual features. Review
[`docs/firmware-support.md`](docs/firmware-support.md) before testing.
Use the [release validation checklist](docs/release-validation.md) when
building or evaluating a candidate release.

Use a cold boot and send the payload only once. Do not load it again into a
ShellUI session that has already run it, and do not run another HEN or
hotkey-injecting tool alongside it. Back up saves and irreplaceable data using
supported console procedures first. No payload can guarantee that the
console will not crash or require recovery.

On first start, the bootstrapper atomically renames the legacy
`/data/OnionHEN` and `/user/data/OnionHEN` directories to their LnkizDen
counterparts when the destination does not exist. It never merges or deletes
files. If both old and new directories exist, startup stops and logs the
conflict; preserve both and resolve it manually before retrying.

### Safe mode

Enable **Toolbox → Preferences → Safe mode (next boot)**, or create an empty
`/data/LnkizDen/safe_mode` file (or
`/mnt/usb0/LnkizDen/safe_mode` on a mounted USB drive) before startup, to skip
kstuff autoload, user payload autostart, external plugin loading, and
title-bound SPRX modules. Turn the preference off (or remove the marker) and
cold-boot to return to normal startup. Core ShellUI integration, filesystem
mounts, and update-partition handling still run; safe mode is a diagnostic
mode, not a recovery environment or a guarantee against crashes or damage.

### Muse monitor palette

Enable **Toolbox → Preferences → Muse palette** for a soft rose-and-violet
version of the LnkizDen in-game monitor. The change is stored locally at
`/data/LnkizDen/muse_style`; turn it off to restore the classic palette. Reopen
the game to recreate the monitor with the selected colors. This styles only
LnkizDen's overlay and does not modify Sony-owned PS5 menus.

### Settings backup

Use **Toolbox → Preferences → Export Settings to USB** to write
`/usbN/LnkizDen/settings-backup.ini` on the first mounted USB device.
**Restore Settings from USB** validates the schema before applying it and saves
the current settings as `settings-before-restore.ini` first. Both export files
are written atomically; restoring settings does not restore payloads, plugins,
cheats, or account data.
Restore accepts only a complete canonical backup written by LnkizDen, rejecting
truncated files and settings values that the parser would otherwise silently
replace with defaults.

### Overlay presets

The **Toolbox → Monitoring & Display → Game Overlay** controls include
**Minimal**, **Performance**, and **Full** presets. Presets apply immediately,
save through the normal settings pipeline, and can be customized afterward
with the individual metric controls.

Payloads can be marked as favorites from their detail page. Favorite payloads
are sorted before other payloads; their `.favorite` marker remains beside the
payload file and can be removed by turning the option off.

External plugin inventory shows its runtime state and version. Plugin and
title-bound module controls refresh their page from the live inventory after
each operation, so the displayed state is not merely the requested toggle.
The Debug group also reports whether the core daemon, util, loader, and kstuff
processes are detected, including PIDs when available. Core services are
status-only: their startup order is managed by the bootstrapper and should not
be bypassed with independent start/stop controls.

# Features

LnkizDen is a practical homebrew stack for jailbroken PS5 consoles.

- **ShellUI Toolbox** — a settings page injected into the PS5 ShellUI
- **System preparation** — raise privileges, remount filesystems, and block the update partition
- **fSELF / fPKG** — bundled kstuff for homebrew SELF / PKG; loads by default, can be turned off in the Toolbox
- **Plugin runtime** — discovers, validates, starts, stops, and removes descriptor-bearing plugin ELFs
- **PS5 FTP server** — available as the optional external `onionHEN-ftpsrv-plugin`
- **ShadowMount+** — available as the optional external `onionHEN-shadowmountplus-plugin`
- **Remote Play pairing** — enable the native PS5 Remote Play service, generate a pairing PIN, and register a client from the Network section
- **User payload manager** — start and stop user-provided `.elf` payloads, with optional auto-start
- **Game overlay** — an in-game bar for FPS, CPU, GPU, RAM, temperatures, and network info
- **Cheat engine** — local JSON, SHN, MC4, and ShnExt files that can be toggled at runtime
- **DPI v2** — available as the optional external `onionHEN-dpiv2-plugin`
- **Console tools** — account activation, external HDD, Title IDs, fan control, shortcuts, and game options
- **App jailbreak** — allowlisted homebrew can ask the daemon for extra privileges through a sandbox FIFO
- **Resilient runtime** — the critical daemon and utility daemon run apart; the main daemon can restart the utility
- **Shared configuration** — the Toolbox and daemons use one versioned `config.ini`

LnkizDen does not include a kernel exploit. The first hop still needs an
external `elfldr` on **9021**. After that, LnkizDen starts its own private
loader on **9020** for later ELF and user-payload launches.

<br>

# Run

### Requirements

1. A jailbroken PS5
2. An external [PS5 `elfldr`](https://github.com/ps5-payload-dev/elfldr) listening on port **9021** for the first hop

### Load LnkizDen

1. Run the kernel exploit and start the external `elfldr` service.
2. Send `LnkizDen.elf` through the loader your exploit host provides.
3. Wait for the utility daemon, `kstuff`, and the main daemon to start, in that order.
4. Open PS5 Settings and enter the LnkizDen Toolbox.

Startup is sequential. After the first hop, LnkizDen uses its own
`onion_elfldr.elf` on port **9020** and keeps **9021** only as a fallback.

```text
LnkizDen.elf → bootstrapper → onion_elfldr.elf (:9020) → util.elf → kstuff.elf → daemon.elf → Toolbox
```

External plugins are discovered and started by the daemon after `kstuff` is
ready.

### FTP server

FTP is provided by the separate
[`onionHEN-ftpsrv-plugin`](https://github.com/OnionBuddies/onionHEN-ftpsrv-plugin).
Install it as `/data/LnkizDen/plugins/FTPS00001.elf`; LnkizDen discovers and
starts the plugin, and its dynamic settings page provides enable, port, and
restart controls. The plugin retains upstream `ftpsrv` commands such as
`KILL`, `SELF`, `SCHK`, `MTRW`, and `AUTHID` where supported.

### ShadowMount+

ShadowMount+ is provided by the optional external
[`onionHEN-shadowmountplus-plugin`](https://github.com/OnionBuddies/onionHEN-shadowmountplus-plugin).
Install its descriptor-bearing ELF under `/data/LnkizDen/plugins/`; the daemon
then owns discovery, start, stop, replacement, and removal. Its dynamic
page provides an immediate scan action. The plugin requires `kstuff` and keeps
its own options under `/data/shadowmount/config.ini`.

### DPI (network package installer)

DPI v2 is provided by the optional external
[`onionHEN-dpiv2-plugin`](https://github.com/OnionBuddies/onionHEN-dpiv2-plugin).
Install it as `/data/LnkizDen/plugins/DPIV00001.elf`; LnkizDen discovers and
starts it, and its dynamic settings page provides enable, API port, WebUI port,
and restart controls. The browser remains the installer interface, including
chunked uploads, queue management, staged-file reuse, and SSE progress. The
default WebUI URL is `http://<console>:12800`.

### Remote Play

Remote Play pairing is available from **Toolbox → Network → Remote Play**.
The current account must already be activated. If it is not, LnkizDen blocks
the Remote Play page and directs you to **Toolbox → Account → Account
activation**. Once the account is activated, LnkizDen enables the Remote Play
registry setting and displays the pairing PIN. Enter that PIN in the official
Remote Play client while the page is open; LnkizDen uses the native PS5 Remote
Play service to confirm the registered device.

The feature provides pairing and registration only. Video streaming and client
transport remain handled by Sony's native Remote Play service.

### Payloads

Place standalone payloads in:

```text
/data/LnkizDen/payloads/
```

Only plain `.elf` files are supported. Auto-start can be turned on in the Toolbox;
LnkizDen remembers that choice with a matching `.auto_start` file next to the ELF.

All `.elf` filenames use the same Payload page, loader, and auto-start flow. A
recorded running instance is left running by later launch and auto-start
requests. LnkizDen plugins are a separate category and belong in
`/data/LnkizDen/plugins/`.

### Cheats

Put cheat files in one directory. Names are `TITLEID_VERSION`, with an
optional process and 8-hex source ID:

```text
/data/LnkizDen/cheats/<TITLE_ID>_<VERSION>[_<PROCESS>][_<SOURCE_ID>].json
/data/LnkizDen/cheats/<TITLE_ID>_<VERSION>[_<PROCESS>][_<SOURCE_ID>].shn
/data/LnkizDen/cheats/<TITLE_ID>_<VERSION>[_<PROCESS>][_<SOURCE_ID>].mc4
/data/LnkizDen/cheats/<TITLE_ID>_<VERSION>[_<PROCESS>][_<SOURCE_ID>].ShnExt
```

`PROCESS` is optional. `SOURCE_ID` is a stable physical-source discriminator.
LnkizDen loads every compatible source for the title/version/process, so
independent JSON, SHN, MC4 and ShnExt files can coexist. A source with an
explicit process is only used for that process; a source without one is
generic.

Cheats load from disk. If a file changes, LnkizDen reloads it without restarting the whole stack.

`DOWNLOAD_CHEATS` downloads a cheat catalog ZIP over HTTPS (GitHub or cnb.cool), extracts the `cheats/` tree, and copies `json/`, `shn/`, and `mc4/` files into this directory with their original names. `[cheats] mirror=auto` uses cnb.cool when the UI/system language is Simplified Chinese, otherwise GitHub.

### Weather data

The weather client uses Open-Meteo geocoding and forecast data. Attribute the
provider as **Weather data by [Open-Meteo](https://open-meteo.com/)**. Its free
API tier is for non-commercial use and is subject to rate limits. Data use is
subject to CC BY 4.0 attribution requirements. Review the
[terms](https://open-meteo.com/en/terms) before deployment, especially for
commercial use. Weather lookups require a caller-supplied city; location is
not inferred or persisted.

<br>

# Build

### Dependencies

| Dependency | Purpose |
| --- | --- |
| [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) | Prospero compiler, target headers, runtime, and CMake wrapper |
| CMake 3.20+ and Ninja | Configure and build the payload graph |
| Clang / LLVM | Compile the `x86_64-sie-ps5` targets |
| `lzma` or `xz` | Compress the bootstrapper |
| Git and `curl` or `wget` | Initialize submodules and fetch external payload inputs |

### Full build

```shell
git submodule update --init --recursive

export PS5_PAYLOAD_SDK=/path/to/ps5-payload-sdk
./scripts/build.sh --jobs 8
```

The script configures the project, fetches external dependencies, and builds
the embed chain in the required order.

For the reproducible Docker build:

```shell
docker compose build onionhen-build
docker compose run --rm onionhen-build
```

### Common options

| Option | Description |
| --- | --- |
| `--build-type Debug\|Release` | Select the CMake build type |
| `--build-dir <path>` | Override the default `build/` directory |
| `--cache-dir <path>` | Override `.cache/dependencies/` |
| `--stub-missing` | Use compile-only placeholder external ELFs; never use on hardware |
| `--skip-unpacker` | Stop after building the bootstrapper |
| `--init-submodules` | Initialize submodules before dependency sync |

Run `./scripts/build.sh --help` for the complete option list.

### Outputs

| Path | Description |
| --- | --- |
| `build/bin/LnkizDen.elf` | Final payload sent to the console |
| `build/bin/bootstrapper.elf` | Uncompressed bootstrapper |
| `build/bin/bootstrapper.elf.lzma` | Bootstrapper embedded by the final payload |
| `build/bin/daemon.elf` | Critical daemon |
| `build/bin/util.elf` | Utility daemon |
| `build/bin/shellui.elf` | Toolbox injection payload |
| `build/lib/*.a` | In-tree static libraries |

Build outputs stay in `build/`. Downloads are cached in `.cache/dependencies/`.
Nothing is written back into `source/`.

### Tests

Host tests cover settings, IPC, payload helpers, cheat parsers, relocation,
Toolbox routing, and shared platform code:

```shell
make -C source/util/tests clean
make -C source/util/tests test -j8
```

Linking the host tests needs Keystone on `KEYSTONE_PREFIX`
(default: `/opt/homebrew`).

<br>

# Configuration

LnkizDen reads and writes the same config in two places:

```text
/data/LnkizDen/config.ini
/user/data/LnkizDen/config.ini
```

Most options can be changed in the Toolbox. The format is
`schema_version=1`. If no file exists, LnkizDen writes a commented
default from [`config.ini.example`](config.ini.example).

| Key | Default | Values |
| --- | --- | --- |
| `meta.schema_version` | `1` | `1` |
| `toolbox.language` | `system` | `system`, `zh-Hans`, `zh-Hant`, `en`, `ja`, `ko`, `fr`, `de`, `it`, `es`, `pt-BR`, `pt-PT`, `pl`, `ru`, `ar`, `th`, `nl`, `fi`, `sv`, `da`, `no`, `tr`, `cs`, `hu`, `el`, `ro`, `vi`, `id`, `uk` |
| `startup.open_after_load` | `none` | `none`, `home_menu` |
| `home_screen.show_title_ids` | `false` | `true`, `false` |
| `game_menu.show_onionhen_options` | `true` | `true`, `false` |
| `rest_mode.resume_reinject_delay_seconds` | `10` | `0` through `255` seconds |
| `cheats.mirror` | `auto` | `auto`, `github`, `cnb` |
| `app_jailbreak.debug_notifications` | `false` | `true`, `false` |
| `cooling.fan_control` | `automatic` | `automatic`, `temperature_threshold` |
| `cooling.temperature_threshold_celsius` | `77` | `0` through `100` |
| `overlay.enabled` | `true` | `true`, `false` |
| `overlay.background` | `true` | `true`, `false` |
| `overlay.edge` | `top` | `top`, `bottom` |
| `overlay.align` | `center` | `left`, `center`, `right` |
| `overlay.font_size` | `medium` | `small`, `medium`, `large` |
| `overlay.order` | `fps,cpu,gpu,memory,ip,fan` | comma-separated `fps`, `cpu`, `gpu`, `memory`, `ip`, `fan` |
| `overlay.show_cpu` / `overlay.show_gpu` / `overlay.show_memory` / `overlay.show_fps` | `true` | `true`, `false` |
| `overlay.cpu_usage_mode` | `average` | `average`, `per_core` |
| `overlay.show_ip_address` | `false` | `true`, `false` |
| `shortcuts.cheats_menu` | `off` | `off`, `r3_l3`, `l2_triangle`, `long_options`, `long_share`, `share` |
| `shortcuts.toolbox` | `off` | `off`, `l2_r3`, `long_share`, `share` |

### Runtime data

| Path | Purpose |
| --- | --- |
| `/data/LnkizDen/payloads/` | User payload ELFs |
| `/data/LnkizDen/plugins/` | Descriptor-bearing LnkizDen plugin ELFs |
| `/data/LnkizDen/cheats/` | Cheat files |
| `/data/LnkizDen/cheats_tmp/` | Temporary HTTPS ZIP and extraction files, cleaned after sync |
| `/data/LnkizDen/kstuff.elf` | Optional runtime override with priority over the embedded `kstuff` |
| `/data/LnkizDen/LnkizDen.log` | Main runtime log |
| `/data/LnkizDen/LnkizDen_crash.log` | Preserved daemon signal and backtrace log |
| `/data/LnkizDen/LnkizDen_util_daemon.log` | Utility daemon log |

<br>

# Host tools

| Tool | Purpose |
| --- | --- |
| [`scripts/daemon_log.py`](scripts/daemon_log.py) | Stream daemon logs from the console |
| [`scripts/shutdown_onion.py`](scripts/shutdown_onion.py) | Shut down the LnkizDen userland stack without killing `kstuff` |
| [`scripts/ps5_cmake.sh`](scripts/ps5_cmake.sh) | Run CMake through the PS5 payload SDK |
| [`scripts/sync_dependencies.sh`](scripts/sync_dependencies.sh) | Fetch or build external payload inputs |

<br>

# Repository layout

```text
.
├── assets/                    Project artwork
├── docs/                      Architecture and technical notes
├── scripts/                   Build, dependency, and host-side helpers
├── source/                    In-tree PS5 sources
│   ├── bootstrapper/          Startup and embedded payload chain
│   ├── daemon/                Critical daemon and Toolbox injection
│   ├── util/                  Utility daemon, IPC, and cheats
│   ├── shellui/               Toolbox and ShellUI hooks
│   ├── i18n/                  Shared Toolbox / notification locale catalogs
│   ├── unpacker/              Final LnkizDen payload wrapper
│   ├── libonion_*/            Shared in-tree libraries
│   ├── common/                Shared low-level implementations
│   └── platform/ps5/stubs/    PS5 system-library link stubs
├── third_party/               External source, archives, and submodules
├── tools/                     Repository-side generators
├── build/                     Generated build outputs (ignored)
└── .cache/dependencies/       Downloaded external inputs (ignored)
```

See [the architecture document](docs/arch.md) for the complete module and IPC map.

<br>

# Troubleshooting

1. Before loading LnkizDen, make sure the external `elfldr` is reachable on port **9021**.
2. Make sure the payload was built for this firmware with a complete PS5 Payload SDK.
3. If the Toolbox does not appear, wait for `util → kstuff → daemon` to finish and check the logs.
4. If the build cannot get `kstuff.elf`, run `./scripts/sync_dependencies.sh` again or initialize the submodule.
5. When you report a problem, include firmware, exploit host, SDK version, build type, logs, and steps to reproduce.

<br>

# Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md).

<br>

# Support

<a href="https://ko-fi.com/0xp0co"><img src="https://img.shields.io/badge/Ko--fi-Support-FF6433?style=for-the-badge&logo=kofi&logoColor=white" alt="Support LnkizDen on Ko-fi"/></a>

For Chinese users, you can also scan the QR codes below to donate:

国内用户也可以扫描下方二维码进行捐赠：

| Alipay (CN) | WeChat (CN) |
| --- | --- |
| <img src="assets/donation_alipay_cn.JPG" width="200" alt="Alipay"/> | <img src="assets/donation_wechat_cn.JPG" width="200" alt="WeChat"/> |

If LnkizDen is useful to you, consider supporting further development. Thank you.

<br>

# Credits

LnkizDen exists because of the PS5 homebrew and reverse-engineering community.

### Contributors

<p align="center">
  <a href="https://github.com/aydencharles/onionHEN/graphs/contributors">
    <img src="https://contrib.rocks/image?repo=aydencharles/onionHEN" alt="LnkizDen contributors"/>
  </a>
</p>

### Based on

- [etaHEN](https://github.com/LightningMods/etaHEN) — LightningMods and contributors; source base of this tree
- [GoldHEN](https://github.com/GoldHEN/GoldHEN) — SiSTR0 and contributors; the PS4 all-in-one HEN this project takes after

### Referenced

- [kstuff-lite](https://github.com/EchoStretch/kstuff-lite) — EchoStretch, sleirsgoevy, and contributors; Rest Mode Toolbox recovery follows its SceSysCore `NOTE_EXEC` watch for `NPXS40087` and wait for `libSceNpTrophy.sprx` / `libSceNpTrophy2.sprx`
- [ps5-payload-manager](https://github.com/itsplk/ps5-payload-manager) — itsplk; listen-socket rebind after Rest Mode (Unix IPC and TCP accept-fail self-heal) follows this project
- [HEN-Cheats-Collection](https://github.com/TeeKay87/HEN-Cheats-Collection) — TeeKay87; the community cheat collection downloaded by the built-in cheat sync
- [PHU Games Tools](https://github.com/ArkSama) — ArkSama; the in-game FPS counter follows PHU Games Tools skip-hook sampling (`/dev/dce` scanout and DMAP reads of `libSceAgcDriver`)

### Used or embedded

- [PS5 Payload SDK](https://github.com/ps5-payload-dev/sdk) — Prospero toolchain and headers
- [elfldr](https://github.com/ps5-payload-dev/elfldr) — first-hop loader on port 9021; not shipped in the payload
- [kstuff-lite](https://github.com/EchoStretch/kstuff-lite) — EchoStretch, sleirsgoevy, and contributors; optional `kstuff.elf`
- [onionHEN-ftpsrv-plugin](https://github.com/OnionBuddies/onionHEN-ftpsrv-plugin) — optional external FTP server plugin based on drakmor/ftpsrv
- [onionHEN-shadowmountplus-plugin](https://github.com/OnionBuddies/onionHEN-shadowmountplus-plugin) — optional external ShadowMount+ scanner/mounter plugin based on Drakmor's project
- [libhijacker](https://github.com/astrelsky/libhijacker) — astrelsky; process hijack and kernel R/W
- [NineS](https://github.com/buzzer-re/NineS) — buzzer-re; ShellUI injection
- [cJSON](https://github.com/DaveGamble/cJSON) — JSON parsing
- [7-Zip LZMA SDK](https://www.7-zip.org/sdk.html) — unpacker decompression
- [miniz](https://github.com/richgel999/miniz) — cheat-file decompression

### Testers

即食面, 雨之声, 大饼电玩, 安定区, 随风, 麒麟, 尼克库尔曼, 云, 啊烦, 小小蔡, B站谢锡榆, 荆枫, 苏打先森, 噢, 天天向上

Thanks as well to everyone else who tested, researched, or sent usable feedback.

<br>

# License

This project is licensed under the [GNU General Public License v3.0](LICENSE).
Third-party components retain their respective licenses and notices.

> LnkizDen is an unofficial homebrew project and is not affiliated with Sony Interactive Entertainment.
> Use it only on hardware you own and at your own risk. No warranty is provided.
