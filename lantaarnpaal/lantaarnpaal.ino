#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>

#define LED_PIN 5
#define LED_COUNT 1

const char *WIFI_SSID = "lampding";
const char *WIFI_PASS = "lamp1234";

#define DISCOVERY_PORT 3091
#define WS_PORT 3090

Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
WebSocketsClient ws;
WiFiUDP udp;
Preferences prefs;

String lampId = "";
bool serverDiscovered = false;
bool wsAuthorized = false;

bool identifying = false;
unsigned long identifyUntil = 0;
unsigned long lastBlink = 0;
bool blinkOn = false;

bool currentOn = false;
uint8_t currentBrightness = 0;
uint32_t currentColor = 0;

bool prevOn = false;
uint8_t prevBrightness = 0;
uint32_t prevColor = 0;

enum StatusMode
{
  STATUS_WIFI,
  STATUS_DISCOVERY,
  STATUS_WS,
  STATUS_NORMAL
};

StatusMode statusMode = STATUS_WIFI;

String loadLampId()
{
  prefs.begin("lamp", true);
  String id = prefs.getString("id", "");
  prefs.end();
  return id;
}

void saveLampId(const String &id)
{
  prefs.begin("lamp", false);
  prefs.putString("id", id);
  prefs.end();
}

void applyState(bool on, uint8_t brightness, uint32_t color)
{
  currentOn = on;
  currentBrightness = brightness;
  currentColor = color;
  statusMode = STATUS_NORMAL;

  if (!on || brightness == 0)
  {
    led.clear();
    led.show();
    return;
  }

  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;

  led.clear();
  led.setBrightness(brightness);
  led.setPixelColor(0, r, g, b);
  led.show();
}

void startIdentify(uint32_t durationMs)
{
  prevOn = currentOn;
  prevBrightness = currentBrightness;
  prevColor = currentColor;

  identifying = true;
  identifyUntil = millis() + durationMs;
  lastBlink = 0;
  blinkOn = false;
}

void sendJson(JsonDocument &doc)
{
  String msg;
  serializeJson(doc, msg);
  ws.sendTXT(msg);
}

void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  if (type == WStype_CONNECTED)
  {
    wsAuthorized = false;
    statusMode = STATUS_WS;

    lampId = loadLampId();

    StaticJsonDocument<128> out;
    out["type"] = lampId.length() == 0 ? "request_id" : "authorize";
    if (lampId.length())
      out["id"] = lampId;

    sendJson(out);
  }

  if (type != WStype_TEXT)
    return;

  StaticJsonDocument<512> doc;
  if (deserializeJson(doc, payload, length))
    return;

  const char *msgType = doc["type"];

  if (strcmp(msgType, "assigned_id") == 0)
  {
    lampId = doc["id"].as<String>();
    saveLampId(lampId);

    StaticJsonDocument<128> auth;
    auth["type"] = "authorize";
    auth["id"] = lampId;
    sendJson(auth);
  }

  else if (strcmp(msgType, "authorized") == 0)
  {
    wsAuthorized = true;
    statusMode = STATUS_NORMAL;
    led.clear();
    led.show();
  }

  else if (
      (strcmp(msgType, "activated") == 0 || strcmp(msgType, "state") == 0) && !identifying)
  {
    JsonObject state = doc["state"];
    const char *colorStr = state["color"] | "#000000";
    uint32_t color = strtol(colorStr + 1, NULL, 16);

    applyState(
        state["on"],
        state["brightness"],
        color);
  }

  else if (strcmp(msgType, "identify") == 0)
  {
    const char *targetId = doc["id"];
    uint32_t duration = doc["durationMs"].isNull()
                            ? 3000
                            : doc["durationMs"].as<uint32_t>();

    if (lampId == targetId)
    {
      startIdentify(duration);
    }
  }
}

void startDiscovery()
{
  udp.begin(DISCOVERY_PORT);
  statusMode = STATUS_DISCOVERY;
}

void handleDiscovery()
{
  if (serverDiscovered)
    return;

  int packetSize = udp.parsePacket();
  if (packetSize <= 0)
    return;

  char buffer[256];
  int len = udp.read(buffer, sizeof(buffer) - 1);
  buffer[len] = 0;

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, buffer))
    return;
  if (strcmp(doc["type"], "lamp_server_announce") != 0)
    return;

  String serverIp = udp.remoteIP().toString();
  serverDiscovered = true;
  udp.stop();

  ws.begin(serverIp.c_str(), WS_PORT, "/");
  ws.onEvent(onWebSocketEvent);
  ws.setReconnectInterval(5000);

  statusMode = STATUS_WS;
}

void handleIdentify()
{
  if (!identifying)
    return;

  if (millis() > identifyUntil)
  {
    identifying = false;
    applyState(prevOn, prevBrightness, prevColor);
    return;
  }

  if (millis() - lastBlink > 300)
  {
    lastBlink = millis();
    blinkOn = !blinkOn;

    led.clear();
    led.setBrightness(255);
    if (blinkOn)
    {
      led.setPixelColor(0, 255, 255, 255);
    }
    led.show();
  }
}

void handleStatusLed()
{
  if (statusMode == STATUS_NORMAL || identifying)
    return;

  if (millis() - lastBlink > 400)
  {
    lastBlink = millis();
    blinkOn = !blinkOn;

    led.clear();
    led.setBrightness(120);

    if (!blinkOn)
    {
      led.show();
      return;
    }

    if (statusMode == STATUS_WIFI)
    {
      led.setPixelColor(0, 0, 0, 255);
    }
    else if (statusMode == STATUS_DISCOVERY)
    {
      led.setPixelColor(0, 0, 255, 0);
    }
    else if (statusMode == STATUS_WS)
    {
      led.setPixelColor(0, 128, 0, 128);
    }

    led.show();
  }
}

void setup()
{
  led.begin();
  led.show();

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  statusMode = STATUS_WIFI;

  while (WiFi.status() != WL_CONNECTED)
  {
    handleStatusLed();
    delay(50);
  }

  startDiscovery();
}

void loop()
{
  handleDiscovery();
  ws.loop();
  handleIdentify();
  handleStatusLed();
}
