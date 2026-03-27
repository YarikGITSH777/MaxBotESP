#ifndef MAXBOT_H
#define MAXBOT_H

#include <Arduino.h>
#include <WiFiClientSecure.h>

struct MaxMsg {
    int64_t chat_id = 0;
    int64_t user_id = 0;
    String text;
    String type; 
    String callback_data;
    String message_id;
    
    // Новые поля для файлов
    bool isFile = false;
    String fileUrl;    // Ссылка на скачивание
    String fileName;   // Имя файла
};

class MaxBot {
public:
    MaxBot(const char* token);
    void tick(); 
    void attach(void (*handler)(MaxMsg& msg));
    
    uint8_t sendMessage(String text);
    uint8_t sendMessage(String text, int64_t chat_id);
    uint8_t sendMessage(String text, int64_t chat_id, String buttonLayout);
    void answerCallback(String msg_id, String notification);
    
    // Новое: OTA Обновление
    // Возвращает true при успехе, false при ошибке
    bool updateFirmware(String fileUrl);

private:
    WiFiClientSecure _client;
    String _token;
    String _host = "platform-api.max.ru";
    int64_t _lastMarker = 0;
    int64_t _lastChatId = 0;
    
    void (*_callback)(MaxMsg& msg) = nullptr;
    bool processUpdate(MaxMsg* msgOut);
    uint8_t sendPostRequest(String url, String body);
    String generateKeyboardJson(String text, String buttonLayout);
};

#endif
