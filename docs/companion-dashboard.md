# LnkizDen Companion Dashboard

The daemon can serve the paired Companion dashboard on TCP port `9049`. It is
disabled by default and does not use the existing loader or control ports.

## Enable and connect

1. On the console, edit `/data/LnkizDen/config.ini` (or the active
   `/user/data/LnkizDen/config.ini`) and set:

   ```ini
   [companion]
   dashboard_enabled=true
   ```

2. Restart LnkizDen or reload its settings. The daemon displays the console's
   current LAN URL and a temporary 12-digit pairing code in a notification.
3. Open that URL from a browser on the same trusted private network and enter
   the code. The code is single-use and expires after ten minutes. Pairing
   failures are rate-limited. A session is tied to the pairing device's LAN IP
   and expires after one hour without activity.
4. Set `dashboard_enabled=false` and reload settings or restart to stop the
   listener. The service also stops when the daemon shuts down or the console
   changes IP.

The dashboard uses plain HTTP because the PS5 service does not provision a
trusted TLS certificate. Use it only on a trusted network; pairing and session
cookies are not encrypted in transit. Same-origin and Host checks, private-LAN
peer filtering, bounded HTTP parsing, and a strict route/setting allowlist
reduce exposure but do not make an untrusted network safe.

## Data and current coverage

The service only serves embedded dashboard assets and versioned API routes. It
does not provide generic filesystem access, arbitrary file paths, ELF launching,
or Sony system-setting writes. Overlay changes use the existing LnkizDen
settings schema. Recovery points contain only a canonical LnkizDen settings
snapshot in `/data/LnkizDen/companion/recovery`; the newest 16 points are kept.
Restores validate the full snapshot before saving and reloading settings.

Runtime status and a bounded in-memory startup event list are available.
Compatibility evidence and cheat conflict analysis currently report
unavailable rather than making unsupported claims. No third-party web server or
GitHub project is vendored.
