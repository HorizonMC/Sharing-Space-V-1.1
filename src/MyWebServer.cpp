#include "MyWebServer.h" 
#include <AsyncJson.h>

MyWebServer::MyWebServer(){

}

void MyWebServer::send_properties(AsyncWebSocketClient *client) {
  DynamicJsonBuffer jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["id"] = deviceId;
  root["ip"] = WiFi.localIP().toString();
  root["type"] = 4;
  root["ssid"] = WiFi.SSID();
  root["netmask"] = WiFi.subnetMask().toString();
  root["gateway"] = WiFi.gatewayIP().toString();
  root["version"] = FIRMWARE_VERSION;
  size_t len = root.measureLength();
  AsyncWebSocketMessageBuffer * buffer = ws->makeBuffer(len); //  creates a buffer (len + 1) for you.
  if (buffer) {
    root.printTo((char *)buffer->get(), len + 1);
    if (client) {
      client->text(buffer);
    } else {
      ws->textAll(buffer);
    }
  }
}

void MyWebServer::textAll(const char *msg, size_t len) {
  this->lastMessage = msg;
  ws->textAll(msg, len);
}

void MyWebServer::onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len){
  switch (type) {
    case WS_EVT_CONNECT: {
      Serial.println(this->lastMessage);
      client->text(this->lastMessage.c_str(), this->lastMessage.length());
      send_properties(client);
      break;
    }
    case WS_EVT_DISCONNECT:
      break;
    case WS_EVT_ERROR:
      break;
    case WS_EVT_PONG:
      break;
    case WS_EVT_DATA:{
      AwsFrameInfo * info = (AwsFrameInfo*)arg;
      String msg = "";
      if(info->opcode == WS_TEXT){
        for(size_t i=0; i < info->len; i++) {
          msg += (char) data[i];
        }
        Serial.println("WS Message: " + msg);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& root = jsonBuffer.parseObject(msg);
        if (root.success()) {
          if (root["type"].as<int>() == 1) {
            if (_handler) {
              int gpio = root["gpio"].as<int>();
              Serial.println("execute ws toggle: " + String(gpio));
              _handler(gpio);
            }
          }
        } else {
          String json = "{ \"type\": 0, \"value\": \"invalid json command\" }";
          client->text(json.c_str(), json.length());
        }
      }
      break;
    }
  }
}

String scan_network() {
  String json = "[";
  int n = WiFi.scanComplete();
  if(n == -2){
    WiFi.scanNetworks(true);
  } else if(n){
    for (int i = 0; i < n; ++i){
      if(i) json += ",";
      json += "{";
      json += "\"rssi\":"+String(WiFi.RSSI(i));
      json += ",\"ssid\":\""+WiFi.SSID(i)+"\"";
      json += ",\"bssid\":\""+WiFi.BSSIDstr(i)+"\"";
      json += ",\"channel\":"+String(WiFi.channel(i));
      json += ",\"secure\":"+String(WiFi.encryptionType(i));
      json += "}";
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
      WiFi.scanNetworks(true);
    }
  }
  json += "]";
  return json;
}

String scan_option_network() {
  String option = "";
  int n = WiFi.scanComplete();
  if(n == -2){
    WiFi.scanNetworks(true);
  } else if(n){
    for (int i = 0; i < n; ++i){
  		if (WiFi.SSID(i) == WiFi.SSID()){
  			option += "<option selected>" + WiFi.SSID(i) + "</option>";
  		}else{
  			option += "<option>" + WiFi.SSID(i) + "</option>";
  		}
    }
    WiFi.scanDelete();
    if(WiFi.scanComplete() == -2){
      WiFi.scanNetworks(true);
    }
  }
  return option;
}
 
String index_processor(const String& var) {
  if(var == "ledstatus1"){ return digitalRead(CHANNEL_1) == HIGH ? "led-on" : "led-off"; }
  if(var == "ledstatus2"){ return  digitalRead(CHANNEL_2) == HIGH ? "led-on" : "led-off"; }
  if(var == "ledstatus3"){ return  digitalRead(CHANNEL_3) == HIGH ? "led-on" : "led-off";}
  if(var == "ledstatus4"){ return  digitalRead(CHANNEL_4) == HIGH ? "led-on" : "led-off";}
  if(var == "checked1"){ return  digitalRead(CHANNEL_1) == HIGH ? "checked" : "";}
  if(var == "checked2"){ return  digitalRead(CHANNEL_2) == HIGH ? "checked" : "";}
  if(var == "checked3"){ return  digitalRead(CHANNEL_3) == HIGH ? "checked" : "";}
  if(var == "checked4"){ return  digitalRead(CHANNEL_4) == HIGH ? "checked" : "";} 
  return String();
}

void MyWebServer::setup(AsyncWebServer *server, const char *username, const char *password) {
  ws->onEvent([&](AsyncWebSocket * server, AsyncWebSocketClient * client, AwsEventType type, void * arg, uint8_t *data, size_t len){
    this->onWsEvent(server, client, type, arg, data, len);
  });

  server->addHandler(ws);
  events->onConnect([](AsyncEventSourceClient *client){
    client->send("hello!",NULL,millis(),1000);
  });

  server->addHandler(events);
  server->addHandler(new SPIFFSEditor(SPIFFS, username, password));

  server->on("/ajax/device", HTTP_GET, [&](AsyncWebServerRequest *request){
    AsyncJsonResponse * response = new AsyncJsonResponse();
    JsonObject& root = response->getRoot();
  	root["id"] = deviceId;
  	root["type"] = 4;
  	root["ssid"] = WiFi.SSID();
  	root["ip"] = WiFi.localIP().toString();
  	root["version"] = FIRMWARE_VERSION;
  	root["netmask"] =  WiFi.subnetMask().toString();
  	root["gateway"] = WiFi.gatewayIP().toString();
    root["ota"] = false;
    String json = "";
    response->setLength();
    request->send(response);
  });
  server->on("/scan", HTTP_GET, [&](AsyncWebServerRequest *request){
    String json = scan_network ();
    request->send(200, "text/json", json);
    json = String();
  });

  server->on("/", HTTP_GET, [&](AsyncWebServerRequest *request){
     request->send(SPIFFS, "/index.html", "text/html",  false, index_processor);
  });


  server->serveStatic("/", SPIFFS, "/").setCacheControl("max-age=86400")
    .setAuthentication(username, password);
  server->onNotFound([](AsyncWebServerRequest *request){
    Serial.printf("File Not Found");
    request->send(404, "text/html", "<b>File Not Found</b>");
  });
  server->begin();
}

void MyWebServer::sendMessage(const char* msg,  size_t len){
  lastMessage = msg;
  ws->textAll(msg, len);
}

void MyWebServer::send_event(const char *message, const char *event){
  events->send(message, event);
}

void MyWebServer::setExecuteGpio(ExecuteGpioHandler handler) {
  Serial.println("Set toggle handler");
  _handler = handler;
}
