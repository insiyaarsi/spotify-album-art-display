# Spotify Album Display (Wokwi Simulator)

ESP32 firmware that displays the album art and track info for whatever you are
currently playing on Spotify. This repository covers the **simulator phase** —
everything is proven in [Wokwi](https://wokwi.com) before buying hardware.

![Status](https://img.shields.io/badge/phase-simulator-blue)
![Platform](https://img.shields.io/badge/platform-ESP32-informational)
![Display](https://img.shields.io/badge/display-ILI9341%20240x320-lightgrey)

## What it does

- Connects to Wi-Fi (`Wokwi-GUEST` in the simulator)
- Exchanges a stored refresh token for short-lived Spotify access tokens over HTTPS,
  renewing 5 minutes before expiry
- Polls the currently-playing endpoint every 5 seconds
- Downloads the album-art JPEG — preferring the **300×300** variant, falling back to
  **64×64** — and scales it to fit a 240×240 art area
- Renders the track name in a 40 px top bar and the artist in a 40 px bottom bar
- Redraws only when the track ID changes, so a playing song does not re-download art
- Shows a **Nothing playing** screen when playback is paused or stopped
- Retries failed HTTPS requests up to 3 times with increasing backoff

### Display layout (240×320, portrait)

```
┌──────────────────────┐
│  Track name          │  40 px top bar
├──────────────────────┤
│                      │
│     Album art        │  240×240 art area
│                      │
├──────────────────────┤
│  Artist name         │  40 px bottom bar
└──────────────────────┘
```

## Prerequisites

| Requirement | Why |
|---|---|
| **Spotify Premium** | The currently-playing API requires it |
| Spotify Developer app | Provides the Client ID / Secret |
| Python 3.9+ | One-time OAuth login + credential validation |
| [Wokwi VS Code extension](https://marketplace.visualstudio.com/items?itemName=Wokwi.wokwi-vscode) | Runs the simulator |
| `arduino-cli` **or** Docker Desktop | Compiles the firmware (Wokwi cannot start without a compiled binary) |

A copy of `arduino-cli.exe` is vendored in `tools/` for Windows; the build script
falls back to it automatically if `arduino-cli` is not on your `PATH`.

## Project structure

```
spotify-album-display/
├── .devcontainer/           # Docker build env (Arduino CLI + ESP32 core)
│   ├── Dockerfile
│   └── devcontainer.json
├── docs/
│   └── credentials.md       # Spotify OAuth walkthrough
├── scripts/
│   ├── spotify_auth.py      # One-time browser login → refresh token
│   ├── spotify_validate.py  # Validate credentials + inspect JSON
│   ├── build_wokwi.ps1      # Firmware build (Windows)
│   ├── build_wokwi.sh       # Firmware build (macOS/Linux/container)
│   └── requirements.txt
├── wokwi/
│   ├── wokwi.ino            # Firmware
│   ├── wokwi.toml           # Points Wokwi at the compiled .bin/.elf
│   ├── diagram.json         # ESP32 + ILI9341 wiring
│   ├── tft_setup.h          # TFT_eSPI pin config
│   ├── build_opt.h          # Force-includes tft_setup.h into the build
│   ├── libraries.txt        # TFT_eSPI, ArduinoJson, TJpg_Decoder
│   ├── secrets.example.h
│   └── secrets.h            # Your credentials (gitignored)
├── .env.example             # Python-side credentials template
└── wokwi-simulator.code-workspace
```

> **Note:** `tft_setup.h` must be included before `TFT_eSPI.h`. This is handled by
> `build_opt.h`, which passes `-include "tft_setup.h"` to the compiler. Do not
> remove it, or TFT_eSPI will fall back to its own default pin map.

## Quick start

### 1. Get Spotify credentials

Full walkthrough: [docs/credentials.md](docs/credentials.md). In short:

1. Create an app in the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Add redirect URI `http://127.0.0.1:8888/callback`.
3. Allowlist your own Spotify account under Development Mode.

Then set up the Python environment and copy your Client ID/Secret into `.env`:

```bash
cp .env.example .env     # then edit .env with your Client ID and Secret
python -m venv .venv
```

Activate it — **PowerShell:**

```powershell
.venv\Scripts\Activate.ps1
```

**macOS/Linux:**

```bash
source .venv/bin/activate
```

Install dependencies and run the one-time login:

```bash
pip install -r scripts/requirements.txt
python scripts/spotify_auth.py      # opens a browser; prints a refresh token
```

Paste the printed refresh token into `.env` as `SPOTIFY_REFRESH_TOKEN`, start
playing something on Spotify, then confirm it all works:

```bash
python scripts/spotify_validate.py
```

You should see the current track ID, title, artist, and album-art URL.

### 2. Configure firmware secrets

```bash
cp wokwi/secrets.example.h wokwi/secrets.h
```

Fill in the same three values from `.env` (`SPOTIFY_CLIENT_ID`,
`SPOTIFY_CLIENT_SECRET`, `SPOTIFY_REFRESH_TOKEN`). `wokwi/secrets.h` is gitignored
and must never be committed.

### 3. Build the firmware

Wokwi cannot start without compiled binaries, so build before simulating.

**Windows (PowerShell):**

```powershell
.\scripts\build_wokwi.ps1
```

**macOS/Linux:**

```bash
./scripts/build_wokwi.sh
```

Either script installs the ESP32 core and the three required libraries if missing,
then produces `wokwi/build/wokwi.ino.bin` and `wokwi/build/wokwi.ino.elf`.

<details>
<summary><strong>Alternative: build in a Docker container</strong> (keeps the toolchain off your PC)</summary>

With Docker Desktop installed:

1. Open the repo folder in VS Code/Cursor.
2. Run **Dev Containers: Reopen in Container** from the command palette.
3. The container installs Arduino CLI, the ESP32 core, and libraries, then builds
   the firmware automatically via `postCreateCommand`.
4. Open `wokwi-simulator.code-workspace` inside the container and start the simulator.

The Wokwi UI still runs in your editor; only compilation happens in the container.
</details>

### 4. Run the simulator

1. Open **`wokwi-simulator.code-workspace`** (double-click it in the repo root).
   Wokwi needs `diagram.json` and `wokwi.toml` at the workspace root — opening the
   repo root directly will **not** work.
2. Press **F1 → "Wokwi: Start Simulator"**.
3. Play music on Spotify from the same Premium account you allowlisted.

**What to expect:** the display stays blank for a few seconds while the firmware
connects to Wi-Fi and authenticates — the screen is only initialized *after* both
succeed. Watch the serial monitor for progress. Once the display comes up you will
briefly see `Starting...`, then `Waiting for playback...`, then your album art.

<details>
<summary><strong>Alternative: run on wokwi.com in the browser</strong></summary>

1. Go to [wokwi.com](https://wokwi.com) and create a new ESP32 project.
2. Upload the contents of the `wokwi/` folder.
3. Start the simulation.
</details>

## Manual test checklist

- [ ] Wi-Fi connects and serial shows an IP address
- [ ] Access token refresh succeeds (`Access token refreshed.`)
- [ ] Currently playing track appears with album art and text
- [ ] Skipping to another song updates the display within ~5 s
- [ ] Pausing/stopping shows the idle **Nothing playing** screen
- [ ] Serial log does not re-fetch art while the same track keeps playing
- [ ] Long track/artist names are truncated rather than overflowing their bars

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Simulator refuses to start | `wokwi/build/wokwi.ino.bin` missing — run the build script |
| Simulator refuses to start | You opened the repo root instead of `wokwi-simulator.code-workspace` |
| Screen stays blank, serial silent | Firmware halted in `setup()` — Wi-Fi or auth failed; check serial |
| `Setup stopped: Spotify auth failed.` | Bad refresh token or Client ID/Secret in `secrets.h` |
| `Token refresh HTTP 400` | Refresh token revoked or copied incorrectly; re-run `spotify_auth.py` |
| **Nothing playing** while music is on | Playing from an account that isn't allowlisted, or not Premium |
| Garbled / mirrored display | `tft_setup.h` not picked up — ensure `build_opt.h` exists and rebuild with `--clean` |
| `Image too large` in serial | Art exceeded the 96 KB buffer (`MAX_IMAGE_BYTES`) |

## Handoff to real hardware (CYD / ESP32-2432S028R)

When you move off Wokwi:

1. Install the **CH340** USB-serial driver on Windows and confirm the COM port.
2. Update `tft_setup.h` to the CYD pin map (the current values match the Wokwi
   wiring in `diagram.json`, and `TFT_RST` is `-1` because Wokwi does not simulate it).
3. Change `WIFI_SSID` / `WIFI_PASSWORD` in `secrets.h` to your home **2.4 GHz**
   network — the ESP32 does not support 5 GHz.
4. Flash a display test sketch first to confirm wiring, then this firmware.

The Spotify logic (auth, polling, parsing, JPEG decode/draw) stays unchanged.

## Security note

Wokwi's free Public Gateway can inspect outbound HTTPS traffic, and the firmware
calls `setInsecure()` (no certificate validation) for the simulator. Treat simulator
credentials as **burnable** and rotate your Spotify Client Secret after the simulator
phase. See [docs/credentials.md](docs/credentials.md).

## Reference

- Build brief: [spotify-album-display-wokwi-brief.md](spotify-album-display-wokwi-brief.md)
- Required Spotify scopes: `user-read-currently-playing`, `user-read-playback-state`
