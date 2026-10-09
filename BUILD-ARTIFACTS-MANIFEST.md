# LnkizDen v0.9.0-beta1 Build Artifacts Manifest

**Build Date**: 2024-10-07  
**Platform**: PS5 (ARM64)  
**Firmware Target**: 13.60 (under test)  
**Build Configuration**: Release  
**Build System**: Ninja + CMake  
**SDK Version**: PS5 SDK v0.43 (verified)

## Final ELF Outputs

All artifacts are in: `build/bin/`

### Primary Payloads

| Name | Size | Purpose | SHA256 |
|------|------|---------|--------|
| **bootstrapper.elf** | 26 MB | Kernel bootstrap & SPRX loader | `99ba7932e0cad8b23ada75b59d078c7f0d92d6c3f7e67a148c6bd398b0dd0ead` |
| **daemon.elf** | 13 MB | Runtime service manager (plugins, payloads, settings) | `b987b72f7246e39e45018aeb2cf34d1afb03656ce6f7248faab7443bdce63467` |
| **shellui.elf** | 2.1 MB | Web UI overlay (menu, settings, plugin manager) | `094823f809fd71fcbb1f4c6265ae776d8d3111054c961cabeb2bb00fa537bbab` |
| **util.elf** | 9.6 MB | Utilities (memory editor, dumper, misc tools) | `6693196264d2362fb56ff9044f031e08c682e9f7dce80a29fcf7179213f7a793` |
| **onion_elfldr.elf** | 130 KB | ELF loader / secondary bootstrapper | `d1777926f4456e9a9693cb4d292b7c9e5bf6c5c1489e15d8e37dae9f13eb5b9a` |

### Supporting Outputs

| Name | Size | Purpose | SHA256 |
|------|------|---------|--------|
| **bootstrapper.elf.pre-lzma** | 26 MB | Uncompressed bootstrapper (before LZMA) | N/A |
| **bootstrapper.elf.lzma** | 13 MB | LZMA-compressed bootstrapper for smaller payload | N/A |
| **test.elf** | 26 MB | Host test binary (regression suite) | `552808...` |

## Build Flags & Configuration

```cmake
CMAKE_BUILD_TYPE: Release
CMAKE_CXX_FLAGS_RELEASE: -O3 -DNDEBUG
CMAKE_C_FLAGS_RELEASE: -O3 -DNDEBUG
PS5_PAYLOAD_SDK: ${PS5_PAYLOAD_SDK} (v0.43)
KEYSTONE_PREFIX: /mnt/c/Users/hp/OneDrive/Documents/lnkiz-den/third_party/keystone (absolute path)
```

## Compiler Versions

- **clang**: 16.0.6 (PS5 SDK)
- **cmake**: 3.28.1
- **ninja**: 1.12.0
- **make**: 4.3

## Dependencies Built

The following third-party libraries were built from pacbrew and linked statically:

- **libcurl** v8.18.0 (HTTP client)
- **zstd** v1.5.6 (compression)
- **zlib** v1.3.1 (compression)
- **openssl** v3.0.10 (cryptography & SSL/TLS)
- **libpsl** v0.21.5 (Public Suffix List for cookie handling)

All are static archives in: `${PS5_PAYLOAD_SDK}/target/user/homebrew/lib/`

## Known Changes from OnionHEN v0.9.0-beta1

### Features Added

1. **Safe Mode**
   - Marker file: `/data/LnkizDen/safe_mode` or `/mnt/usb0/LnkizDen/safe_mode`
   - Disables: kstuff autoload, user payload autostart, plugin autostart, title-bound SPRX loading
   - Preserves: Core mounts, ShellUI, system handling

2. **Atomic Settings**
   - All settings writes now use mkstemp + fsync + rename pattern
   - Prevents partial/corrupted settings files
   - Exact-file validation on backup restore (rejects truncation/parser defaults)

3. **Data Migration**
   - Automatic rename: `/data/OnionHEN` → `/data/LnkizDen`
   - Conflict detection and rollback on failure

4. **USB Settings Backup/Restore**
   - Export: Settings → Backup/Restore → "Export Settings to USB"
   - Import: Settings → Backup/Restore → "Import Settings from USB"
   - Validates backup is not truncated or corrupted

5. **Overlay Presets**
   - Minimal: Only core UI, no plugins/services
   - Performance: Balanced UI + selective plugins
   - Full: Complete UI + all services

6. **Payload Favorites**
   - Mark/unmark payloads as favorites for quick access
   - Persistent marker files

7. **Plugin/SPRX Runtime Status**
   - Display: status (running/stopped), version, uptime
   - Automatic inventory refresh after operations

### Files Modified

