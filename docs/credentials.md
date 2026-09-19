# Spotify Credentials Setup

This project uses a one-time OAuth Authorization Code flow on your PC. The ESP32 firmware only stores the refresh token and exchanges it for short-lived access tokens.

## Prerequisites

- A [Spotify Developer](https://developer.spotify.com/dashboard) account
- **Spotify Premium** on the account that will use the device
- Your account allowlisted in the app's Development Mode settings

## 1. Create the Spotify app

1. Open the [Spotify Developer Dashboard](https://developer.spotify.com/dashboard).
2. Create an app and note the **Client ID** and **Client Secret**.
3. Add Redirect URI: `http://127.0.0.1:8888/callback`
4. Under Development Mode, allowlist your Spotify account email.

## 2. Configure local environment

```bash
cp .env.example .env
```

Edit `.env` and set `SPOTIFY_CLIENT_ID` and `SPOTIFY_CLIENT_SECRET`.

Install Python dependencies:

```bash
python -m venv .venv
.venv\Scripts\activate
pip install -r scripts/requirements.txt
```

## 3. Obtain a refresh token (one time)

```bash
python scripts/spotify_auth.py
```

This opens a browser login. After approval, copy the printed refresh token into `.env`:

```
SPOTIFY_REFRESH_TOKEN=...
```

## 4. Validate before flashing firmware

Start playback on Spotify, then run:

```bash
python scripts/spotify_validate.py
```

You should see the current track ID, title, artist, and 64×64 album-art URL.

## 5. Copy credentials into firmware

Copy `wokwi/secrets.example.h` to `wokwi/secrets.h` and fill in the same three values:

- `SPOTIFY_CLIENT_ID`
- `SPOTIFY_CLIENT_SECRET`
- `SPOTIFY_REFRESH_TOKEN`

`wokwi/secrets.h` is gitignored and must never be committed.

## Wokwi security note

Wokwi's free **Public Gateway** can inspect outbound HTTPS traffic. Treat simulator credentials as **burnable**:

- Regenerate the Client Secret after the simulator phase, or
- Use Wokwi's paid Private Gateway if you need local-only traffic

## Required scopes

- `user-read-currently-playing`
- `user-read-playback-state`

Playback control scopes are not needed for v1 (art + text only).
