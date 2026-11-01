#include "web_task.h"

#include <WiFi.h>
#include <esp_http_server.h>
#include <cJSON.h>
#include <math.h>
#include <esp_heap_caps.h>

#include "device_state.h"
#include "face_state.h"
#include "face_frame.h"
#include "network_config.h"
#include "network_options.h"
#include "web_assets.h"

namespace {
httpd_handle_t server = nullptr;

esp_err_t sendScreen(httpd_req_t* request) {
  uint32_t seq;
  uint8_t* copy = faceFrameCopy(&seq);
  if (!copy) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "display not ready");
  httpd_resp_set_type(request, "application/octet-stream");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  char sequence[16];snprintf(sequence,sizeof(sequence),"%lu",(unsigned long)seq);
  httpd_resp_set_hdr(request,"X-Frame-Seq",sequence);
  esp_err_t result = httpd_resp_send(request, reinterpret_cast<const char*>(copy), 240*240*2);
  heap_caps_free(copy);
  return result;
}

esp_err_t sendAsset(httpd_req_t* request) {
  const auto* asset = static_cast<const WebAsset*>(request->user_ctx);
  httpd_resp_set_type(request, asset->type);
  if (asset->gzip) httpd_resp_set_hdr(request, "Content-Encoding", "gzip");
  httpd_resp_set_hdr(request, "Cache-Control", asset->gzip ? "no-cache" : "public, max-age=86400");
  // Flash-backed data, no 1 MB String or task-stack copy.
  return httpd_resp_send(request, reinterpret_cast<const char*>(asset->data), asset->size);
}

// Board status and semantic events. Diagnostic screenshots are outgoing only;
// /api/face never receives pixels or drawing commands.
esp_err_t sendState(httpd_req_t* request) {
  const ImuSnapshot imu = deviceState.imu();
  const LocalFaceEvent local = deviceState.localFace();
  const int32_t remaining = int32_t(local.untilMs-millis());
  const FaceUpdate face = faceRequest.snapshot();
  uint32_t displayed, renderUs, flushUs; faceRequest.metrics(displayed, renderUs, flushUs);
  char json[960];
  snprintf(json, sizeof(json),
      "{\"stage\":%lu,\"face\":\"%s\",\"accepted_seq\":%lu,\"displayed_seq\":%lu,\"render_us\":%lu,\"flush_us\":%lu,\"uptime_ms\":%lu,\"heap\":%lu,\"psram\":%lu,\"rssi\":%d,"
      "\"local\":{\"event_seq\":%lu,\"face\":\"%s\",\"remaining_ms\":%lu},\"imu\":{\"ready\":%s,\"updated_ms\":%lu,\"acc\":[%.3f,%.3f,%.3f],"
      "\"gyro\":[%.4f,%.4f,%.4f],\"angles\":[%.2f,%.2f,%.2f]}}",
      (unsigned long)faceRenderStage, face.face[0] ? face.face : "idle", (unsigned long)face.clientSeq, (unsigned long)displayed,
      (unsigned long)renderUs, (unsigned long)flushUs,
      static_cast<unsigned long>(millis()),
      static_cast<unsigned long>(ESP.getFreeHeap()),
      static_cast<unsigned long>(ESP.getFreePsram()), WiFi.RSSI(),
      (unsigned long)local.seq, remaining>0 && local.seq ? "dizzy" : "", (unsigned long)(local.seq && remaining>0?remaining:0),
      imu.ready ? "true" : "false", static_cast<unsigned long>(imu.updatedMs),
      imu.acceleration[0], imu.acceleration[1], imu.acceleration[2],
      imu.gyro[0], imu.gyro[1], imu.gyro[2], imu.angles[0], imu.angles[1], imu.angles[2]);
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_sendstr(request, json);
}


