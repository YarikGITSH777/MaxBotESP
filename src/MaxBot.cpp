#include "MaxBot.h"
#include <WiFi.h>
#include <Update.h> // Для OTA обновлений

// ==========================================
//          КОНСТРУКТОР И БАЗОВЫЕ
// ==========================================

MaxBot::MaxBot(const char* token) {
    _token = String(token);
}

void MaxBot::attach(void (*handler)(MaxMsg& msg)) {
    _callback = handler;
}

void MaxBot::tick() {
    if (!_callback) return;
    MaxMsg msg;
    if (processUpdate(&msg)) {
        _lastChatId = msg.chat_id;
        _callback(msg);
    }
}

// ==========================================
//          ПАРСИНГ (ВСЕ ТИПЫ)
// ==========================================
bool MaxBot::processUpdate(MaxMsg* msgOut) {
    WiFiClientSecure client;
    client.setInsecure();
    
    if (!client.connect(_host.c_str(), 443)) return false;

    String url = "/updates?limit=1&timeout=10";

    // при первом запуске НЕ берем историю
    if (!_initialized) {
    url = "/updates?limit=1&timeout=1";
    }

    if (_lastMarker > 0) url += "&marker=" + String(_lastMarker);

    client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                 "Host: " + _host + "\r\n" +
                 "Authorization: " + _token + "\r\n" +
                 "Connection: close\r\n\r\n");

    unsigned long start = millis();
    while (!client.available() && millis() - start < 12000) { delay(10); }
    if (!client.available()) { client.stop(); return false; }

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
    }

    String payload = client.readString();
    client.stop();
    if (payload.length() == 0) return false;

    // --- ПАРСИНГ МАРКЕРА ---
    int markerIdx = payload.indexOf("\"marker\":");
    if (markerIdx != -1) {
        int start = markerIdx + 9; String numStr;
        while (start < payload.length() && isDigit(payload[start])) { numStr += payload[start]; start++; }
        if (numStr.length() > 0) _lastMarker = atoll(numStr.c_str());
    }

    // ===== INIT: пропускаем старые сообщения =====
    if (!_initialized) {
       _initialized = true;
       // просто обновили marker и ВЫХОДИМ
       return false;
    }


    // ==========================================
    //    1. ОБЫЧНОЕ СООБЩЕНИЕ + ФАЙЛЫ
    // ==========================================
    if (payload.indexOf("\"message_created\"") != -1) {
        msgOut->type = "message_created";
        
        // Chat ID
        int cidIdx = payload.indexOf("\"chat_id\":");
        if (cidIdx != -1) { int start = cidIdx + 10; String numStr; while (start < payload.length() && isDigit(payload[start])) { numStr += payload[start]; start++; } msgOut->chat_id = atoll(numStr.c_str()); }
        
        // User ID
        int uidIdx = payload.indexOf("\"user_id\":");
        if (uidIdx != -1) { int start = uidIdx + 10; String numStr; while (start < payload.length() && isDigit(payload[start])) { numStr += payload[start]; start++; } msgOut->user_id = atoll(numStr.c_str()); }
        
        // Text
        int textKeyIdx = payload.indexOf("\"text\":");
        if (textKeyIdx != -1) {
             int startQuote = payload.indexOf("\"", textKeyIdx + 7);
             if (startQuote != -1) { String txt = ""; int pos = startQuote + 1; while (pos < payload.length()) { char c = payload[pos]; if (c == '\\' && payload[pos+1] == '"') { txt += '"'; pos += 2; } else if (c == '"') break; else { txt += c; pos++; } } msgOut->text = txt; }
        }
        
        // Message ID (mid)
        int midIdx = payload.indexOf("\"mid\":");
        if (midIdx != -1) { int s = payload.indexOf("\"", midIdx + 6); int e = payload.indexOf("\"", s + 1); if (s != -1 && e != -1) msgOut->message_id = payload.substring(s + 1, e); }

        // === ПРОВЕРКА НА ФАЙЛ (OTA) ===
        if (payload.indexOf("\"attachments\"") != -1) {
            int urlIdx = payload.indexOf("\"url\":");
            if (urlIdx != -1) {
                int startQuote = payload.indexOf("\"", urlIdx + 6);
                int endQuote = payload.indexOf("\"", startQuote + 1);
                if (startQuote != -1 && endQuote != -1) {
                    msgOut->fileUrl = payload.substring(startQuote + 1, endQuote);
                    msgOut->isFile = true;
                    
                    int nameIdx = payload.indexOf("\"title\":");
                    if (nameIdx != -1) {
                        int s = payload.indexOf("\"", nameIdx + 8);
                        int e = payload.indexOf("\"", s + 1);
                        if (s != -1 && e != -1) msgOut->fileName = payload.substring(s + 1, e);
                    }
                }
            }
        }
        return true;
    }

    // ==========================================
    //    2. НАЖАТИЕ КНОПКИ (CALLBACK)
    // ==========================================
    if (payload.indexOf("\"message_callback\"") != -1) {
        msgOut->type = "message_callback";
        
        // User ID
        int uidIdx = payload.indexOf("\"user_id\":");
        if (uidIdx != -1) { int start = uidIdx + 10; String numStr; while (start < payload.length() && isDigit(payload[start])) { numStr += payload[start]; start++; } msgOut->user_id = atoll(numStr.c_str()); }
        
        // Chat ID (ищем первое упоминание после callback блока)
        int cidIdx = payload.indexOf("\"chat_id\":", payload.indexOf("callback"));
        if (cidIdx != -1) { int start = cidIdx + 10; String numStr; while (start < payload.length() && isDigit(payload[start])) { numStr += payload[start]; start++; } msgOut->chat_id = atoll(numStr.c_str()); }

        // Message ID (mid)
        int midIdx = payload.indexOf("\"mid\":");
        if (midIdx != -1) { int s = payload.indexOf("\"", midIdx + 6); int e = payload.indexOf("\"", s + 1); if (s != -1 && e != -1) msgOut->message_id = payload.substring(s + 1, e); }

        // Payload (данные кнопки)
        int payIdx = payload.indexOf("\"payload\":");
        if (payIdx != -1) { int s = payload.indexOf("\"", payIdx + 10); int e = payload.indexOf("\"", s + 1); if (s != -1 && e != -1) msgOut->callback_data = payload.substring(s + 1, e); }
        
        return true;
    }

    return false;
}

