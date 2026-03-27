/*
   Пример OTA обновления для MaxBot.
   
   Алгоритм:
   1. Бот получает файл в чате.
   2. Проверяет, что это файл прошивки (.bin).
   3. Скачивает и обновляет прошивку.
   4. Перезагружает плату.
*/

#include <WiFi.h>
#include "MaxBot.h"

// ================= НАСТРОЙКИ =================
const char* ssid = "YOUR_WIFI_SSID";
const char* pass = "YOUR_WIFI_PASS";
const char* token = "YOUR_MAX_BOT_TOKEN";
// =============================================

MaxBot bot(token);

// Обработчик сообщений
void newMsg(MaxMsg& msg) {
  
  // Если пришло сообщение с файлом
  if (msg.isFile) {
    Serial.println("Received file:");
    Serial.println(msg.fileUrl);

    // Проверяем, что это бинарный файл прошивки
    // Проверяем и имя файла, и ссылку (на случай если имя пустое)
    if (msg.fileName.indexOf(".bin") != -1 || msg.fileUrl.indexOf(".bin") != -1) {
      
      bot.sendMessage("📥 Получена прошивка. Начинаю обновление...", msg.chat_id);
      
      // Запускаем процесс OTA
      bool success = bot.updateFirmware(msg.fileUrl);
      
      if (success) {
        bot.sendMessage("✅ Обновление успешно! Перезагрузка...", msg.chat_id);
        delay(1000);
        ESP.restart(); // Перезагружаем плату
      } else {
        bot.sendMessage("❌ Ошибка при обновлении!", msg.chat_id);
      }
      
    } else {
      bot.sendMessage("⚠️ Это не файл прошивки (.bin).", msg.chat_id);
    }
    return;
  }

  // Обычные текстовые команды
  if (msg.type == "message_created") {
    if (msg.text == "Версия") {
      // Меняйте версию перед компиляцией, чтобы проверить, что обновление прошло
      bot.sendMessage("Текущая версия: 1.0", msg.chat_id);
    } else {
      bot.sendMessage("Отправьте файл .bin для обновления прошивки.", msg.chat_id);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Подключение к WiFi
  WiFi.begin(ssid, pass);
  Serial.print("Connecting WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ WiFi Connected");

  // Синхронизация времени (ОБЯЗАТЕЛЬНО для HTTPS)
  configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Sync time");
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\n✅ Time synced");

  // Подключаем обработчик
  bot.attach(newMsg);
  
  Serial.println("🤖 Bot ready. Send .bin file to update.");
}

void loop() {
  bot.tick();
}
