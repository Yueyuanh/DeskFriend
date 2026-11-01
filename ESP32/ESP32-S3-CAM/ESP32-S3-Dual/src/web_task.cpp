#include "web_task.h"

#include <WebServer.h>
#include <WiFi.h>
#include "led_task.h"
#include "servo_task.h"

namespace {
constexpr char WIFI_SSID[] = "这是浩的米";
constexpr char WIFI_PASSWORD[] = "123456789";
WebServer server(80);
bool serverStarted = false;

extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_index_html_end");

void sendIndex() {
  const size_t length = static_cast<size_t>(index_html_end - index_html_start);
  server.setContentLength(length);
  server.send(200, "text/html; charset=utf-8");
  server.sendContent(reinterpret_cast<const char *>(index_html_start), length);
}

void sendState() {
  String json = "{\"pitch\":" + String(servoPitchAngle)
              + ",\"yaw\":" + String(servoYawAngle)
              + ",\"wifi\":" + String(wifiConnected ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handlePose() {
  if (server.hasArg("pitch")) {
    servoPitchAngle = static_cast<uint8_t>(constrain(server.arg("pitch").toInt(), 0, 180));
  }
  if (server.hasArg("yaw")) {
    servoYawAngle = static_cast<uint8_t>(constrain(server.arg("yaw").toInt(), 0, 180));
  }
  sendState();
}

void handleColor() {
  const uint8_t red = static_cast<uint8_t>(constrain(server.arg("r").toInt(), 0, 255));
  const uint8_t green = static_cast<uint8_t>(constrain(server.arg("g").toInt(), 0, 255));
  const uint8_t blue = static_cast<uint8_t>(constrain(server.arg("b").toInt(), 0, 255));
  setExpressionColor(red, green, blue);
  server.send(200, "application/json", "{\"ok\":true}");
}

void startServer() {
  server.on("/", HTTP_GET, sendIndex);
  server.on("/api/state", HTTP_GET, sendState);
  server.on("/api/pose", HTTP_GET, handlePose);
  server.on("/api/color", HTTP_GET, handleColor);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  serverStarted = true;
  Serial.printf("Website: http://%s/\n", WiFi.localIP().toString().c_str());
}
} // namespace

WebTask_t webTaskInstance;
TaskHandle_t webTaskHandle = nullptr;

void WebTask_t::run(void *pvParameters) {
  (void)pvParameters;
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.printf("Connecting WiFi: %s\n", WIFI_SSID);

  for (;;) {
    if (WiFi.status() != WL_CONNECTED) {
      wifiConnected = false;
      if (millis() % 10000 < 20) {
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
      }
    } else {
      if (!wifiConnected) {
        wifiConnected = true;
        Serial.printf("WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
      }
      if (!serverStarted) {
        startServer();
      }
      server.handleClient();
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void WebTask_t::runTask(void *pvParameters) {
  webTaskInstance.run(pvParameters);
}
