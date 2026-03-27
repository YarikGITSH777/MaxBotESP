#include <WiFi.h>
#include "MaxBot.h"

const char* ssid = "login";
const char* pass = "pass";
const char* token = "xxxxxxxxxxxxxxxxxxxxxxxx"; // Токен из раздела Чат-боты -> Интеграция

// Обычно встроенный LED на ESP32 - это GPIO 2.
// Если на вашей плате другой пин, измените число.
const int LED_PIN = 2; 
// =============================================

MaxBot bot(token);

// Обработчик всех входящих событий
void newMsg(MaxMsg& msg) {

    // ------------------ OTA ОБНОВЛЕНИЕ ------------------
    if (msg.isFile) {
        // Если прислали файл
        Serial.println("Received FILE URL:");
        Serial.println(msg.fileUrl);

        if (msg.fileUrl.length() > 0) {
             bot.sendMessage("Получена прошивка. Обновляюсь...", msg.chat_id);
             
             bool success = bot.updateFirmware(msg.fileUrl);
             
             if (success) {
                 bot.sendMessage("Успех! Перезагрузка...", msg.chat_id);
                 delay(1000);
                 ESP.restart();
             } else {
                 bot.sendMessage("Ошибка обновления!", msg.chat_id);
             }
        }
        return;
    }

    // ------------------ КНОПКИ (CALLBACK) ------------------
    if (msg.type == "message_callback") {
        // Сначала убираем часики у кнопки
        bot.answerCallback(msg.message_id, "Выполнено: " + msg.callback_data);

        // Логика кнопок
        if (msg.callback_data == "LED ON") {
            digitalWrite(LED_PIN, HIGH); // Включаем светодиод
            bot.sendMessage("💡 Светодиод ВКЛЮЧЕН", msg.chat_id);
        } 
        else if (msg.callback_data == "LED OFF") {
            digitalWrite(LED_PIN, LOW);  // Выключаем светодиод
            bot.sendMessage("🔌 Светодиод ВЫКЛЮЧЕН", msg.chat_id);
        }
        else if (msg.callback_data == "Status") {
            // Проверяем состояние пина
            String stat = digitalRead(LED_PIN) ? "ВКЛЮЧЕН" : "ВЫКЛЮЧЕН";
            bot.sendMessage("📊 Текущий статус: " + stat, msg.chat_id);
        }
        return;
    }

    // ------------------ ТЕКСТОВЫЕ СООБЩЕНИЯ ------------------
    if (msg.type == "message_created") {
        
        if (msg.text == "Меню" || msg.text == "меню") {
            // Создаем меню с кнопками
            // \t - разделяет кнопки в одной строке
            // \n - переносит на новую строку
            String menu = "LED ON \t LED OFF \n Status";
            
            // Отправляем сообщение с меню
            bot.sendMessage("Панель управления Йотик 32A:", msg.chat_id, menu);
        } 
        else if (msg.text == "Версия") {
            bot.sendMessage("Версия прошивки: Final OTA Test v67.0", msg.chat_id);
        }
        else if (msg.text == "Пинг") {
            bot.sendMessage("Понг! Я в сети.", msg.chat_id);
        }
        else {
            // Эхо-ответ для неизвестных команд
            bot.sendMessage("Я не понял команду. Напишите 'Меню'.", msg.chat_id);
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Настройка светодиода
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW); // Гасим при старте

    // Подключение к WiFi
    WiFi.begin(ssid, pass);
    Serial.print("Connecting to WiFi");
    while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
    Serial.println("\n✅ WiFi OK");

    // Синхронизация времени (ОБЯЗАТЕЛЬНО для HTTPS)
    configTime(3 * 3600, 0, "pool.ntp.org", "time.nist.gov");
    Serial.print("Sync time");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) { Serial.print("."); delay(1000); }
    Serial.println("\n✅ Time OK");

    // Подключаем обработчик
    bot.attach(newMsg);
    
    Serial.println("🤖 Bot Ready! Send 'Menu' to start.");
}

void loop() {
    bot.tick();
}