// ==========================================
//          ОТПРАВКА СООБЩЕНИЙ
// ==========================================

uint8_t MaxBot::sendMessage(String text) {
    return sendMessage(text, _lastChatId, "");
}

uint8_t MaxBot::sendMessage(String text, int64_t chat_id) {
    return sendMessage(text, chat_id, "");
}

uint8_t MaxBot::sendMessage(String text, int64_t chat_id, String buttonLayout) {
    String body;
    if (buttonLayout.length() > 0) {
        body = generateKeyboardJson(text, buttonLayout);
    } else {
        body = "{\"text\":\"";
        for (int i=0; i<text.length(); i++) {
            if (text[i] == '"') body += "\\\"";
            else body += text[i];
        }
        body += "\"}";
    }
    
    String url = "/messages?chat_id=" + String(chat_id);
    return sendPostRequest(url, body);
}

void MaxBot::answerCallback(String msg_id, String notification) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(_host.c_str(), 443)) return;

    String body = "{\"message_id\":\"" + msg_id + "\", \"notification\":\"" + notification + "\"}";

    client.print(String("POST /answers HTTP/1.1\r\n") +
                 "Host: " + _host + "\r\n" +
                 "Authorization: " + _token + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + body.length() + "\r\n" +
                 "Connection: close\r\n\r\n" + 
                 body);
    
    unsigned long start = millis();
    while (!client.available() && millis() - start < 2000) { delay(10); }
    while (client.available()) client.read();
    client.stop();
}

