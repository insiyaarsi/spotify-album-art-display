#!/usr/bin/env python3
"""One-time Authorization Code flow to obtain a Spotify refresh token."""

from __future__ import annotations

import os
import sys

from dotenv import load_dotenv
from spotipy.oauth2 import SpotifyOAuth

SCOPES = "user-read-currently-playing user-read-playback-state"
REDIRECT_URI = "http://127.0.0.1:8888/callback"


def main() -> int:
    load_dotenv()

    client_id = os.getenv("SPOTIFY_CLIENT_ID")
    client_secret = os.getenv("SPOTIFY_CLIENT_SECRET")

    if not client_id or not client_secret:
        print("Set SPOTIFY_CLIENT_ID and SPOTIFY_CLIENT_SECRET in .env first.")
        return 1

    auth = SpotifyOAuth(
        client_id=client_id,
        client_secret=client_secret,
        redirect_uri=REDIRECT_URI,
        scope=SCOPES,
        open_browser=True,
        cache_path=".spotify_cache",
    )

    token_info = auth.get_access_token(as_dict=True, check_cache=False)
    refresh_token = token_info.get("refresh_token")

    if not refresh_token:
        print("No refresh token returned. Try revoking app access and rerunning.")
        return 1

    print()
    print("Authorization succeeded.")
    print("Add this to your .env file:")
    print(f"SPOTIFY_REFRESH_TOKEN={refresh_token}")
    print()
    print("Then copy the same values into wokwi/secrets.h.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
