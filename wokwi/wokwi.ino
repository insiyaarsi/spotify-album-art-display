#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TJpg_Decoder.h>

#include "tft_setup.h"
#include <TFT_eSPI.h>

#include "secrets.h"

// --- Layout (ILI9341 240x320) ---
static const int TOP_BAR_H = 40;
static const int BOTTOM_BAR_H = 40;
static const size_t MAX_IMAGE_BYTES = 98304;

static int screenW = 240;
static int screenH = 320;
static int artY = TOP_BAR_H;
static int artSize = 240;
static int bottomBarY = 280;

static int artDrawX = 0;
static int artDrawY = 0;

static String fallbackImageUrl;

static const unsigned long POLL_INTERVAL_MS = 5000;
static const unsigned long TOKEN_REFRESH_MARGIN_MS = 5UL * 60UL * 1000UL;
static const int HTTPS_RETRY_COUNT = 3;

static const char *TOKEN_URL = "https://accounts.spotify.com/api/token";
static const char *NOW_PLAYING_URL =
    "https://api.spotify.com/v1/me/player/currently-playing";

enum class PlaybackResult { Ok, NothingPlaying, Error };

TFT_eSPI tft = TFT_eSPI();
WiFiClientSecure secureClient;

String accessToken;
unsigned long tokenExpiresAtMs = 0;

String lastTrackId;
String currentTrackName;
String currentArtistName;
String currentImageUrl;

bool nothingPlaying = false;

// --- Display helpers ---

void updateLayoutMetrics() {
  screenW = tft.width();
  screenH = tft.height();
  artY = TOP_BAR_H;
  bottomBarY = screenH - BOTTOM_BAR_H;
  artSize = bottomBarY - artY;
  Serial.printf("Display layout: %dx%d, art=%dx%d at y=%d, bottom bar y=%d\n",
                screenW, screenH, screenW, artSize, artY, bottomBarY);
}

