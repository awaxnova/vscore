// Updated to work with maintained Async libs (mathieucarbou or esphome forks)
// Further updates: NimBLE callback signatures + advertising API; ArduinoJson v7 deprecations removed
// CHANGED lines are marked.

#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>                // CHANGED: include AsyncTCP *before* ESPAsyncWebServer for newer forks
#include <ESPAsyncWebServer.h>       // CHANGED: same header name; using maintained fork via platformio.ini
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "display.h"

// ---- Config ----
#ifndef GH_PAGES_ORIGIN
#define GH_PAGES_ORIGIN "*"
#endif
#ifndef SOFTAP_SSID
#define SOFTAP_SSID "ESP32-SCOREBOARD"
#endif
#ifndef SOFTAP_PASS
#define SOFTAP_PASS "volley123"
#endif

// BLE UUIDs (Nordic UART style)
static NimBLEUUID SERVICE_UUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static NimBLEUUID RX_CHAR_UUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e"); // write
static NimBLEUUID TX_CHAR_UUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e"); // notify

// Globals
AsyncWebServer server(80);
AsyncEventSource events("/api/v1/events");
DisplayRenderer* renderer = makeRenderer();

// Shared state
ScoreboardState S;
SemaphoreHandle_t stateMutex;

// BLE
NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pTx = nullptr;
NimBLECharacteristic* pRx = nullptr;
std::string bleRxBuffer;
volatile bool gPendingBroadcast = false;

// Forward decl
String stateToJson();
bool updateStateFromJson(const String& jsonStr, String* errorMsg);
void broadcastState();
void scheduleBroadcast();

// ---- Utilities ----
void withState(std::function<void(ScoreboardState&)> fn) {
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  fn(S);
  xSemaphoreGive(stateMutex);
}

String stateToJson() {
  JsonDocument doc;                                   // CHANGED: use JsonDocument (ArduinoJson v7)
  doc["type"] = "state";
  JsonObject data = doc["data"].to<JsonObject>();     // CHANGED: create nested object per v7
  xSemaphoreTake(stateMutex, portMAX_DELAY);
  data["ta"] = S.ta;
  data["tb"] = S.tb;
  data["ca"] = S.ca;
  data["cb"] = S.cb;
  data["abg"] = S.abg;
  data["bbg"] = S.bbg;
  data["a"] = S.a;
  data["b"] = S.b;
  data["sv"] = String(S.sv);
  data["set"] = S.set;
  data["ma"] = S.ma;
  data["mb"] = S.mb;
  data["bo"] = S.bo;
  data["ble"] = S.ble;
  // rotations
  {
    JsonArray ra = data["ra"].to<JsonArray>();
    for (int i=0;i<6;i++) ra.add(S.ra[i]);
    JsonArray rb = data["rb"].to<JsonArray>();
    for (int i=0;i<6;i++) rb.add(S.rb[i]);
    data["rsa"] = (int)S.rsa;
    data["rsb"] = (int)S.rsb;
  }
  // last 4 logs per team
  {
    JsonArray la = data["la"].to<JsonArray>();
    for (int i=0;i<S.laCount; i++) {
      JsonObject e = la.add<JsonObject>();
      e["reason"] = S.la[i].reason;
      e["scorer"] = S.la[i].scorer;
      e["ts"] = (uint32_t)S.la[i].ts;
    }
    JsonArray lb = data["lb"].to<JsonArray>();
    for (int i=0;i<S.lbCount; i++) {
      JsonObject e = lb.add<JsonObject>();
      e["reason"] = S.lb[i].reason;
      e["scorer"] = S.lb[i].scorer;
      e["ts"] = (uint32_t)S.lb[i].ts;
    }
  }
  xSemaphoreGive(stateMutex);

  String out;
  serializeJson(doc, out);
  return out;
}