- **bootstrap_filesystem.c**: Data migration with rollback
- **launch_pipeline.c**: Safe Mode gating for kstuff autoload
- **payload_autostart.c**: Safe Mode gating for user payload autostart
- **plugin_manager_runtime.cpp**: Safe Mode gating for plugin operations
- **sprx_plugin_manager_runtime.cpp**: Safe Mode gating for SPRX operations
- **settings.cpp**: Atomic writes + backup validation
- **onpress_misc.cpp**: USB settings backup/restore handlers
- **onpress_overlay.cpp**: Overlay preset buttons
- **onpress_payloads.cpp**: Payload favorites
- **onpress_sprx.cpp**: Added missing `#include "toolbox_i18n.hpp"`
- **plugin_sprx_pages.cpp**: Runtime status display
- **toolbox_xml.cpp**: UI for settings, overlays, plugins, SPRX, debug info

### Documentation Added

- **TESTING-GUIDE-13.60.md**: Comprehensive testing protocol for firmware 13.60
- **docs/firmware-support.md**: Firmware boundaries and support matrix
- **docs/release-validation.md**: Release validation gate and per-firmware checklist
- **README.md**: Updated branding (LnkizDen), safety disclaimers, Safe Mode & backup docs

## Validation Status

### Host Regression Tests ✅

All host-side tests pass:

```
make -C source/util/tests test
✓ Keystone test (disassembly accuracy)
✓ Settings atomic write validation
✓ Backup file exact-match validation
✓ Data migration rollback simulation
```

### PS5 Payload Build ✅

- [x] All .elf files compile without errors
- [x] All .elf files link successfully
- [x] No undefined symbol references
- [x] LZMA compression successful for bootstrapper
- [x] Artifact sizes reasonable (no bloat)

### Hardware Testing ❌ (PENDING)

- [ ] Cold boot injection on PS5 13.60
- [ ] Safe Mode marker gating verification
- [ ] USB settings backup/restore on 13.60
- [ ] Overlay presets rendering
- [ ] Plugin/SPRX status display
- [ ] System stability over 5+ injection cycles
- [ ] Post-injection normal boot (no payload)
- [ ] Game launch without regression

## Deployment Checklist

### For Console Testing

```
1. [ ] Create USB payload device:
   - Format USB 3.0 (8GB+ capacity) as exFAT
   - Create LnkizDen/payloads, LnkizDen/plugins, LnkizDen/data directories
   - Copy all .elf files to LnkizDen/ root:
     * bootstrapper.elf
     * daemon.elf
     * shellui.elf
     * util.elf
     * onion_elfldr.elf

2. [ ] Perform official PS5 backup
   - Settings → Backup and Restore → Back Up Your PS5 (Full)
   - Destination: External USB drive (500GB+)
   - Wait for completion confirmation

3. [ ] Cold boot injection test (SINGLE LOAD)
   - Power off PS5 completely (hold 10+ seconds)
   - Wait 60 seconds
   - Plug in USB payload device
   - Boot and inject payload (follow bootstrapper prompt)
   - Observe Home screen for 2+ minutes (no freezes/artifacts)
   - Test UI: open overlay, navigate 2–3 menus
   - Power off gracefully

4. [ ] Stability check after single load
   - Power on PS5 (no USB)
   - Verify normal boot and game launch
   - Check free storage (should be unchanged)
   - Restart and verify normal boot again

5. [ ] If single loads pass, test Safe Mode
   - Create USB:/LnkizDen/safe_mode (empty file)
   - Inject again and observe normal startup
   - Remove safe_mode marker and re-test

6. [ ] Document observations in TESTING-GUIDE-13.60.md
   - Use TEST-REPORT-13.60-YYYYMMDD.md template
   - Include any crashes, visual issues, or anomalies
```

## Known Limitations

1. **Not a Stable Release**
   - This build is based on OnionHEN v0.9.0-beta1, which lists 13.60 as "under test"
   - No proven immunity to system corruption or data loss
   - Safe Mode is a mitigation, not a guarantee

2. **Safe Mode Does NOT Provide Recovery**
   - Safe Mode disables autoload but does NOT enter a sandbox or recovery environment
   - Core mounts, ShellUI, and system services still run normally
   - If a critical operation crashes, Safe Mode does not prevent corruption

3. **No Hardware-Level Protection**
   - This is software-based; no protection at firmware level against bad writes
   - Malicious plugins or payload code can still damage the console

4. **Requires Manual Testing**
   - Stability claims depend on community testing on 13.60 hardware
   - Each console may behave differently (hardware revision, firmware variant, etc.)

## Artifact Integrity

To verify artifacts after download:

```bash
# On your PC:
sha256sum bootstrapper.elf
# Expected: 99ba7932e0cad8b23ada75b59d078c7f0d92d6c3f7e67a148c6bd398b0dd0ead

sha256sum daemon.elf
# Expected: b987b72f7246e39e45018aeb2cf34d1afb03656ce6f7248faab7443bdce63467

# (Repeat for all .elf files)
```

If any checksum does not match, the artifact may be corrupted in transit. Re-download before injection.

---

**Build Status**: ✅ COMPLETE  
**Ready for Testing**: YES  
**Recommended Next Step**: Follow TESTING-GUIDE-13.60.md for cold-boot injection test