void drawStatusMessage(const char *message) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(message, screenW / 2, screenH / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawTopBar(const String &trackName) {
  tft.fillRect(0, 0, screenW, TOP_BAR_H, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(trackName, screenW / 2, TOP_BAR_H / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawBottomBar(const String &artistName) {
  tft.fillRect(0, bottomBarY, screenW, BOTTOM_BAR_H, TFT_NAVY);
  tft.setTextColor(TFT_WHITE, TFT_NAVY);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(artistName, screenW / 2, bottomBarY + BOTTOM_BAR_H / 2, 2);
  tft.setTextDatum(TL_DATUM);
}

void drawIdleScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("Nothing playing", screenW / 2, screenH / 2, 4);
  tft.setTextDatum(TL_DATUM);
}

String truncateText(const String &text, size_t maxLen) {
  if (text.length() <= maxLen) {
    return text;
  }
  return text.substring(0, maxLen - 3) + "...";
}

// --- JPEG decode callback ---

void configureArtPlacement(uint16_t imgW, uint16_t imgH) {
  const int imgSize = min(static_cast<int>(imgW), static_cast<int>(imgH));
  int scale = 1;

  if (imgSize < artSize) {
    if (imgSize <= 64) {
      scale = 4;  // 64 -> 256 px, fills art area with viewport crop
    } else if (imgSize <= 128) {
      scale = 2;
    }
  }

  TJpgDec.setJpgScale(scale);
  const int drawSize = imgSize * scale;
  artDrawX = (screenW - drawSize) / 2;
  artDrawY = artY + (artSize - drawSize) / 2;
}

bool tftJpgOutput(int16_t x, int16_t y, uint16_t w, uint16_t h,
                  uint16_t *bitmap) {
  if (y >= tft.height()) {
    return false;
  }
  tft.setViewport(0, artY, screenW, artSize);
  tft.pushImage(artDrawX + x, artDrawY + y, w, h, bitmap);
  tft.resetViewport();
  yield();
  return true;
}

// --- Wi-Fi ---

bool connectWiFi() {
  Serial.printf("Connecting to Wi-Fi: %s\n", WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    if (millis() - start > 30000) {
      Serial.println("Wi-Fi connection timed out.");
      return false;
    }
  }

  Serial.print("Wi-Fi connected. IP: ");
  Serial.println(WiFi.localIP());
  return true;
}

// --- HTTPS helpers ---

bool httpsPostForm(const char *url, const String &body, int &httpCode,
                   String &response, bool useSpotifyBasicAuth = false) {
  for (int attempt = 0; attempt < HTTPS_RETRY_COUNT; attempt++) {
    HTTPClient http;
    http.setTimeout(15000);
    http.begin(secureClient, url);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    if (useSpotifyBasicAuth) {
      http.setAuthorization(SPOTIFY_CLIENT_ID, SPOTIFY_CLIENT_SECRET);
    }

    httpCode = http.POST(body);
    response = http.getString();
    http.end();
    secureClient.stop();

    if (httpCode > 0) {
      return true;
    }

    Serial.printf("HTTPS POST failed (attempt %d): %s\n", attempt + 1, url);
    delay(500 * (attempt + 1));
  }
  return false;
}

bool httpsGet(const char *url, const String &bearerToken, int &httpCode,
              String &response) {
  for (int attempt = 0; attempt < HTTPS_RETRY_COUNT; attempt++) {
    HTTPClient http;
    http.setTimeout(15000);
    http.begin(secureClient, url);
    http.addHeader("Authorization", "Bearer " + bearerToken);

    httpCode = http.GET();
    response = http.getString();
    http.end();
    secureClient.stop();

    if (httpCode > 0) {
      return true;
    }

    Serial.printf("HTTPS GET failed (attempt %d): %s\n", attempt + 1, url);
    delay(500 * (attempt + 1));
  }
  return false;
}

bool httpsGetBytes(const char *url, uint8_t **outData, size_t *outLen) {
  for (int attempt = 0; attempt < HTTPS_RETRY_COUNT; attempt++) {
    HTTPClient http;
    http.setTimeout(20000);
    http.begin(secureClient, url);

    const int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("Image GET HTTP %d (attempt %d): %s\n", httpCode, attempt + 1,
                    url);
      http.end();
      secureClient.stop();
      delay(500 * (attempt + 1));
      continue;
    }

    WiFiClient *stream = http.getStreamPtr();
    uint8_t *buffer =
        static_cast<uint8_t *>(malloc(MAX_IMAGE_BYTES));
    if (buffer == nullptr) {
      Serial.println("Failed to allocate image buffer.");
      http.end();
      secureClient.stop();
      return false;
    }

    const int contentLength = http.getSize();
    size_t offset = 0;
    const unsigned long readStart = millis();

    if (contentLength > 0) {
      if (static_cast<size_t>(contentLength) > MAX_IMAGE_BYTES) {
        Serial.printf("Image too large: %d bytes\n", contentLength);
        free(buffer);
        http.end();
        secureClient.stop();
        return false;
      }

      while (offset < static_cast<size_t>(contentLength)) {
        const int readBytes = stream->readBytes(
            buffer + offset, static_cast<size_t>(contentLength) - offset);
        if (readBytes <= 0) {
          if (millis() - readStart > 20000) {
            break;
          }
          delay(1);
          continue;
        }
        offset += readBytes;
      }

      if (offset != static_cast<size_t>(contentLength)) {
        free(buffer);
        http.end();
        secureClient.stop();
        Serial.println("Incomplete image download.");
        delay(500 * (attempt + 1));
        continue;
      }
    } else {
      while (http.connected() || stream->available()) {
        if (offset >= MAX_IMAGE_BYTES) {
          Serial.println("Image exceeds max buffer while streaming.");
          break;
        }

        if (stream->available()) {
          const size_t chunk =
              min(static_cast<size_t>(stream->available()),
                  MAX_IMAGE_BYTES - offset);
          offset += stream->readBytes(buffer + offset, chunk);
        } else if (!http.connected()) {
          break;
        } else if (millis() - readStart > 20000) {
          break;
        } else {
          delay(1);
        }
      }
    }

    http.end();
    secureClient.stop();

    if (offset == 0) {
      free(buffer);
      Serial.println("Empty image response.");
      delay(500 * (attempt + 1));
      continue;
    }

    uint8_t *rightSized = static_cast<uint8_t *>(realloc(buffer, offset));
    if (rightSized != nullptr) {
      buffer = rightSized;
    }

    *outData = buffer;
    *outLen = offset;
    Serial.printf("Downloaded image: %u bytes\n", static_cast<unsigned>(offset));
    return true;
  }

  return false;
}

// --- Spotify auth ---

bool refreshAccessToken() {
  Serial.println("Refreshing Spotify access token...");

  const String body = String("grant_type=refresh_token&refresh_token=") +
                      SPOTIFY_REFRESH_TOKEN;

  int httpCode = 0;
  String response;
  if (!httpsPostForm(TOKEN_URL, body, httpCode, response, true)) {
    Serial.println("Token refresh request failed.");
    return false;
  }

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Token refresh HTTP %d: %s\n", httpCode, response.c_str());
    return false;
  }

  StaticJsonDocument<768> doc;
  const DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.printf("Token JSON parse error: %s\n", err.c_str());
    return false;
  }

  const char *token = doc["access_token"];
  const long expiresIn = doc["expires_in"] | 3600;
  if (token == nullptr) {
    Serial.println("No access_token in response.");
    return false;
  }

  accessToken = token;
  tokenExpiresAtMs = millis() + (static_cast<unsigned long>(expiresIn) * 1000UL);
  Serial.println("Access token refreshed.");
  return true;
}