// The protocol transfers only semantic state, never a draw list or framebuffer.
esp_err_t receiveFace(httpd_req_t* request) {
  constexpr int maxBody = 4096;
  if (request->content_len <= 0 || request->content_len >= maxBody)
    return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "state must be under 4096 bytes");
  char* body = static_cast<char*>(heap_caps_malloc(request->content_len + 1, MALLOC_CAP_8BIT));
  if (!body) return httpd_resp_send_err(request, HTTPD_500_INTERNAL_SERVER_ERROR, "no memory");
  int used = 0;
  while (used < request->content_len) {
    int n = httpd_req_recv(request, body + used, request->content_len - used);
    if (n <= 0) { free(body); return ESP_FAIL; }
    used += n;
  }
  body[used] = 0;
  const char* parseEnd = nullptr;
  cJSON* json = cJSON_ParseWithLengthOpts(body, used + 1, &parseEnd, true);
  free(body);
  if (!cJSON_IsObject(json)) {
    cJSON_Delete(json);
    return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid JSON");
  }
  bool valid = true;
  auto number = [&](const cJSON* object, const char* key, double fallback, double low, double high) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) return fallback;
    if (!cJSON_IsNumber(item) || !isfinite(item->valuedouble) || item->valuedouble < low || item->valuedouble > high) { valid = false; return fallback; }
    return item->valuedouble;
  };
  auto boolean = [&](const cJSON* object, const char* key) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (item && !cJSON_IsBool(item)) valid = false;
    return cJSON_IsTrue(item);
  };
  auto string = [&](const cJSON* object, const char* key, char* target, size_t capacity) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(object, key);
    if (!item) return;
    if (!cJSON_IsString(item) || strlen(item->valuestring) >= capacity) { valid = false; return; }
    strcpy(target, item->valuestring);
  };
  FaceUpdate update{};
  number(json, "protocol", 2, 2, 2);
  string(json, "face", update.face, sizeof(update.face));
  const char* names[] = {"idle","happy","curious","shy","pressed","greeting","eat","water","off","energy","night","thanks","miss","msg","cheer","sleepy","wow","sad","love","wink","think","scared","dance","dizzy","no","lowbat","charging","full","boot","offline","clock","chat","stick","gsend","grecv","replyWait","replyDone"};
  bool known = false; for (auto name : names) if (!strcmp(name, update.face)) known = true;
  valid &= known;
  string(json, "text", update.text, sizeof(update.text));
  update.time = number(json, "time", 0, 0, 1e8);
  update.age = number(json, "age", 0, 0, 1e8);
  update.blink = boolean(json, "blink");
  update.blinkRemaining = number(json, "blinkRemaining", update.blink ? .12 : 0, 0, 1);
  update.hold = boolean(json, "hold");
  update.holdProgress = number(json, "holdProgress", 0, 0, 1);
  update.holdRate = number(json, "holdRate", 0, -10, 10);
  update.wallClockMs = number(json, "wallClockMs", 0, 0, 1e14);
  update.utcOffsetMinutes = number(json, "utcOffsetMinutes", 0, -840, 840);
  update.paused = boolean(json, "paused");
  update.clientSeq = number(json, "seq", 0, 0, 4294967295.0);
  update.clockHour = number(json, "ch", 0, 0, 23);
  update.clockMinute = number(json, "cm", 0, 0, 59);
  update.clockSecond = number(json, "cs", 0, 0, 59);
  update.clockWeekday = number(json, "cw", 0, 0, 6);
  update.clockMonth = number(json, "cmo", 1, 1, 12);
  update.clockDay = number(json, "cd", 1, 1, 31);
  update.clockSeconds = number(json, "csec", 0, 0, 59.99999);
  const cJSON* chat = cJSON_GetObjectItemCaseSensitive(json, "chat");
  if (chat && !cJSON_IsObject(chat)) valid = false;
  if (chat) {
    string(chat, "name", update.chatName, sizeof(update.chatName));
    const cJSON* pages = cJSON_GetObjectItemCaseSensitive(chat, "pages");
    const cJSON* starts = cJSON_GetObjectItemCaseSensitive(chat, "starts");
    if (!cJSON_IsArray(pages) || !cJSON_IsArray(starts) || cJSON_GetArraySize(pages) > 4 || cJSON_GetArraySize(starts) != cJSON_GetArraySize(pages)) valid = false;
    if (valid) for (int p = 0; p < cJSON_GetArraySize(pages); ++p) {
      const cJSON* page = cJSON_GetArrayItem(pages, p);
      const cJSON* at = cJSON_GetArrayItem(starts, p);
      if (!cJSON_IsArray(page) || cJSON_GetArraySize(page) < 1 || cJSON_GetArraySize(page) > 3 || !cJSON_IsNumber(at) || !isfinite(at->valuedouble) || at->valuedouble < 0 || at->valuedouble > 1e8 || (p && at->valuedouble < update.chatStarts[p - 1])) { valid = false; break; }
      update.chatStarts[p] = at->valuedouble;
      update.chatPageLines[p] = cJSON_GetArraySize(page);
      ++update.chatPageCount;
      for (int line = 0; line < cJSON_GetArraySize(page); ++line) {
        const cJSON* text = cJSON_GetArrayItem(page, line);
        char* dest = update.chatLines[p * 3 + line];
        if (!cJSON_IsString(text) || strlen(text->valuestring) >= sizeof(update.chatLines[0])) { valid = false; break; }
        strcpy(dest, text->valuestring);
        update.chatLineCount = p * 3 + line + 1;
      }
    }
  }
  const cJSON* stick = cJSON_GetObjectItemCaseSensitive(json, "stick");
  strcpy(update.stick, "idle"); strcpy(update.stickEyes, "open"); strcpy(update.stickMouth, "smile");
  if (stick && !cJSON_IsObject(stick)) valid = false;
  if (stick) {
    string(stick, "anim", update.stick, sizeof(update.stick));
    string(stick, "eyes", update.stickEyes, sizeof(update.stickEyes));
    string(stick, "mouth", update.stickMouth, sizeof(update.stickMouth));
    update.cameraPose = boolean(stick, "camera");
    const cJSON* joints = cJSON_GetObjectItemCaseSensitive(stick, "joints");
    if (update.cameraPose) {
      if (!cJSON_IsArray(joints) || cJSON_GetArraySize(joints) != 13) valid = false;
      else for (int i = 0; i < 13; ++i) for (int axis = 0; axis < 2; ++axis) {
        const cJSON* xy = cJSON_GetArrayItem(joints, i);
        const cJSON* v = cJSON_GetArrayItem(xy, axis);
        if (!cJSON_IsArray(xy) || cJSON_GetArraySize(xy) != 2 || !cJSON_IsNumber(v) || !isfinite(v->valuedouble) || fabs(v->valuedouble) > 1000) valid = false;
        else update.joints[i][axis] = v->valuedouble;
      }
    }
  }
  cJSON_Delete(json);
  if (!valid) return httpd_resp_send_err(request, HTTPD_400_BAD_REQUEST, "invalid face state");
  static char lastFace[16] = "";
  if (strcmp(lastFace, update.face)) {
    strcpy(lastFace, update.face);
    Serial.printf("[FACE] page -> %s \"%s\"\n", update.face, update.text);
  }
  faceRequest.update(update);
  uint32_t displayed, renderUs, flushUs; faceRequest.metrics(displayed, renderUs, flushUs);
  char reply[160];
  snprintf(reply, sizeof(reply), "{\"ok\":true,\"client_seq\":%lu,\"displayed_seq\":%lu,\"render_us\":%lu,\"flush_us\":%lu}", (unsigned long)update.clientSeq, (unsigned long)displayed, (unsigned long)renderUs, (unsigned long)flushUs);
  httpd_resp_set_type(request, "application/json");
  httpd_resp_set_hdr(request, "Cache-Control", "no-store");
  return httpd_resp_sendstr(request, reply);
}

