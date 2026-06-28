#include "telegram_notify.h"
#include "telegram_config.h"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

// Encode URL sederhana
static String urlEncode(const String &str) {
  String encoded = "";
  char buf[4];
  for (size_t i = 0; i < str.length(); i++) {
    char c = str[i];
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || ('0' <= c && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
      encoded += c;
    } else if (c == ' ') {
      encoded += "%20";
    } else {
      snprintf(buf, sizeof(buf), "%%%02X", (unsigned char)c);
      encoded += buf;
    }
  }
  return encoded;
}

void sendTelegramNotification(String message) {
#if TELEGRAM_ENABLED
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure client;
  client.setInsecure(); // Melewati verifikasi sertifikat (lebih sederhana untuk prototyping)

  HTTPClient https;

  String url = "https://api.telegram.org/bot";
  url += TELEGRAM_BOT_TOKEN;
  url += "/sendMessage?chat_id=";
  url += TELEGRAM_CHAT_ID;
  url += "&text=";
  url += urlEncode(message);

  if (https.begin(client, url)) {
    int httpCode = https.GET();
    if (httpCode <= 0) {
      // gagal mengirim, bisa ditambahkan retry atau log
    }
    https.end();
  }
#endif
}