bool ensureAccessToken() {
  if (accessToken.length() == 0 ||
      millis() + TOKEN_REFRESH_MARGIN_MS >= tokenExpiresAtMs) {
    return refreshAccessToken();
  }
  return true;
}

// --- Spotify playback parsing ---

String pickAlbumImageUrl(JsonArray images) {
  const char *url300 = nullptr;
  const char *url64 = nullptr;
  const char *bestUrl = nullptr;
  int bestHeight = 0;

  fallbackImageUrl = "";

  for (JsonObject image : images) {
    const int height = image["height"] | 0;
    const char *url = image["url"];
    if (url == nullptr || height <= 0) {
      continue;
    }
    if (height == 300) {
      url300 = url;
    }
    if (height == 64) {
      url64 = url;
    }
    if (height <= 300 && height > bestHeight) {
      bestHeight = height;
      bestUrl = url;
    }
  }

  if (url64 != nullptr) {
    fallbackImageUrl = url64;
  }

  if (url300 != nullptr) {
    return String(url300);
  }
  if (bestUrl != nullptr) {
    return String(bestUrl);
  }
  if (url64 != nullptr) {
    return String(url64);
  }
  return String();
}

String joinArtistNames(JsonArray artists) {
  String result;
  for (JsonObject artist : artists) {
    const char *name = artist["name"];
    if (name == nullptr) {
      continue;
    }
    if (result.length() > 0) {
      result += ", ";
    }
    result += name;
  }
  return result;
}

PlaybackResult fetchCurrentlyPlaying(String &trackId, String &trackName,
                                     String &artistName, String &imageUrl) {
  if (!ensureAccessToken()) {
    return PlaybackResult::Error;
  }

  int httpCode = 0;
  String response;
  if (!httpsGet(NOW_PLAYING_URL, accessToken, httpCode, response)) {
    return PlaybackResult::Error;
  }

  if (httpCode == 401) {
    accessToken = "";
    if (!refreshAccessToken()) {
      return PlaybackResult::Error;
    }
    if (!httpsGet(NOW_PLAYING_URL, accessToken, httpCode, response)) {
      return PlaybackResult::Error;
    }
  }

  if (httpCode == 204 || response.length() == 0) {
    return PlaybackResult::NothingPlaying;
  }

  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("Now playing HTTP %d: %s\n", httpCode, response.c_str());
    return PlaybackResult::Error;
  }

  StaticJsonDocument<4096> doc;
  const DeserializationError err = deserializeJson(doc, response);
  if (err) {
    Serial.printf("Playback JSON parse error: %s\n", err.c_str());
    return PlaybackResult::Error;
  }

  JsonObject item = doc["item"];
  if (item.isNull()) {
    return PlaybackResult::NothingPlaying;
  }

  const char *id = item["id"];
  const char *name = item["name"];
  if (id == nullptr || name == nullptr) {
    return PlaybackResult::NothingPlaying;
  }

  trackId = id;
  trackName = name;
  artistName = joinArtistNames(item["artists"].as<JsonArray>());
  imageUrl = pickAlbumImageUrl(item["album"]["images"].as<JsonArray>());
  return PlaybackResult::Ok;
}

// --- Rendering ---

