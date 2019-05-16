//#define TEMP_SENSOR

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <SPIFFS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <AsyncMqttClient.h>
#include <M5Stack.h>
#include <time.h>
#ifdef TEMP_SENSOR
#include "DHTesp.h"
#endif
#include "MyWebServer.h"
#include "NetworkConfig.h"
#include "BrokerConfig.h"
#include "m5stack_logo.h"

#ifndef WIFI_SSID
	//#define WIFI_SSID "see_dum"
	//#define WIFI_PWD "0863219053"
  //#define WIFI_SSID "HUAWEI"
	//#define WIFI_PWD "0000000000"
  // #define WIFI_SSID "DESS_MAKER"
	// #define WIFI_PWD "1212312121"
  // #define WIFI_SSID "Horizon_Plus"
  // #define WIFI_PWD "1q2w3e4r"
  #define WIFI_SSID "NSTDA-D"
	#define WIFI_PWD "i0t#dEsS"
#endif

// #define MQTT_HOST 		IPAddress(128,199,120,225)
// #define MQTT_PORT 		1883
// #define mqtt_user 		"TEST"
// #define mqtt_password   "12345"

#define MQTT_HOST 		IPAddress(203,185,64,8)
#define MQTT_PORT 		1883
#define mqtt_user 		"admin"
#define mqtt_password   "public"

// #define MQTT_HOST 		IPAddress(10,226,49,113)
// #define MQTT_PORT 		1883
// #define mqtt_user 		"horizon"
// #define mqtt_password   "1212312121"
#define LED_PIN         5
#define STACK_SIZE    1024

const char* ntpServer = "time.navy.mi.th";
const long  gmtOffset_sec = 3600 * 7;
const int   daylightOffset_sec = 0;

// SKETCH BEGIN
AsyncWebServer server(80);
MyWebServer myWeb;
AsyncMqttClient mqttClient;
#ifdef TEMP_SENSOR
#define DHTTYPE   		DHTesp::DHT22
#define DHTPIN    		18
DHTesp dht;
#endif

unsigned long previousMillis = 0;     // will store last time LED was updated
const long interval = 1000 * 10;
float t = 0;
float h = 0;
uint64_t  chipid=ESP.getEfuseMac();
const String deviceId =  String((uint16_t)(chipid>>32), HEX) + String((uint32_t)chipid, HEX);
bool isWiFiReset = false;
bool isSmartConfig = false;

const char * hostName = "esp-async";
const char* http_username = "";
const char* http_password = "";
const String statusTopic = "/device/"+deviceId+"/status";
const String statusGetTopic = "/device/"+deviceId+"/status/get";
const String deviceTopic = "/device/"+deviceId+"/properties";
const String deviceResultTopic = "/device/"+deviceId+"/properties/result";
const String toggleTopic = "/device/"+deviceId+"/io/toggle";
const String willTopic = "/device/"+deviceId+"/will";
const String sessionTopic = "/device/"+deviceId+"/session";
const String reset = "/device/"+deviceId+"/reset";


// const String base_url = "http://10.226.49.113:1880/sharing/";
// const String base_url = "http://eessmaker.cf/sharing/";
const String base_url = "http://203.185.64.8:1880/sharing/";

char *letters = "abcdefghijklmnopqrstuvwxyz0123456789";
String session_key = "";

QueueHandle_t     qu_task;

void connectToMqtt();
#ifdef TEMP_SENSOR
void read_tem();
#endif
void toggle(int pin);
void reset_wifi();

void show_qrcode() {
  M5.Lcd.qrcode(base_url + deviceId + "/" + session_key);
}

#ifdef TEMP_SENSOR
void vDhtTask(void * pvParameters){
  while(true){
    read_tem();
    vTaskDelay(10000 / portTICK_PERIOD_MS);
  }
}
#endif