bool applyDataObject(JsonObject data, String* err) {
  auto clamp = [](int v, int lo, int hi){ return v < lo ? lo : (v > hi ? hi : v); };
  // Names
  if (data["ta"].is<const char*>()) S.ta = String((const char*)data["ta"]).substring(0, 20);   // CHANGED: v7 style
  if (data["tb"].is<const char*>()) S.tb = String((const char*)data["tb"]).substring(0, 20);   // CHANGED
  // Colors (basic trust)
  if (data["ca"].is<const char*>()) S.ca = String((const char*)data["ca"]);                     // CHANGED
  if (data["cb"].is<const char*>()) S.cb = String((const char*)data["cb"]);                     // CHANGED
  if (data["abg"].is<const char*>()) S.abg = String((const char*)data["abg"]);
  if (data["bbg"].is<const char*>()) S.bbg = String((const char*)data["bbg"]);
  // Scores
  if (data["a"].is<int>()) S.a = clamp((int)data["a"], 0, 99);                                   // CHANGED
  if (data["b"].is<int>()) S.b = clamp((int)data["b"], 0, 99);                                   // CHANGED
  // Serving
  if (data["sv"].is<const char*>()) {                                                              // CHANGED
    const char* sv = data["sv"];
    if (sv && (sv[0]=='A' || sv[0]=='B')) S.sv = sv[0];
    else { if (err) *err = "sv must be 'A' or 'B'"; return false; }
  }
  // Rotations and current server
  if (data["ra"].is<JsonArray>()) {
    JsonArray ra = data["ra"].as<JsonArray>();
    for (int i=0;i<6;i++) {
      if (i < (int)ra.size() && ra[i].is<const char*>()) S.ra[i] = String((const char*)ra[i]);
      else S.ra[i] = String("");
    }
  }
  if (data["rb"].is<JsonArray>()) {
    JsonArray rb = data["rb"].as<JsonArray>();
    for (int i=0;i<6;i++) {
      if (i < (int)rb.size() && rb[i].is<const char*>()) S.rb[i] = String((const char*)rb[i]);
      else S.rb[i] = String("");
    }
  }
  if (data["rsa"].is<int>()) S.rsa = (uint8_t)clamp((int)data["rsa"], 0, 5);
  if (data["rsb"].is<int>()) S.rsb = (uint8_t)clamp((int)data["rsb"], 0, 5);
  // Last 4 logs per team (keep only the tail)
  if (data["la"].is<JsonArray>()) {
    JsonArray la = data["la"].as<JsonArray>();
    int n = (int)la.size();
    int start = n > 4 ? n - 4 : 0;
    S.laCount = 0;
    for (int i=start; i<n && S.laCount < 4; i++) {
      JsonObject e = la[i].as<JsonObject>();
      S.la[S.laCount].reason = String((const char*)(e["reason"] | ""));
      S.la[S.laCount].scorer = String((const char*)(e["scorer"] | ""));
      S.la[S.laCount].ts = (uint32_t)(e["ts"] | 0);
      S.laCount++;
    }
  }
  if (data["lb"].is<JsonArray>()) {
    JsonArray lb = data["lb"].as<JsonArray>();
    int n = (int)lb.size();
    int start = n > 4 ? n - 4 : 0;
    S.lbCount = 0;
    for (int i=start; i<n && S.lbCount < 4; i++) {
      JsonObject e = lb[i].as<JsonObject>();
      S.lb[S.lbCount].reason = String((const char*)(e["reason"] | ""));
      S.lb[S.lbCount].scorer = String((const char*)(e["scorer"] | ""));
      S.lb[S.lbCount].ts = (uint32_t)(e["ts"] | 0);
      S.lbCount++;
    }
  }
  // Set / Match / Best-of
  if (data["set"].is<int>()) S.set = clamp((int)data["set"], 1, 9);                               // CHANGED
  if (data["ma"].is<int>())  S.ma  = clamp((int)data["ma"], 0, 9);                               // CHANGED
  if (data["mb"].is<int>())  S.mb  = clamp((int)data["mb"], 0, 9);                               // CHANGED
  if (data["bo"].is<int>()) {                                                                      // CHANGED
    int bo = (int)data["bo"];
    if (bo==3 || bo==5) S.bo = bo;
    else { if (err) *err = "bo must be 3 or 5"; return false; }
  }
  return true;
}

bool updateStateFromJson(const String& jsonStr, String* errorMsg) {
  JsonDocument doc;                                      // CHANGED: v7 style
  DeserializationError e = deserializeJson(doc, jsonStr);
  if (e) {
    if (e == DeserializationError::IncompleteInput) {
      if (errorMsg) *errorMsg = "incomplete";
      return false; // caller can accumulate more data
    }
    if (errorMsg) *errorMsg = String("parse error: ") + e.c_str();
    return false;
  }
  const char* type = doc["type"] | "state";
  if (strcmp(type, "state") == 0) {
    JsonObject data = doc["data"].as<JsonObject>();
    String err;
    bool ok = false;
    xSemaphoreTake(stateMutex, portMAX_DELAY);
    ok = applyDataObject(data, &err);
    xSemaphoreGive(stateMutex);
    if (!ok) {
      if (errorMsg) *errorMsg = err;
      return false;
    }
    return true;
  }
  // ignore other types
  return true;
}

// ---- BLE callbacks ----
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo& connInfo) override {  // CHANGED: signature uses NimBLEConnInfo in newer NimBLE
    std::string v = c->getValue();
    if (v.empty()) return;
    bleRxBuffer.append(v);
    String err;
    if (updateStateFromJson(String(bleRxBuffer.c_str()), &err)) {
      bleRxBuffer.clear();
      scheduleBroadcast();
    } else {
      if (err != "incomplete") {
        Serial.printf("[BLE] JSON error: %s\n", err.c_str());
        bleRxBuffer.clear();
        // send error
        JsonDocument ed;                                  // CHANGED: v7 style
        ed["type"] = "error";
        ed["data"]["code"] = "parse";
        ed["data"]["msg"] = err;
        String out; serializeJson(ed, out);
        if (pTx) { pTx->setValue(out); pTx->notify(); }
      }
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* s, NimBLEConnInfo& connInfo) override {          // CHANGED: signature
    Serial.println("[BLE] Central connected");
    withState([](ScoreboardState& st){ st.ble = true; });
    scheduleBroadcast();
  }
  void onDisconnect(NimBLEServer* s, NimBLEConnInfo& connInfo, int reason) override { // CHANGED: signature
    Serial.printf("[BLE] Central disconnected (%d)\n", reason);
    withState([](ScoreboardState& st){ st.ble = false; });
    NimBLEDevice::startAdvertising();
    scheduleBroadcast();
  }
};