bool startServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.core_id = 0;  // Network on Core 0, display/IMU on Core 1.
  config.task_priority = 2;
  config.stack_size = 16384;
  config.max_uri_handlers = 12;
  config.max_open_sockets = 6;
  config.lru_purge_enable = true;
  if (httpd_start(&server, &config) != ESP_OK) return false;
  auto add = [](const char* uri, httpd_method_t method,
                esp_err_t (*handler)(httpd_req_t*), void* context = nullptr) {
    httpd_uri_t route{};
    route.uri = uri;
    route.method = method;
    route.handler = handler;
    route.user_ctx = context;
    return httpd_register_uri_handler(server, &route) == ESP_OK;
  };
  bool ok = true;
  for (const WebAsset& asset : kWebAssets) {
    ok &= add(asset.uri, HTTP_GET, sendAsset, const_cast<WebAsset*>(&asset));
  }
  ok &= add("/api/state", HTTP_GET, sendState);
  ok &= add("/api/screen.rgb565", HTTP_GET, sendScreen);
  ok &= add("/api/face", HTTP_POST, receiveFace);
  if (!ok) {
    httpd_stop(server);
    server = nullptr;
  }
  return ok;
}
}  // namespace

WebTask_t webTaskInstance;
TaskHandle_t webTaskHandle = nullptr;

void WebTask_t::begin() {
  // The display task owns the face buffer; nothing is allocated for streaming here.
}

void WebTask_t::runTask(void* parameters) { webTaskInstance.run(parameters); }

void WebTask_t::run(void* parameters) {
  (void)parameters;
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("deskfriend-lcd");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  // This project targets the user's Windows hotspot (ICS subnet). Keep a
  // stable address for the browser and serial log.
  WiFi.config(kDeviceIp,kHotspotGateway,kHotspotMask,kHotspotGateway);
  WiFi.begin(kWifiSsid, kWifiPassword);
  Serial.printf("[WIFI] connecting to %s (2.4 GHz)\n", kWifiSsid);
  uint32_t lastRetry = millis();
  uint32_t lastLog = 0;
  bool wasConnected = false;
  for (;;) {
    const uint32_t now = millis();
    const bool connected = WiFi.status() == WL_CONNECTED;
    deviceState.network(connected);
    if (connected) {
      if (!server && now - lastRetry > 1000) {
        lastRetry = now;
        if (!startServer()) Serial.println("[WEB] start failed; retrying");
      }
      if (server && (!wasConnected || now - lastLog >= 10000)) {
        lastLog = now;
        Serial.printf("[WEB] http://%s/ | RSSI=%d | heap=%lu PSRAM=%lu stack=%lu\n",
            WiFi.localIP().toString().c_str(), WiFi.RSSI(),
            static_cast<unsigned long>(ESP.getFreeHeap()),
            static_cast<unsigned long>(ESP.getFreePsram()),
            static_cast<unsigned long>(uxTaskGetStackHighWaterMark(nullptr)));
        if (!wasConnected) Serial.printf("[WIFI] MAC=%s gateway=%s mask=%s\n", WiFi.macAddress().c_str(), WiFi.gatewayIP().toString().c_str(), WiFi.subnetMask().toString().c_str());
      }
    } else if (now - lastRetry >= 15000) {
      lastRetry = now;
      WiFi.disconnect();
      WiFi.begin(kWifiSsid, kWifiPassword);
      Serial.printf("[WIFI] retry, status=%d; check hotspot name/password and 2.4 GHz\n", WiFi.status());
    }
    wasConnected = connected;
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}