void printLocalTime(void *pvParameters)
{
  while(true) {
    struct tm timeinfo;
    if(!getLocalTime(&timeinfo)){
      Serial.println("Failed to obtain time");
      return;
    }
    // Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
    Serial.println(&timeinfo, "%a, %d/%m/%Y %H:%M:%S");
    // Serial.println(timeinfo.tm_hour);
    if((timeinfo.tm_hour==17&&timeinfo.tm_min==00&&timeinfo.tm_sec==00)||(timeinfo.tm_hour==23&&timeinfo.tm_min==59&&timeinfo.tm_sec==59)||(timeinfo.tm_hour==6&&timeinfo.tm_min==00&&timeinfo.tm_sec==00)){
      digitalWrite(CHANNEL_1,0);
      digitalWrite(CHANNEL_2,0);
      digitalWrite(CHANNEL_3,0);
      digitalWrite(CHANNEL_4,0);
      ESP.restart();
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

void vBlinkTask(void *pvParameters){
  int value = (int)pvParameters;
  while(true){
    if (xQueueReceive(qu_task, &value, 0)){
      Serial.println("Update period task: " + String(value));
    }
    if (value == 3000) {
      digitalWrite(LED_PIN, HIGH);
      vTaskDelay(100 / portTICK_PERIOD_MS);
      digitalWrite(LED_PIN, LOW);
      vTaskDelay(value / portTICK_PERIOD_MS);
    }else{
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      vTaskDelay(value / portTICK_PERIOD_MS);
    }
  }
}

void vBtnTask (void* pvParameter) {
int i,j,k = 1;

  while(true) {
    if (M5.BtnA.isPressed()&&i==1) {
      Serial.println("BtnA.wasPressed");
      Serial.println(digitalRead(CHANNEL_2));
      toggle(CHANNEL_2);
      i = 0;
    }
    if (M5.BtnB.isPressed()&&j==1) {
      Serial.println("BtnB.wasPressed");
      Serial.println(digitalRead(CHANNEL_4));
      toggle(CHANNEL_4);
      j = 0;
    }
    if (M5.BtnC.isPressed()&&k==1) {
      Serial.println("BtnC.wasPressed");
      Serial.println(digitalRead(CHANNEL_3));
      toggle(CHANNEL_3);
      k = 0;
    }

    if (M5.BtnA.isReleased()) {
        i = true;
        // M5.Speaker.mute();
    }
    if (M5.BtnB.isReleased()) {
        j = true;
        // M5.Speaker.mute();
    }
    if (M5.BtnC.isReleased()) {
        k = true;
        // M5.Speaker.mute();
    }

    // Serial.println("vBtnTask");
    // if (M5.BtnA.pressedFor(2000)&&j==true) {
    //   Serial.println("BtnA.wasPressed");
    //   Serial.println("setBrightness 0");
    //     j = false;
    //     M5.Lcd.setBrightness(0);
    //     // M5.Speaker.tone(1200,200);
    // }

    // if (M5.BtnB.isReleased()&&i==true) {
    //   Serial.println("BtnB.wasPressed");
    //   Serial.println(digitalRead(CHANNEL_4));
    //   toggle(CHANNEL_4);
    //
    // }
    // if (M5.BtnC.isPressed()&&i==true) {
    //   Serial.println("BtnA.wasPressed");
    //   Serial.println(digitalRead(CHANNEL_3));
    //   toggle(CHANNEL_3);
    //
    // }

    if (M5.BtnB.pressedFor(2000)) {
        Serial.println("BtnC.waspressedFor(2000)");
        Serial.println("WiFi reset configuration");
        ESP.restart();
        //M5.powerOFF();
    }

     // if (M5.BtnB.pressedFor(2000)&&j==true) {
     //     Serial.println("BtnB.wasPressed");
     //     // Serial.println("WiFi reset configuration");
     //     // reset_wifi();
     //     // ESP.restart();
     //     // M5.powerOFF();
     //     // M5.Lcd.writecommand(ILI9341_DISPOFF);
     //     M5.Lcd.setBrightness(255);
     //     show_qrcode();
     //     j = false;
     //     //M5.Speaker.tone(1200,200);
     // }

    vTaskDelay(50);
  }
}


void toggle_status(int pin){
  //String deviceJson = "{ \"id\": \""+ deviceId +"\", \"type\": 1, \"value\": " + String(digitalRead(pin)) + ", \"gpio\": "+String(pin)+" }";
  DynamicJsonBuffer jsonBuffer;
	JsonObject& device = jsonBuffer.createObject();
  device["id"] = deviceId;
  device["type"] = 1;
  device["value"] = digitalRead(pin);
  device["gpio"] = pin;
  String deviceJson = "";
	device.printTo(deviceJson);
	mqttClient.publish(statusTopic.c_str(),  1, true, deviceJson.c_str()); // topic, qos, rtain, payload
	myWeb.sendMessage(deviceJson.c_str(), deviceJson.length());
}

void toggle(int pin) {
  digitalWrite(pin, !digitalRead(pin));
  toggle_status(pin);
}

//format bytes
String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

void reset_wifi(){
	if (M5.BtnA.isPressed()) {
		if (isWiFiReset) {
			NetworkSettings.active = false;
			NetworkSettings.save();
			ESP.restart();
		}
	} else {
		isWiFiReset = false;
	}
}

#ifdef TEMP_SENSOR
void read_tem() {
    TempAndHumidity newValues = dht.getTempAndHumidity();
	// Check if any reads failed and exit early (to try again).
	if (dht.getStatus() != 0) {
		Serial.println("DHT22 error status: " + String(dht.getStatusString()));
		return;
	}

    t = newValues.temperature;
    h = newValues.humidity;

    String json = "{";
        json += "\"type\": 2";
        json += ", \"id\": \""+deviceId+"\"";
        json += ", \"temperature\": "+String(t);
        json += ", \"humidity\": "+String(h);
        json += ", \"heap\": "+String(ESP.getFreeHeap());
        json += ", \"value\": "+ String(digitalRead(RELAY_PIN));
        json += "}";
    Serial.println("Publish: "+json);
        if (mqttClient.connected()){
        mqttClient.publish(statusTopic.c_str(), 1, true, json.c_str());
    }else{
            Serial.println("mqtt client disconnected from broker");
        }
    myWeb.textAll(json.c_str(), json.length());
}
#endif

void send_device_properties() {
	DynamicJsonBuffer jsonBuffer;
	JsonObject& device = jsonBuffer.createObject();
	device["id"] = deviceId;
	device["type"] = 4;
	device["ssid"] = WiFi.SSID();
	device["ip"] = WiFi.localIP().toString();
	device["netmask"] = WiFi.subnetMask().toString();
	device["gateway"] = WiFi.gatewayIP().toString();
	device["version"] = FIRMWARE_VERSION;
	device["ota"] = false;
	String deviceJson = String();
	device.printTo(deviceJson);
	mqttClient.publish(deviceResultTopic.c_str(), 1, true, deviceJson.c_str());
	Serial.println("Send device property: " + String(deviceJson.length()));
	Serial.println(deviceResultTopic +", "+deviceJson);
}

void onMqttConnect(bool sessionPresent) {
	Serial.println("Connected to MQTT.");
  Serial.print("Session present: ");
  Serial.println(sessionPresent);
	mqttClient.subscribe (deviceTopic.c_str(), 0);
	mqttClient.subscribe (toggleTopic.c_str(), 0);
  mqttClient.subscribe (statusGetTopic.c_str(), 0);
  mqttClient.subscribe (reset.c_str(), 0);
	send_device_properties();
  toggle_status(CHANNEL_1);
  toggle_status(CHANNEL_2);
  toggle_status(CHANNEL_3);
  toggle_status(CHANNEL_4);
  int value = 3000;
  xQueueSend(qu_task, &value, 10);


  for(int i = 0; i<15; i++) {
    int randomValue = random(0, 36);
    char letter = letters[randomValue];
    session_key += String(letter);
  }
  Serial.println("Session Key: " + session_key);
  DynamicJsonBuffer jsonBuffer;
	JsonObject& session = jsonBuffer.createObject();
	session["deviceid"] = deviceId;
  session["session"] = session_key;
  String sessionJson = String();
	session.printTo(sessionJson);
  mqttClient.publish(sessionTopic.c_str(), 0, false, sessionJson.c_str());
  show_qrcode();
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  Serial.println("Disconnected from MQTT.");
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  connectToMqtt();
  if (reason == AsyncMqttClientDisconnectReason::TLS_BAD_FINGERPRINT) {
    Serial.println("Bad server fingerprint.");
  }
}

void onMqttSubscribe(uint16_t packetId, uint8_t qos) {
  Serial.println("Subscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
  Serial.print("  qos: ");
  Serial.println(qos);
}

void onMqttUnsubscribe(uint16_t packetId) {
  Serial.println("Unsubscribe acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttPublish(uint16_t packetId) {
  Serial.println("Publish acknowledged.");
  Serial.print("  packetId: ");
  Serial.println(packetId);
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
	Serial.print(topic);
	Serial.print(":\r\n\t");
	Serial.println((char*)payload);
	DynamicJsonBuffer jsonBuffer;
	JsonObject& root = jsonBuffer.parseObject(payload);
	if (root.success()) {
		if (String(topic) == deviceTopic) {
			send_device_properties();
		}
		if (String(topic) == toggleTopic) {
      int gpio = root["gpio"].as<int>();
			toggle(gpio);
		}

    if (String(topic) == statusGetTopic) {
      toggle_status(CHANNEL_1);
      toggle_status(CHANNEL_2);
      toggle_status(CHANNEL_3);
      toggle_status(CHANNEL_4);
    }
	}
  if (String(topic) == reset) {
    String id = root["id"];
    Serial.print(id+" : ");
    Serial.println(deviceId);
    if(id == deviceId)
    {
      Serial.println("Reset");
      // int i = (int)payload;
      int i = root["reset"].as<int>();
      if(i == 1)
      {
        Serial.println("Reset Form network");
        ESP.restart();
      }
    }
  }
}

void connectToMqtt() {
	mqttClient.connect();
}

void webserver_config() {
  myWeb.setup(&server, http_username, http_password);
	myWeb.setExecuteGpio([](uint8_t gpio) {
		toggle(gpio);
	});
}

void mqttclient_config() {
  BrokerSettings.load();
  mqttClient.onConnect(onMqttConnect);
	mqttClient.onDisconnect(onMqttDisconnect);
	mqttClient.onSubscribe(onMqttSubscribe);
	mqttClient.onUnsubscribe(onMqttUnsubscribe);
	mqttClient.onMessage(onMqttMessage);
	mqttClient.onPublish(onMqttPublish);
	mqttClient.setClientId(deviceId.c_str() );
	String willPayload = "{ \"id\": \""+deviceId+"\", \"msg\": \"The connection from this device is lost:(\" }";
	mqttClient.setKeepAlive(20).setWill(willTopic.c_str(), 1, true, willPayload.c_str());
	mqttClient.setServer(BrokerSettings.serverHost.c_str(), BrokerSettings.serverPort);
}

void ota_config() {
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");
  ArduinoOTA.setPort(3232);
  ArduinoOTA.setHostname("esp32");
  /* we set password for updating */
  ArduinoOTA.setPassword("iotsharing");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";
    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
}

void server_config() {
  ota_config();
  webserver_config();
  mqttclient_config();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);
    File root = fs.open(dirname);
    if(!root){
        Serial.println("Failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        return;
    }
    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.name(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("  SIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
}

void WiFiEvent(WiFiEvent_t event)
{
    // M5.Lcd.fillScreen(WHITE);
    // M5.Lcd.setCursor(190, 0);
    // M5.Lcd.setTextColor(BLACK);
    // M5.Lcd.setTextSize(1);
    // M5.Lcd.printf("[WiFi-event] event: %d\n", event);
    Serial.printf("[WiFi-event] event: %d\n", event);
    switch(event) {
        case SYSTEM_EVENT_STA_STOP:
        break;

        case SYSTEM_EVENT_STA_START:
        server_config();
        break;
        case SYSTEM_EVENT_WIFI_READY:
        break;
        case SYSTEM_EVENT_SCAN_DONE:
        break;
        case SYSTEM_EVENT_STA_GOT_IP: {
            Serial.println("WiFi got ip address");
            Serial.println("IP address: ");
            Serial.printf("[WiFi-event] event: %d\n", event);
            Serial.println(WiFi.localIP());
            ArduinoOTA.begin();
            connectToMqtt();
            int value = 500;
            xQueueSend(qu_task, &value, 10);
            configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
             xTaskCreatePinnedToCore(printLocalTime, "printLocalTime", STACK_SIZE * 2,  NULL, tskIDLE_PRIORITY+3, NULL, 0);
             // vTaskDelay(2000 / portTICK_PERIOD_MS);
             // WiFi.begin(WIFI_SSID,WIFI_PWD);
            break;
        }
        case SYSTEM_EVENT_STA_DISCONNECTED:{
          struct tm timeinfo;
          if(!getLocalTime(&timeinfo)){
            Serial.println("Failed to obtain time");
            return;
          }
            Serial.println("WiFi lost connection");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            WiFi.begin(WIFI_SSID,WIFI_PWD);
            Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
        }
        case SYSTEM_EVENT_STA_LOST_IP:{
            Serial.println("WiFi lost IP");
            vTaskDelay(2000 / portTICK_PERIOD_MS);
            WiFi.begin(WIFI_SSID,WIFI_PWD);
        }
            break;
        break;
    }
}

void wifi_config() {
#ifndef WIFI_SSID
  NetworkSettings.load();
  if (NetworkSettings.active) {
	  WiFi.onEvent(WiFiEvent);
    WiFi.setAutoConnect(true);
    WiFi.setAutoReconnect(true);
    Serial.println("WiFi load config");
    Serial.println(NetworkSettings.active);
    Serial.println(NetworkSettings.ssid);
    Serial.println(NetworkSettings.password);
    Serial.println(NetworkSettings.dhcp);
    Serial.println(NetworkSettings.ip);
    Serial.println(NetworkSettings.netmask);
    Serial.println(NetworkSettings.gateway);
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", NetworkSettings.ssid);
    if (String(WiFi.SSID()) != NetworkSettings.ssid) {
      WiFi.begin(NetworkSettings.ssid.c_str(), NetworkSettings.password.c_str());
    }
  } else {
    WiFi.mode(WIFI_STA);
    Serial.println("Start SmartConfig...");
    if (WiFi.beginSmartConfig()){
      while (!WiFi.smartConfigDone()) {
        delay(500);
        Serial.print(".");
      }
      Serial.println();
      Serial.println("SmartConfig done");
      int value = 500;
      xQueueSend(qu_task, &value, 10);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
      }
      server_config();
      connectToMqtt();
      Serial.println();
      NetworkSettings.active = true;
      NetworkSettings.ssid = WiFi.SSID();
      NetworkSettings.password = WiFi.psk();
      NetworkSettings.dhcp = true;
      NetworkSettings.ip = WiFi.localIP();
      NetworkSettings.netmask = WiFi.subnetMask();
      NetworkSettings.gateway = WiFi.gatewayIP();
      NetworkSettings.save();
      Serial.println("");
      Serial.println("WiFi SmartConfig connected");
      Serial.println(NetworkSettings.active);
      Serial.println(NetworkSettings.ssid);
      Serial.println(NetworkSettings.password);
      Serial.println(NetworkSettings.dhcp);
      Serial.println(NetworkSettings.ip);
      Serial.println(NetworkSettings.netmask);
      Serial.println(NetworkSettings.gateway);
    }
  }
#else
    WiFi.onEvent(WiFiEvent);
    Serial.println("WiFi load config");
    WiFi.mode(WIFI_STA);
    Serial.printf("Connecting to %s\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PWD);
#endif
}

void setup() {
    // put your setup code here, to run once:
    Serial.begin(115200);
    Serial.print ("ESP32 cpu frequency: ");
	  Serial.println(ESP.getCpuFreqMHz());
    Serial.setDebugOutput(false);
    M5.begin();
    //M5.Lcd.drawBitmap(0, 0, 320, 240, (uint16_t *)gImage_logoM5);
    M5.Lcd.fillScreen(WHITE);

    // M5.Speaker.tone(NOTE_G,200);
    // delay(200);
    // M5.Speaker.tone(NOTE_A,200);
    // delay(200);

    M5.Speaker.mute();

    Serial.println("M5Strack initiated");

    pinMode(CHANNEL_1, OUTPUT);
    pinMode(CHANNEL_2, OUTPUT);
    pinMode(CHANNEL_3, OUTPUT);
    pinMode(CHANNEL_4, OUTPUT);

    if(!SPIFFS.begin(true)){
        Serial.println("SPIFFS Mount Failed");
        while(true);
    }
    listDir(SPIFFS, "/", 0);
#ifdef TEMP_SENSOR
    dht.setup(DHTPIN, DHTTYPE);
	  Serial.println("DHT initiated");
#endif

    qu_task = xQueueCreate(10, sizeof(int));
#ifdef TEMP_SENSOR
    xTaskCreatePinnedToCore(vDhtTask, "vDhtTask", STACK_SIZE * 2,  NULL, tskIDLE_PRIORITY-1, NULL, 1);
#endif
    xTaskCreatePinnedToCore(vBlinkTask, "vBlinkTask", STACK_SIZE,  (void*)100, tskIDLE_PRIORITY+2, NULL, 1);
    xTaskCreatePinnedToCore(vBtnTask, "vBtnTask", STACK_SIZE * 2,  NULL, tskIDLE_PRIORITY+2, NULL, 0);
    //xTaskCreatePinnedToCore(vBtnBTask, "vBtnBTask", STACK_SIZE * 2,  NULL, tskIDLE_PRIORITY+2, NULL, 0);
    wifi_config();
    M5.setWakeupButton(BUTTON_B_PIN);


}

void loop() {
    // put your main code here, to run repeatedly:std::cerr << "/* error message */" << '\n';
    ArduinoOTA.handle();
    M5.update();

}
