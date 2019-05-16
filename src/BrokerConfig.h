/*
 * BrokerConfig.h
 *
 *  Created on: Jul 24, 2558 BE
 *      Author: admin
 */

#ifndef APP_BROKERCONFIG_H_
#define APP_BROKERCONFIG_H_

 #include <WiFi.h>
 #include <ArduinoJson.h>
 #include "SPIFFS.h"

#define BROKER_SETTINGS_FILE "/broker.json" // leading point for security reasons :)

struct BrokerSettingsStorage
{
	String user_name;
	String password;
	String serverHost;
	bool active;
	int serverPort;
	int updateInterval;

	void load(){
		DynamicJsonBuffer jsonBuffer;
		if (exist())
		{
      File openFile = SPIFFS.open(BROKER_SETTINGS_FILE, FILE_READ);
      size_t size = openFile.size();
      char* jsonString = new char[size + 1];
      openFile.readBytes(jsonString, size);
      JsonObject& root = jsonBuffer.parseObject(jsonString);

			JsonObject& broker = root["broker"];
			user_name = broker["user_name"].as<char*>();
			password = broker["password"].as<char*>();
			serverHost = broker["host"].as<char*>();
			active = broker["active"].as<bool>();
			updateInterval = broker["updateInterval"].as<int>();
			serverPort = broker["port"].as<int>();

			delete[] jsonString;
		}else{
			user_name = "";
			password = "";
			// serverHost = "128.199.120.225";
      serverHost = "203.185.64.8";
			active = false;
			serverPort = 1883;
			updateInterval = 30;
		}
	}

	void save(){
		DynamicJsonBuffer jsonBuffer;
		JsonObject& root = jsonBuffer.createObject();
		JsonObject& broker = jsonBuffer.createObject();
		root["broker"] = broker;
		broker["user_name"] = user_name.c_str();
		broker["password"] = password.c_str();
		broker["host"] = serverHost.c_str();
		broker["port"] = String(serverPort);
		broker["active"] = active;
		broker["updateInterval"] = updateInterval;
		//TODO: add direct file stream writing
    File saveFile =  SPIFFS.open(BROKER_SETTINGS_FILE, FILE_WRITE);
		root.printTo(saveFile);
    root.printTo(Serial);
    saveFile.close();
	}

	bool exist() {  return SPIFFS.exists(BROKER_SETTINGS_FILE); }

	void Delete(){
			SPIFFS.remove(BROKER_SETTINGS_FILE);
		}
};

static BrokerSettingsStorage BrokerSettings;

#endif /* APP_BROKERCONFIG_H_ */
