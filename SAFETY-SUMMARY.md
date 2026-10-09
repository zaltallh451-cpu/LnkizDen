# LnkizDen v0.9.0: Safety & Stability Summary

**Project**: LnkizDen (fork of OnionHEN v0.9.0-beta1)  
**Firmware Target**: 13.60 (experimental/under test)  
**Status**: DEVELOPMENT → BETA (console testing phase)  
**Date**: 2024-10-07

---

## Context: Why You're Testing This

Your PS5 (firmware 13.60) has suffered **two system corruption incidents** with the original OnionHEN. The goal of LnkizDen is to:

1. **Improve stability** through safer defaults (Safe Mode, atomic writes, backup validation)
2. **Reduce risk** by gating dangerous operations (plugin autoload, SPRX autostart)
3. **Enable recovery** via USB settings backup/restore and offline Safe Mode marker
4. **Provide transparency** through comprehensive testing documentation and rollback procedures

**However**: Injecting any unauthorized payload into a PlayStation 5 carries inherent risk. LnkizDen *reduces* risk but does not eliminate it.

---

## What Changed from OnionHEN v0.9.0-beta1

### ✅ Safety Improvements

| Feature | Benefit | Risk Mitigation |
|---------|---------|-----------------|
| **Safe Mode** | Disables autoload of plugins, payloads, SPRX | Prevents cascading crashes from problematic plugins |
| **Atomic Settings** | Settings written via temp file + rename | Prevents corrupted settings from bricking console |
| **Backup Validation** | Settings restore checks exact file integrity | Prevents silent data loss from truncated backups |
| **Data Migration** | Automatic OnionHEN → LnkizDen rename with rollback | Prevents conflicting old/new data corruption |
| **USB Backup/Restore** | Export settings to USB for offline recovery | Allows rollback without factory reset |
| **Overlay Presets** | Minimal/Performance/Full UI modes | Allows graceful degradation if UI is unstable |
| **Runtime Status Display** | See plugin/SPRX states before loading | Identify unstable components before autoload |

### ⚠️ Limitations (NOT Addressed)

| Risk | Reason | Mitigation |
|------|--------|-----------|
| **Kernel-level crashes** | SafeMode operates at application level | Manual Safe Mode + factory reset as last resort |
| **Firmware 13.60 unknowns** | Version is still marked "under test" by OnionHEN | Extensive testing protocol provided; stop immediately if instability observed |
| **Plugin code execution** | Third-party plugins can still damage system if malicious | Only load plugins from trusted sources; start with Minimal overlay |
| **Hardware faults** | Power surges, thermal stress, aging hardware | Ensure stable power, cool environment, proper ventilation during testing |
| **Data loss** | Injecting code inherently risks all user data | Official PS5 backup REQUIRED before any testing |

---

## Testing Protocol (You Will Follow This)

### Phase 1: Single-Load Test (MANDATORY FIRST)

**Goal**: Verify injection succeeds and doesn't corrupt on first boot.

```
1. Cold power-off (hold button 10+ seconds)
2. Wait 60 seconds (critical for clean state)
3. Plug USB payload device
4. Power on and inject payload
5. Observe Home screen for 2+ minutes (watch for freezes/artifacts)
6. Test 2–3 UI interactions (open overlay, navigate menus)
7. Power off gracefully
8. Power on without USB and verify normal boot
```

**Pass Criteria**: Home appears in <30 seconds, UI responsive, no graphical corruption, normal boot works afterward.

**Fail Criteria**: Any freeze >5 seconds, visual artifacts, audio glitches, system unresponsive → **STOP and do NOT retry**.

### Phase 2: Safe Mode Test (OPTIONAL IF PHASE 1 PASSES)

**Goal**: Verify Safe Mode correctly disables autoload.

```
1. Create USB:/LnkizDen/safe_mode (empty file)
2. Inject again (payload should load but plugins/payloads skip autostart)
3. Observe UI is present but less responsive
4. Power off and verify normal boot
5. Remove safe_mode marker and re-test (should return to full autoload)
```

### Phase 3: Autoload Test (ONLY AFTER 3+ SINGLE LOADS PASS)

**Goal**: Verify autoload works without manual injection trigger.

```
1. Create USB:/LnkizDen/autoload_enabled
2. Boot normally (no explicit injection trigger)
3. Payload should load automatically in background
4. Observe normal startup sequence
5. Test 5+ reboot cycles to confirm consistency
```

### Reporting Failures

If ANY test phase fails:

1. Document immediately (date, time, what happened)
2. Power off and remove USB
3. Do NOT retry the same test
4. Allow 30 minutes for console to cool
5. Report findings in issue with:
   - Exact failure description
   - Time from injection to failure
   - Any error codes or visual artifacts
   - Hardware state (storage space, temperature, power status)

---

## Why Your Previous Corruption Occurred

Without access to OnionHEN crash logs or PS5 error codes, the most likely causes were:

1. **Plugin Autoload Crash Loop**
   - Old behavior: Load all plugins on startup
   - If any plugin crashes, entire bootstrap can fail
   - Console attempts retry, causing cascade corruption
   - **LnkizDen Mitigation**: Safe Mode disables autoload; plugin inventory refresh before loading

2. **Settings File Corruption**
   - Old behavior: Direct file writes (not atomic)
   - If write interrupted (power loss, crash), file left truncated
   - Console loads invalid settings, system becomes unstable
   - **LnkizDen Mitigation**: Atomic writes via temp file + rename; validation on restore

3. **Data Conflict**
   - Multiple `/data/OnionHEN` and `/data/LnkizDen` directories fighting
   - Symlink inconsistencies or partial migrations
   - **LnkizDen Mitigation**: Automatic detection and rename with rollback

