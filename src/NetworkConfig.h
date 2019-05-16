

#ifndef APP_NETWORKCONFIG_H_
#define APP_NETWORKCONFIG_H_
#define NOTE_C  261
#define NOTE_D  294
#define NOTE_E 329
#define NOTE_F  349
#define NOTE_G 392
#define NOTE_A  440
#define NOTE_B  493
#define NOTE_C 523


#include <WiFi.h>
#include <ArduinoJson.h>
#include "SPIFFS.h"

#define APP_SETTINGS_FILE "/networks.json"
struct NetwotkSettingsStorage {
  String ssid;
	String password;
	bool dhcp = true;
	IPAddress ip;
	IPAddress netmask;
	IPAddress gateway;
	bool active;

  void load() {
  		DynamicJsonBuffer jsonBuffer;
  		if (exist())
  		{
        File openFile = SPIFFS.open(APP_SETTINGS_FILE, "r");
        size_t size = openFile.size();
        char* jsonString = new char[size + 1];
        openFile.readBytes(jsonString, size);
  			JsonObject& root = jsonBuffer.parseObject(jsonString);
  			JsonObject& network = root["network"];
  			ssid = network["ssid"].as<char*>();
  		  password = network["password"].as<char*>();
  			dhcp = network["dhcp"];
  			active = network["active"];
  			ip.fromString(network["ip"].as<char*>());
  			netmask.fromString(network["netmask"].as<char*>());
  			gateway.fromString(network["gateway"].as<char*>());
        openFile.close();
        delete []jsonString;
  		}else{
  			active = false;
        dhcp = true;
        ip.fromString("192.168.1.200");
        netmask.fromString("255.255.255.0");
  		}
  }

  void save() {
    DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		JsonObject& network = jsonBuffer.createObject();
		root["network"] = network;
		network["ssid"] = ssid;
		network["password"] = password;
		network["dhcp"] = dhcp;
		network["ip"] = ip.toString();
		network["netmask"] = netmask.toString();
		network["gateway"] = gateway.toString();
		network["active"] = active;
    File saveFile =  SPIFFS.open(APP_SETTINGS_FILE, FILE_WRITE);
		root.printTo(saveFile);
    root.printTo(Serial);
    saveFile.close();
  }

  void _delete() {
    SPIFFS.remove(APP_SETTINGS_FILE);
  }

  bool exist() { return SPIFFS.exists(APP_SETTINGS_FILE); }
};

static NetwotkSettingsStorage NetworkSettings;

#endif
