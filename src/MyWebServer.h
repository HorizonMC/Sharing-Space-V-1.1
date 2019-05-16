
#ifndef MY_WEBSERVER_H_
#define MY_WEBSERVER_H_
#include <Arduino.h>
#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <SPIFFSEditor.h>
#include <ArduinoJson.h>


#define FIRMWARE_VERSION  "2008.0.5.1"
#define CHANNEL_1         16
#define CHANNEL_2         17
#define CHANNEL_3         2
#define CHANNEL_4         26
//#define CHANNEL_1         16
//#define CHANNEL_2         17
//#define CHANNEL_3         2
//#define CHANNEL_4         26
#define CHIP_ID           ESP.getEfuseMac()
#define DEVICE_ID         String((uint16_t)(CHIP_ID>>32), HEX) + String((uint32_t)CHIP_ID, HEX)

typedef std::function<void(uint8_t gpio)> ExecuteGpioHandler;

class MyWebServer{
  private:
    uint64_t  chipid=ESP.getEfuseMac();
    String deviceId = String((uint16_t)(chipid>>32), HEX) + String((uint32_t)chipid, HEX);
    String lastMessage;
    ExecuteGpioHandler _handler;
    AsyncWebSocket *ws = new AsyncWebSocket("/ws");
    AsyncEventSource *events = new AsyncEventSource("/events");
    void send_properties( AsyncWebSocketClient * client);
    void onWsEvent(AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len);
  public:
    MyWebServer();
    void send_event( const char *message, const char *event );
    void textAll(const char * msg, size_t len);
    void setup(AsyncWebServer *server, const char * username, const char * password);
    void setExecuteGpio(ExecuteGpioHandler handler);
    void sendMessage(const char* msg,  size_t len);
};

#endif