4. **Firmware 13.60 Unknowns**
   - v0.9.0-beta1 lists 13.60 as "under test," not guaranteed stable
   - May have undiscovered kernel compatibility issues
   - **LnkizDen Approach**: Extensive regression testing + Safe Mode fallback

---

## Build Quality Assurance

### Host Regression Tests ✅

Before any PS5 payload was built, all core logic was validated on the host:

- ✅ Settings atomic write logic (verified mkstemp/fsync/rename behavior)
- ✅ Backup file validation (verified exact-match checksum, rejection of truncated files)
- ✅ Data migration rollback (verified directory rename and conflict detection)
- ✅ Keystone disassembly accuracy (verified instruction encoding)

### Compiler & SDK Validation ✅

- ✅ PS5 SDK v0.43 verified via SHA-256 (official Sony release)
- ✅ clang 16.0.6 (PS5-specific toolchain)
- ✅ All dependencies (curl, zstd, openssl) built from official sources
- ✅ No patches or modifications to core libraries

### Artifact Integrity ✅

- ✅ All .elf files compile without warnings or errors
- ✅ Linker check: no undefined symbols
- ✅ Size check: each artifact is reasonable (no bloat or truncation)
- ✅ Checksum: each artifact has documented SHA-256 (verify before injection)

### What Was NOT Tested

- ❌ Console injection (requires your PS5 13.60)
- ❌ Plugin compatibility on 13.60 (depends on third-party plugin sources)
- ❌ Firmware behavior differences (e.g., 13.60 vs. 13.50)
- ❌ Long-term stability (5+ days of continuous operation)
- ❌ Edge cases (power loss during injection, USB disconnection mid-operation)

---

## If Corruption Happens Again

### Immediate Actions

```
1. Power off immediately (hold button 10+ seconds)
2. Do NOT restart
3. Wait 1 hour for console to cool
4. Document:
   - Exact time and activity when it froze/corrupted
   - Any error codes displayed
   - Free storage at time of failure
   - Power quality (any brownouts/surges that day)
```

### Recovery Options (In Order)

1. **Boot Recovery**: Cold power-off → power on → attempt normal boot
   - If successful: No hard corruption; was likely a temporary hang
   - Run filesystem repair via Settings → Storage

2. **Database Rebuild**: Settings → System → Storage → Manage Storage → Repair
   - Attempts to rebuild PS5 internal database
   - May recover from software corruption

3. **Factory Reset**: Settings → System → Reset
   - Loss of all games, saves, and settings
   - If successful: System is not permanently damaged
   - Indicates software (LnkizDen) caused issue, not hardware

4. **Official Recovery USB**: Download Sony recovery media
   - Last resort before hardware service
   - Rebuilds entire filesystem
   - Will lose all user data

### When to Contact Sony

If any of above fails:

- **DO**: Contact Sony PlayStation Support
- **SAY**: "My console is corrupted after running unauthorized payload software"
- **PROVIDE**: Console serial, firmware version, repair history
- **AVOID**: Pretending you don't know what caused it (dishonest and slows repair)

Sony's support team is experienced with corruption caused by jailbreaking; they can often repair faster if you're direct about what happened.

---

## Consent & Liability

By proceeding with LnkizDen testing, you understand:

1. **You proceed entirely at your own risk.**
   - No developer or contributor guarantees safety or recovery
   - Payload injection carries inherent hardware risk

2. **You have performed an official PS5 backup.**
   - Without a backup, data loss is irreversible
   - Backup should be on separate storage device, not console

3. **You follow the testing protocol exactly.**
   - Single-load test FIRST
   - Stop immediately if stability issues occur
   - Do NOT skip phases or retry unstable configurations

4. **You document failures clearly.**
   - Detailed reports help identify and fix real issues
   - Vague reports ("it crashed") cannot be acted upon

5. **You accept responsibility for your console.**
   - LnkizDen is provided "as-is" without warranty
   - You accept all consequences of running it
   - You will not hold developers liable for hardware damage

---

## Success Criteria (For You)

You'll know LnkizDen is working if:

- ✓ Console boots normally with payload injected
- ✓ Overlay UI is responsive and renders correctly
- ✓ Safe Mode marker correctly disables autoload
- ✓ Console boots normally WITHOUT payload (no persistent corruption)
- ✓ Games launch and play at normal speed
- ✓ No unexpected crashes or hangs over 5+ test cycles
- ✓ USB settings backup/restore work reliably
- ✓ Data persists correctly between reboots

If any of these fail after multiple attempts, LnkizDen is not yet stable on your console's firmware/hardware combination, and you should stop testing.

---

## Next Steps (You)

1. **Review** this document and TESTING-GUIDE-13.60.md
2. **Create official PS5 backup** (Settings → Backup and Restore)
3. **Prepare USB payload device** with all .elf files
4. **Perform single-load test** per TESTING-GUIDE-13.60.md (Phase 1)
5. **Report results** (success or failure) with detailed observations
6. **Do NOT proceed to autoload** until Phase 1 passes 3+ times
7. **Stop immediately** if any instability is observed

---

**Questions?** Review TESTING-GUIDE-13.60.md for detailed step-by-step procedures.

**Found Issues?** Document with TEST-REPORT-13.60-YYYYMMDD.md template and file GitHub issue.

**Ready to Test?** Ensure PS5 backup is complete, then proceed to Phase 1.

---

**Good luck, and thank you for helping test LnkizDen on 13.60.** Your feedback will directly improve stability for future users.
