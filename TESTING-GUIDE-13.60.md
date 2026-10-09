# LnkizDen v0.9.0 Testing Guide for PS5 Firmware 13.60

## ⚠️ CRITICAL SAFETY DISCLAIMER

**LnkizDen is in DEVELOPMENT and has NOT been proven stable on firmware 13.60.**

- This is a community-maintained fork of OnionHEN v0.9.0-beta1, which lists 13.60 as "under test."
- **No warranty is provided.** Injecting unauthorized payloads carries inherent risk of system corruption, data loss, or permanent hardware damage.
- **You proceed entirely at your own risk.** Back up your console data before any testing.
- If your PS5 was already corrupted twice, hardware diagnostics may be advisable before further testing.

## Prerequisites

1. **PS5 Firmware**: 13.60 (the exact version you're testing on)
2. **USB Device**: Formatted exFAT or FAT32, minimum 8 GB (for LnkizDen payload and backups)
3. **Backup Tools**: 
   - Official PS5 backup/restore via System Settings
   - External USB storage for system software backup
4. **Testing Environment**: 
   - Quiet, cool location (good power delivery)
   - UPS or stable power source (no brownouts during testing)
   - Wired Ethernet preferred (WiFi can introduce variability)

## Setup Phase (Pre-Injection)

### Step 1: System Baseline
```
1. Power off PS5 completely (not rest mode)
2. Wait 30 seconds
3. Power on and boot to Home
4. Go to Settings → System → System Software → System Software Version
   - Confirm: 13.60
5. Check Storage:
   - Settings → Storage
   - Note free space (at least 50 GB free recommended)
6. Check System Health:
   - Settings → System → Storage
   - Run "Manage Storage" and note any corruption warnings
```

### Step 2: Create Official Backup
```
1. Settings → System → Backup and Restore
2. Back Up Your PS5:
   - Select "Full" backup (includes all apps, save data, settings)
   - USB destination (minimum 500 GB external drive)
   - Allow ~30–60 minutes to complete
   - Verify completion message
```

### Step 3: Prepare USB Payload Device
```
1. Connect USB 3.0 drive to PC (at least 8 GB)
2. Format: exFAT or FAT32
3. Create directory structure on USB:
   └── LnkizDen/
       ├── payloads/
       ├── plugins/
       ├── overlays/
       └── data/

4. Copy payload files to USB:
   - bootstrapper.elf → USB:/LnkizDen/
   - daemon.elf → USB:/LnkizDen/
   - shellui.elf → USB:/LnkizDen/
   - util.elf → USB:/LnkizDen/
   - onion_elfldr.elf → USB:/LnkizDen/
```

## Injection Phase (Single Load Test)

### Step 1: Cold Boot Injection (Recommended First Test)

This injects LnkizDen once and verifies basic stability before enabling autoload.

```
1. COLD BOOT (critical):
   - Power off PS5 completely (hold power button 10+ seconds)
   - Wait 60 seconds
   - Do NOT press buttons or touch console

2. Plug USB payload device into PS5 USB port

3. Boot PS5 and watch for LnkizDen prompt
   - Press [Trigger combination] to inject payload
   - (Exact key sequence depends on bootstrapper; typically L2 + R2 or specific button hold)

4. Wait for Home screen to appear (should be normal)
   - LnkizDen overlay should appear as a new icon or menu option
   - Observe for 2+ minutes (watch for freezes, visual artifacts, audio cutouts)

5. Test Basic Function:
   - Open LnkizDen overlay (via app icon or game button menu)
   - Verify UI renders correctly (no graphical corruption)
   - Navigate a few menus (Settings, Plugin Manager if available)
   - Verify no crashes or hangs
```

### Step 2: Monitor for Stability Issues

During the single-load test, watch for any of these corruption indicators:

| Indicator | Action |
|-----------|--------|
| UI freezes for >5 seconds | **Power off immediately** (hold button) and do NOT restart |
| Screen artifacts (lines, color shifts) | **Power off immediately** |
| Audio cuts out or distorts | **Power off immediately** |
| Game/app crashes to Home | **Note which app**, power off, check System Software |
| System becomes unresponsive | **Force shutdown** (hold power 10+ seconds) |
| Unusual disk activity sounds | **Power off**, let cool for 5 minutes |

### Step 3: Expected Behavior (Single Load)

If injection succeeds without corruption:

```
✓ Home screen appears within 30 seconds
✓ LnkizDen icon or menu overlay is visible
✓ UI is responsive to controller input
✓ No visual artifacts or audio glitches
✓ Can navigate 2–3 menus without hang/crash
✓ Can power down gracefully (home button → Power Off)
```

### Step 4: Safe Mode Test (Optional, if Single Load Succeeds)

If the single-load test passes, test Safe Mode to ensure plugin/payload autoload can be disabled:

```
1. While powered on, mount USB and create marker file:
   - USB:/LnkizDen/safe_mode (empty file)

2. Power off completely and reboot

3. Inject again and observe:
   - Should boot normally (Safe Mode disables autoload of plugins/payloads)
   - No additional functionality vs. safe boot

4. Remove USB:/LnkizDen/safe_mode file
```

## Post-Injection Validation

### Step 1: Verify Console State

After safe shutdown from a successful injection test:

```
1. Power on (NO payload/USB plugged in)
2. Boot to Home normally
3. Settings → System → System Software:
   - Confirm still 13.60
4. Settings → Storage:
   - Check free space (should not have changed unexpectedly)
5. Try launching a game:
   - Pick a game you know works
   - Verify normal load speed and no crashes
6. Restart PS5:
   - Power off, wait 30 seconds
   - Power on, verify normal boot
```

### Step 2: Backup Settings (After First Successful Load)

If LnkizDen runs without corruption, export settings for rollback:

```
1. In LnkizDen overlay:
   - Navigate to Settings → Backup/Restore
   - Select "Export Settings to USB"
   - Confirm USB:/LnkizDen/data/settings_backup_*.json created

2. Keep this backup file safe for rollback if needed later
```

### Step 3: Log Observations

Use the template below to document your test:

```markdown
## Test Run #1 (Date: YYYY-MM-DD)
- **Firmware**: 13.60
- **Free Storage**: ___ GB
- **Payload Files Injected**: bootstrapper, daemon, shellui, util, onion_elfldr
- **Injection Method**: Cold boot USB
- **Safe Mode**: Enabled / Disabled
- **Duration (seconds until Home)**: ___
- **UI Responsiveness**: (1=laggy, 5=smooth) ___
- **Visual Artifacts**: Yes / No → (describe if yes)
- **Audio Issues**: Yes / No → (describe if yes)
- **Crashes**: Yes / No → (describe if yes)
- **Unexpected Behavior**: (list any odd observations)
- **Outcome**: ✓ Stable / ⚠️ Minor Issues / ❌ Corrupted

**Notes**:
```

## Autoload Test (Only After Successful Single Loads)

Only attempt to enable autoload after passing 3+ single-load tests with no issues:

```
1. Create marker file on persistent USB:
   - USB:/LnkizDen/autoload_enabled

2. Reboot normally (no explicit injection trigger)

3. Observe behavior:
   - Home should appear normally
   - LnkizDen should load in background
   - UI should be responsive

4. If unstable:
   - Power off, remove USB
   - Delete autoload_enabled marker
   - Retest without autoload
```

## Recovery Procedures

### If Console Becomes Unresponsive

```
1. Hold power button 10+ seconds (until LED turns off completely)
2. Wait 60 seconds
3. Unplug USB payload device
4. Power on normally
5. If Home screen appears → Injection issue (not hardware corruption)
6. If stuck on startup → Possible filesystem corruption
   → Insert official PS5 recovery USB or factory reset
```

### If Filesystem Corruption Suspected

```
1. Power off immediately
2. Do NOT attempt further injections
3. Obtain PS5 Recovery Media (USB 8GB+):
   - Download from official Sony support
   - Create via another PS5 or PC with official tool
4. Boot from recovery media and attempt:
   - Repair → Rebuild Database
   - Repair → Rebuild Filesystem
5. If recovery fails:
   - Factory Reset (loss of data)
   - Contact Sony support with corruption timeline
```

### If System Corruption Confirmed

```
1. Document:
   - Date and time of first corruption sign
   - What operation was running
   - Any error codes displayed

2. Contact:
   - Sony PlayStation Support (NOT the homebrew developers)
   - Provide console serial, firmware version, corruption description
   - Mention "unauthorized payload injection" to speed triage

3. DO NOT attempt further injections on this console
   - Risk of cascading damage increases
```

## Success Criteria (Release Readiness)

LnkizDen v0.9.0 on 13.60 is considered **stable enough for beta testing** if:

- ✓ Cold-boot injection succeeds 5+ times without corruption
- ✓ Home screen appears within 30 seconds each time
- ✓ UI overlay is responsive and renders correctly
- ✓ No graphics/audio artifacts observed
- ✓ Safe Mode marker correctly disables autoload
- ✓ Settings backup/restore work correctly
- ✓ Post-injection normal boot (without payload) works normally
- ✓ Games launch and play without regression

## Reporting Issues

If you encounter stability issues, please document:

1. **Test Details**:
   - Date/time of incident
   - Exact firmware (e.g., 13.60)
   - Free storage at time of crash
   - Which payload files were injected

2. **Observations**:
   - What was happening when it froze/crashed
   - Any error messages or codes
   - Duration from injection to failure
   - Screenshots or video (if you can safely capture)

3. **Recovery Steps**:
   - How you recovered (power cycle, factory reset, etc.)
   - Whether console boots normally without payload

4. **File**:
   - Create `TEST-REPORT-13.60-YYYYMMDD.md` with above info
   - Include in repository as evidence for next release

---

**Version**: LnkizDen v0.9.0-beta1 + Stability Enhancements  
**Last Updated**: 2024-10-07  
**Status**: DEVELOPMENT / BETA (13.60 testing phase)
