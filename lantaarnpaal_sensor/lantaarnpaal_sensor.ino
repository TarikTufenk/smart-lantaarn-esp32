#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

// ---------------- Config ----------------
#define STATUS_LED_PIN LED_BUILTIN
#define PIR_PIN 4

const char *WIFI_SSID = "lampding";
const char *WIFI_PASS = "lamp1234";

#define DISCOVERY_PORT 3091
#define WS_PORT 3092

// ---------------- Globals ----------------
WiFiUDP udp;
WebSocketsClient ws;
Preferences prefs;

String sensorId = "";
bool serverDiscovered = false;
bool wsAuthorized = false;

// PIR
bool lastPirState = false;
unsigned long lastTrigger = 0;
const unsigned long PIR_COOLDOWN_MS = 3000;

unsigned long lastActiveSend = 0;
const unsigned long ACTIVE_INTERVAL_MS = 1000;

// LED
enum StatusMode
{
  STATUS_WIFI,
  STATUS_DISCOVERY,
  STATUS_WS,
  STATUS_NORMAL
};
StatusMode statusMode = STATUS_WIFI;

unsigned long lastBlink = 0;
bool blinkOn = false;

// PIR LED pulse
bool pirLedActive = false;
unsigned long pirLedUntil = 0;
const unsigned long PIR_LED_MS = 1000;

// ---------------- Preferences ----------------
String loadSensorId()
{
  prefs.begin("sensor", true);
  String id = prefs.getString("id", "");
  prefs.end();
  return id;
}

void saveSensorId(const String &id)
{
  prefs.begin("sensor", false);
  prefs.putString("id", id);
  prefs.end();
}

// ---------------- Helpers ----------------
void setLed(bool on)
{
  digitalWrite(STATUS_LED_PIN, on ? HIGH : LOW);
}

void sendJson(JsonDocument &doc)
{
  String msg;
  serializeJson(doc, msg);
  Serial.print("[WS →] ");
  Serial.println(msg);
  ws.sendTXT(msg);
}

// ---------------- WebSocket ----------------
void onWebSocketEvent(WStype_t type, uint8_t *payload, size_t length)
{

  if (type == WStype_CONNECTED)
  {
    Serial.println("[WS] Connected");
    statusMode = STATUS_WS;
    wsAuthorized = false;

    sensorId = loadSensorId();

    StaticJsonDocument<128> out;
    if (sensorId.length() == 0)
    {
      out["type"] = "request_sensor_id";
    }
    else
    {
      out["type"] = "authorize_sensor";
      out["id"] = sensorId;
    }

    sendJson(out);
    return;
  }

  if (type == WStype_DISCONNECTED)
  {
    wsAuthorized = false;
    statusMode = STATUS_WS;
  }

  if (type != WStype_TEXT)
    return;

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, payload, length))
    return;

  const char *msgType = doc["type"];

  Serial.print("[WS ←] ");
  Serial.println(msgType);

  if (strcmp(msgType, "assigned_sensor_id") == 0)
  {
    sensorId = doc["id"].as<String>();
    saveSensorId(sensorId);

    StaticJsonDocument<128> auth;
    auth["type"] = "authorize_sensor";
    auth["id"] = sensorId;
    sendJson(auth);
  }

  else if (strcmp(msgType, "authorized_sensor") == 0)
  {
    Serial.println("[AUTH] Authorized");
    wsAuthorized = true;
    statusMode = STATUS_NORMAL;
    setLed(false);
  }
}

// ---------------- Discovery ----------------
void startDiscovery()
{
  udp.begin(DISCOVERY_PORT);
  statusMode = STATUS_DISCOVERY;
  Serial.println("[DISCOVERY] Listening");
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
  Serial.print("[DISCOVERY] Found server ");
  Serial.println(serverIp);

  serverDiscovered = true;
  udp.stop();

  ws.begin(serverIp.c_str(), WS_PORT, "/");
  ws.onEvent(onWebSocketEvent);
  ws.setReconnectInterval(5000);
  ws.enableHeartbeat(15000, 3000, 2);

  statusMode = STATUS_WS;
}

// ---------------- PIR ----------------
void handlePir()
{
  if (!wsAuthorized)
    return;

  bool pirState = digitalRead(PIR_PIN);
  unsigned long now = millis();

  // Rising edge → direct event
  if (pirState && !lastPirState)
  {
    lastActiveSend = now;
    lastTrigger = now;

    StaticJsonDocument<128> evt;
    evt["type"] = "sensor_activate";
    evt["id"] = sensorId;
    sendJson(evt);

    pirLedActive = true;
    pirLedUntil = now + PIR_LED_MS;
    setLed(true);
  }

  // While motion active → every 1s
  if (pirState)
  {
    if (now - lastActiveSend >= ACTIVE_INTERVAL_MS)
    {
      lastActiveSend = now;

      StaticJsonDocument<128> evt;
      evt["type"] = "sensor_activate";
      evt["id"] = sensorId;
      sendJson(evt);
    }
  }

  // Falling edge → stop only (log)
  if (!pirState && lastPirState)
  {
    Serial.println("[PIR] Motion stopped");
  }

  lastPirState = pirState;
}

// ---------------- LED ----------------
void handlePirLed()
{
  if (!pirLedActive)
    return;

  if (millis() > pirLedUntil)
  {
    pirLedActive = false;
    if (statusMode == STATUS_NORMAL)
      setLed(false);
  }
}

void handleStatusLed()
{
  if (pirLedActive)
    return;
  if (statusMode == STATUS_NORMAL)
    return;

  if (millis() - lastBlink > 400)
  {
    lastBlink = millis();
    blinkOn = !blinkOn;

    if (!blinkOn)
    {
      setLed(false);
      return;
    }

    if (statusMode == STATUS_WIFI)
    {
      setLed(true);
    }
    else if (statusMode == STATUS_DISCOVERY)
    {
      setLed(true);
    }
    else if (statusMode == STATUS_WS)
    {
      setLed(true);
    }
  }
}

// ---------------- Setup / Loop ----------------
void setup()
{
  Serial.begin(115200);
  delay(200);

  pinMode(STATUS_LED_PIN, OUTPUT);
  pinMode(PIR_PIN, INPUT);

  Serial.println();
  Serial.println("=== SENSOR BOOT ===");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(false);

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  statusMode = STATUS_WIFI;

  while (WiFi.status() != WL_CONNECTED)
  {
    handleStatusLed();
    delay(50);
  }

  Serial.print("[WIFI] Connected, IP=");
  Serial.println(WiFi.localIP());

  startDiscovery();
}

void loop()
{
  handleDiscovery();
  ws.loop();
  handlePir();
  handlePirLed();
  handleStatusLed();
}