void setupBLE() {
  NimBLEDevice::init("ESP32-Display-01");
  NimBLEDevice::setMTU(247);
  NimBLEDevice::setPower(ESP_PWR_LVL_P7); // bump a bit

  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* svc = pServer->createService(SERVICE_UUID);
  pTx = svc->createCharacteristic(TX_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);
  pRx = svc->createCharacteristic(RX_CHAR_UUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  pRx->setCallbacks(new RxCallbacks());

  svc->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  // Include the device name in the scan response so it shows up in scanner UIs
  NimBLEAdvertisementData scanData;                    // CHANGED: advertise name via scan response
  scanData.setName("VSDisplay");
  adv->setScanResponseData(scanData);
  adv->start();
  Serial.println("[BLE] Advertising");
}

// ---- HTTP ----
void addCorsHeaders(AsyncWebServerResponse* r) {
  r->addHeader("Access-Control-Allow-Origin", GH_PAGES_ORIGIN);
  r->addHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  r->addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

void setupHTTP() {
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Origin", GH_PAGES_ORIGIN); // CHANGED: keep default headers for CORS in maintained fork
  DefaultHeaders::Instance().addHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");

  server.on("/api/v1/ping", HTTP_GET, [](AsyncWebServerRequest* req){
    auto* r = req->beginResponse(200, "text/plain", "pong");
    addCorsHeaders(r);
    req->send(r);
  });

  server.on("/api/v1/state", HTTP_GET, [](AsyncWebServerRequest* req){
    String json = stateToJson();
    auto* r = req->beginResponse(200, "application/json", json);
    addCorsHeaders(r);
    req->send(r);
  });

  server.on("/api/v1/scoreboard", HTTP_OPTIONS, [](AsyncWebServerRequest* req){
    auto* r = req->beginResponse(204);
    addCorsHeaders(r);
    req->send(r);
  });

  server.on("/api/v1/scoreboard", HTTP_POST, [](AsyncWebServerRequest* req){}, NULL,
    [](AsyncWebServerRequest* req, uint8_t* data, size_t len, size_t index, size_t total) {
      static String body;
      if (index == 0) body = "";
      body.concat((const char*)data, len);
      if (index + len == total) {
        String err;
        bool ok = updateStateFromJson(body, &err);
        if (ok) {
          scheduleBroadcast();
          JsonDocument ack;                       // CHANGED: v7 style
          ack["type"]="ack"; ack["data"]["ok"]=true;
          String out; serializeJson(ack, out);
          auto* r = req->beginResponse(200, "application/json", out);
          addCorsHeaders(r);
          req->send(r);
        } else {
          JsonDocument ed;                        // CHANGED: v7 style
          ed["type"]="error"; ed["data"]["code"]="parse"; ed["data"]["msg"]=err;
          String out; serializeJson(ed, out);
          auto* r = req->beginResponse(400, "application/json", out);
          addCorsHeaders(r);
          req->send(r);
        }
      }
    }
  );

  events.onConnect([](AsyncEventSourceClient *client){
    Serial.printf("[HTTP] SSE client connected id:%u\n", client->lastId());
    // Send current state immediately
    String json = stateToJson();
    client->send(json.c_str(), "state");
  });
  server.addHandler(&events);

  server.begin();
  Serial.println("[HTTP] Server started");
}

// ---- Broadcast to BLE + SSE + Display ----
void broadcastState() {
  // Render
  withState([](ScoreboardState& s){ renderer->render(s); });

  // BLE
  if (pTx) {
    String json = stateToJson();
    pTx->setValue(json.c_str());
    pTx->notify();
  }

  // SSE
  String json = stateToJson();
  events.send(json.c_str(), "state");
}
void scheduleBroadcast() {
  gPendingBroadcast = true;
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nBooting Scoreboard");

  stateMutex = xSemaphoreCreateMutex();

  // SoftAP for fallback
  WiFi.mode(WIFI_AP);
  bool ap = WiFi.softAP(SOFTAP_SSID, SOFTAP_PASS);
  Serial.printf("[WiFi] SoftAP %s (%s)\n", ap ? "started" : "failed", WiFi.softAPIP().toString().c_str());

  renderer->begin();
  setupHTTP();
  setupBLE();

  // Push initial state
  scheduleBroadcast();
}

void loop() {
  // Poll display for touch to toggle views
  renderer->loop();
  if (gPendingBroadcast) {
    gPendingBroadcast = false;
    broadcastState();
  }
}



