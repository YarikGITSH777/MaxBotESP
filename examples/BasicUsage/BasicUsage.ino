/*
    MaxBot - Библиотека для создания ботов Max на ESP8266/ESP32.
    
    Данный пример демонстрирует все основные возможности библиотеки:
    1. Прием и отправка текстовых сообщений.
    2. Создание Inline-меню (кнопки под сообщением).
    3. Обработку нажатий на кнопки (Callback).
    4. Управление железом (светодиод).
    5. OTA-обновление (прошивка по воздуху).

    =================== НАСТРОЙКА ПЛАТФОРМЫ MAX ===================
    
    1. Создайте бота:
       - Подключиться к платформе «MAX для партнёров», создать и верифицировать профиль организации.
       - Перейти в профиль своей организации на платформе.
       - В разделе «Чат-боты» нажать на кнопку «Создать».
       - Заполнить данные в настройках бота: указать имя бота, телефон и сайт компании, добавить логотип и краткое описание задач, которые решает бот, а также основных функций, преимуществ для клиентов и особенностей использования.
       - Нажать на кнопку «Создать».
       - Получите токен.
    
    2. Важно для групповых чатов:
       - Если бот будет работать в группе, добавьте его в администраторы.
    
    =================== НАСТРОЙКА WiFi И ВРЕМЕНИ ===================
    
    - Библиотека работает по HTTPS (защищенное соединение).
    - Для работы HTTPS на ESP32 и ESP8266 необходимо синхронизировать время!
    - В setup() ОБЯЗАТЕЛЬНО должен быть вызов configTime().
    - Без синхронизации времени сервер Max отвергнет соединение.
    
    =================== КАК ПОЛЬЗОВАТЬСЯ ===================
    
    1. Загрузите этот скетч в плату.
    2. Откройте монитор порта (скорость 115200).
    3. Напишите боту в Max:
       - "Меню" — откроется панель управления с кнопками.
       - "Версия" — бот пришлет текущую версию прошивки.
       - "Пинг" — проверка связи.
    
    4. Нажмите кнопку "LED ON" / "LED OFF":
       - Встроенный светодиод на плате будет включаться и выключаться.
    
    5. OTA Обновление:
       - В Arduino IDE: Скетч -> Экспорт бинарного файла.
       - Отправьте полученный файл .bin боту в чат.
       - Плата автоматически обновится и перезагрузится.
    
    =================== ПОДКЛЮЧЕНИЕ ===================
    
    Встроенный светодиод (LED_BUILTIN) на ESP32 обычно подключен к GPIO 2.
    Если на вашей плате другой пин, измените константу LED_PIN.
    
    ================================================================
*/
#include <WiFi.h>
#include "MaxBot.h"

// ================= НАСТРОЙКИ =================
const char* ssid = "YOUR_WIFI_SSID";
const char* pass = "YOUR_WIFI_PASS";
const char* token = "YOUR_MAX_BOT_TOKEN";         // Токен из раздела Чат-боты -> Интеграция
// =============================================


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