bool decodeAndDrawJpeg(uint8_t *jpegData, size_t jpegLen) {
  uint16_t imgW = 0;
  uint16_t imgH = 0;
  if (TJpgDec.getJpgSize(&imgW, &imgH, jpegData, jpegLen) != JDR_OK) {
    Serial.println("Failed to read JPEG dimensions.");
    return false;
  }

  configureArtPlacement(imgW, imgH);
  Serial.printf("Album art: %ux%u -> draw at (%d, %d)\n", imgW, imgH, artDrawX,
                artDrawY);

  const JRESULT result = TJpgDec.drawJpg(0, 0, jpegData, jpegLen);
  if (result != JDR_OK) {
    Serial.printf("JPEG decode failed: %d\n", result);
    return false;
  }

  return true;
}

bool drawAlbumArtFromUrl(const String &imageUrl) {
  if (imageUrl.length() == 0) {
    return false;
  }

  uint8_t *jpegData = nullptr;
  size_t jpegLen = 0;
  if (!httpsGetBytes(imageUrl.c_str(), &jpegData, &jpegLen)) {
    return false;
  }

  const bool ok = decodeAndDrawJpeg(jpegData, jpegLen);
  free(jpegData);
  return ok;
}

bool drawAlbumArt(const String &imageUrl) {
  if (imageUrl.length() == 0) {
    Serial.println("No album image URL.");
    return false;
  }

  tft.fillRect(0, artY, screenW, artSize, TFT_BLACK);

  Serial.printf("Downloading album art: %s\n", imageUrl.c_str());
  if (drawAlbumArtFromUrl(imageUrl)) {
    return true;
  }

  if (fallbackImageUrl.length() > 0 && fallbackImageUrl != imageUrl) {
    Serial.println("Retrying album art with 64px fallback...");
    tft.fillRect(0, artY, screenW, artSize, TFT_BLACK);
    if (drawAlbumArtFromUrl(fallbackImageUrl)) {
      return true;
    }
  }

  Serial.println("Failed to download album art.");
  return false;
}

void renderNowPlaying(const String &trackName, const String &artistName,
                      const String &imageUrl) {
  tft.resetViewport();
  drawTopBar(truncateText(trackName, 28));
  if (!drawAlbumArt(imageUrl)) {
    tft.fillRect(0, artY, screenW, artSize, TFT_DARKGREY);
    tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Art unavailable", screenW / 2, artY + artSize / 2, 2);
    tft.setTextDatum(TL_DATUM);
  }
  drawBottomBar(truncateText(artistName, 28));
}

void updatePlaybackState() {
  String trackId;
  String trackName;
  String artistName;
  String imageUrl;

  const PlaybackResult result =
      fetchCurrentlyPlaying(trackId, trackName, artistName, imageUrl);

  if (result == PlaybackResult::Error) {
    Serial.println("Playback poll failed.");
    return;
  }

  if (result == PlaybackResult::NothingPlaying) {
    if (!nothingPlaying || lastTrackId.length() > 0) {
      Serial.println("Nothing currently playing.");
      nothingPlaying = true;
      lastTrackId = "";
      drawIdleScreen();
    }
    return;
  }

  nothingPlaying = false;

  if (trackId == lastTrackId) {
    return;
  }

  Serial.printf("Track changed: %s — %s\n", artistName.c_str(),
                trackName.c_str());
  lastTrackId = trackId;
  currentTrackName = trackName;
  currentArtistName = artistName;
  currentImageUrl = imageUrl;
  renderNowPlaying(currentTrackName, currentArtistName, currentImageUrl);
}

// --- Arduino lifecycle ---

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("Spotify Album Display booting...");
  Serial.printf("Free heap: %u bytes\n", ESP.getFreeHeap());

  secureClient.setInsecure();
  secureClient.setTimeout(15000);

  if (!connectWiFi()) {
    Serial.println("Setup stopped: Wi-Fi failed.");
    return;
  }

  Serial.println("Authenticating with Spotify...");
  if (!refreshAccessToken()) {
    Serial.println("Setup stopped: Spotify auth failed.");
    return;
  }

  Serial.println("Initializing display...");
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  updateLayoutMetrics();
  drawStatusMessage("Starting...");
  Serial.printf("Free heap after display init: %u bytes\n", ESP.getFreeHeap());

  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tftJpgOutput);

  drawStatusMessage("Waiting for\nplayback...");
  Serial.println("Polling currently playing track...");
  updatePlaybackState();
}

void loop() {
  static unsigned long lastPollMs = 0;
  const unsigned long now = millis();

  if (now - lastPollMs >= POLL_INTERVAL_MS) {
    lastPollMs = now;
    updatePlaybackState();
  }

  delay(50);
}
