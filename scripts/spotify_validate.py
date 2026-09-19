#!/usr/bin/env python3
"""Validate Spotify credentials and inspect currently-playing JSON shape."""

from __future__ import annotations

import os
import sys

import spotipy
from dotenv import load_dotenv
from spotipy.oauth2 import SpotifyOAuth

SCOPES = "user-read-currently-playing user-read-playback-state"
REDIRECT_URI = "http://127.0.0.1:8888/callback"


def pick_album_image_url(images: list[dict]) -> str | None:
    if not images:
        return None

    for image in images:
        if image.get("height") == 64:
            return image.get("url")

    smallest = min(images, key=lambda img: img.get("height") or 0)
    return smallest.get("url")


def main() -> int:
    load_dotenv()

    client_id = os.getenv("SPOTIFY_CLIENT_ID")
    client_secret = os.getenv("SPOTIFY_CLIENT_SECRET")
    refresh_token = os.getenv("SPOTIFY_REFRESH_TOKEN")

    missing = [
        name
        for name, value in [
            ("SPOTIFY_CLIENT_ID", client_id),
            ("SPOTIFY_CLIENT_SECRET", client_secret),
            ("SPOTIFY_REFRESH_TOKEN", refresh_token),
        ]
        if not value
    ]
    if missing:
        print("Missing required environment variables:", ", ".join(missing))
        print("Copy .env.example to .env and fill in your credentials.")
        print("Run scripts/spotify_auth.py first if you do not have a refresh token.")
        return 1

    auth = SpotifyOAuth(
        client_id=client_id,
        client_secret=client_secret,
        redirect_uri=REDIRECT_URI,
        scope=SCOPES,
        open_browser=False,
    )

    token_info = auth.refresh_access_token(refresh_token)
    access_token = token_info["access_token"]
    print("Access token refreshed successfully.")

    spotify = spotipy.Spotify(auth=access_token)
    playback = spotify.current_user_playing_track()

    if playback is None or playback.get("item") is None:
        print("Nothing is currently playing. Start playback on Spotify and retry.")
        return 0

    item = playback["item"]
    artists = ", ".join(artist["name"] for artist in item.get("artists", []))
    image_url = pick_album_image_url(item.get("album", {}).get("images", []))

    print()
    print("Currently playing:")
    print(f"  track_id : {item.get('id')}")
    print(f"  track    : {item.get('name')}")
    print(f"  artist(s): {artists}")
    print(f"  album    : {item.get('album', {}).get('name')}")
    print(f"  image_64 : {image_url}")
    print()
    print("Credentials look good. Copy the same values into wokwi/secrets.h.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