// ==========================================
//          ГЕНЕРАТОР КЛАВИАТУРЫ
// ==========================================
String MaxBot::generateKeyboardJson(String text, String buttonLayout) {
    String json = "{\"text\":\"";
    for (int i=0; i<text.length(); i++) {
        if (text[i] == '"') json += "\\\"";
        else json += text[i];
    }
    json += "\", \"attachments\":[{";
    json += "\"type\":\"inline_keyboard\", ";
    json += "\"payload\":{";
    json += "\"buttons\":[";
    
    int lineStart = 0;
    bool firstLine = true;
    
    while (lineStart < buttonLayout.length()) {
        int lineEnd = buttonLayout.indexOf('\n', lineStart);
        if (lineEnd == -1) lineEnd = buttonLayout.length();
        
        String line = buttonLayout.substring(lineStart, lineEnd);
        if (line.length() > 0) {
            if (!firstLine) json += ",";
            firstLine = false;
            json += "[";
            
            int btnStart = 0;
            bool firstBtn = true;
            while (btnStart < line.length()) {
                int btnEnd = line.indexOf('\t', btnStart);
                if (btnEnd == -1) btnEnd = line.length();
                
                String btnText = line.substring(btnStart, btnEnd);
                btnText.trim();
                
                if (btnText.length() > 0) {
                    if (!firstBtn) json += ",";
                    firstBtn = false;
                    json += "{\"type\":\"callback\", \"text\":\"" + btnText + "\", \"payload\":\"" + btnText + "\"}";
                }
                btnStart = btnEnd + 1;
            }
            json += "]";
        }
        lineStart = lineEnd + 1;
    }
    
    json += "]";
    json += "}";
    json += "}]";
    json += "}";
    return json;
}

// ==========================================
//          НИЗКОУРОВНЕВАЯ ОТПРАВКА
// ==========================================
uint8_t MaxBot::sendPostRequest(String url, String body) {
    WiFiClientSecure client;
    client.setInsecure();
    if (!client.connect(_host.c_str(), 443)) return 4;

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + _host + "\r\n" +
                 "Authorization: " + _token + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "Content-Length: " + body.length() + "\r\n" +
                 "Connection: close\r\n\r\n" + body);

    unsigned long start = millis();
    while (!client.available() && millis() - start < 5000) { delay(10); }
    
    int httpCode = 0;
    if (client.available()) {
        String line = client.readStringUntil('\n');
        int spaceIdx = line.indexOf(' ');
        if (spaceIdx != -1) httpCode = line.substring(spaceIdx + 1, spaceIdx + 4).toInt();
    }
    client.stop();
    
    if (httpCode == 200) return 1;
    return 3;
}

// ==========================================
//          OTA ОБНОВЛЕНИЕ
// ==========================================
bool MaxBot::updateFirmware(String fileUrl) {
    Serial.println("\n--- Starting OTA Update ---");
    Serial.println("URL: " + fileUrl);

    WiFiClientSecure client;
    client.setInsecure();

    // Парсим хост и путь из URL
    // URL вида https://host/path?params
    int hostStart = fileUrl.indexOf("://");
    if (hostStart == -1) return false;
    hostStart += 3;
    
    // Ищем начало пути (первый слеш после домена)
    int pathStart = fileUrl.indexOf("/", hostStart);
    
    String host;
    String path;
    
    if (pathStart == -1) {
        // Если слеша нет (бывает редко), то путь корневой
        host = fileUrl.substring(hostStart);
        path = "/";
    } else {
        host = fileUrl.substring(hostStart, pathStart);
        // ВАЖНО: Берем всё до конца строки, включая ? и &
        path = fileUrl.substring(pathStart); 
    }

    Serial.println("Host: " + host);
    Serial.println("Path: " + path);

    if (!client.connect(host.c_str(), 443)) {
        Serial.println("Connect failed");
        return false;
    }

    // Запрашиваем файл
    client.print(String("GET ") + path + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Connection: close\r\n\r\n");

    // Ждем заголовков
    unsigned long start = millis();
    while (!client.available() && millis() - start < 5000) { delay(10); }
    
    // Читаем заголовки, ищем Content-Length
    size_t contentLength = 0;
    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r") break;
        if (line.startsWith("Content-Length:")) {
            contentLength = line.substring(15).toInt();
        }
    }

    if (contentLength == 0) {
        Serial.println("Size is 0 or Content-Length not found");
        return false;
    }

    Serial.printf("Update size: %d bytes\n", contentLength);

    // Инициализируем обновление
    if (!Update.begin(contentLength, U_FLASH)) {
        Serial.println("Not enough space");
        return false;
    }

    // Пишем данные из потока
    size_t written = Update.writeStream(client);
    
    if (written == contentLength) {
        Serial.println("Written successfully");
    } else {
        Serial.printf("Written %d / %d\n", written, contentLength);
    }

    // Завершаем обновление
    if (Update.end()) {
        Serial.println("OTA Finished!");
        client.stop();
        return true;
    } else {
        Serial.println("OTA Error: " + String(Update.getError()));
        return false;
    }
}
