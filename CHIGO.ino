#include <ESP8266WiFi.h>
//#include <ESP8266WiFiMulti.h>
#include <PubSubClient.h>
#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <ir_Goodweather.h>
#include <FS.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include "DHT.h"


#define DHTTYPE DHT22
#define DHTPIN D3
#define Bz D7

/*
  #define WIFI_SSID1 "Baby Ancestor"
  #define WIFI_PASSWORD1 "bbancestor2020$@@"
  //
  #define WIFI_SSID "HOME436UP"
  #define WIFI_PASSWORD "home4362020$"
*/

const uint16_t kIrLed = 4;
IRGoodweatherAc ac(kIrLed);




char mqtt_server[40];
char mqtt_port[6] = "1883";
//default custom static IP


//const char* mqtt_server = "xc23e826.en.emqx.cloud";

//const char* mqtt_server = "192.168.20.105";

WiFiClient espClient;
PubSubClient client(espClient);


unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;
char result;
int i;
String recv_packet = " ";

//ESP8266WiFiMulti wifiMulti;

int setcool = 16;
bool state;
int count = 0;
int count1 = 0;
int state1;

unsigned int upper_limit = 30; // default  state
unsigned int lower_limit = 24; // default state

DHT dht(DHTPIN, DHTTYPE);

bool shouldSaveConfig = false;

//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}



void printState() {
  // Display the settings.
  Serial.println("Khino A/C remote is in the following state:");
  Serial.printf("  %s\n", ac.toString().c_str());
}


void setup_wifi() {
  //  delay(10);
  //  Serial.println();
  //  Serial.print("Connecting to ");
  //  // Serial.println(WIFI_SSID);
  //  WiFi.mode(WIFI_STA);
  //
  //  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);
  //  wifiMulti.addAP(WIFI_SSID1, WIFI_PASSWORD1);
  //
  //  while (wifiMulti.run() != WL_CONNECTED) {
  //    delay(500);
  //    Serial.print(".");
  //  }
  //
  //  randomSeed(micros());
  //  Serial.println("");
  //  Serial.println("WiFi connected");
  //  Serial.println("IP address: ");
  //  Serial.println(WiFi.localIP());
  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(mqtt_server, json["mqtt_server"]);
          strcpy(mqtt_port, json["mqtt_port"]);

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  Serial.println(mqtt_server);


  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);


  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  //set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);


  //reset settings - for testing
  //wifiManager.resetSettings();

  //set minimu quality of signal so it ignores AP's under that quality

  //defaults to 8%
  wifiManager.setMinimumSignalQuality();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  //wifiManager.setTimeout(120);

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect("SmartIRBridge", "0000")) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  Serial.println("connected... :)");

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());


  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["mqtt_server"] = mqtt_server;
    json["mqtt_port"] = mqtt_port;

    json["ip"] = WiFi.localIP().toString();
    json["gateway"] = WiFi.gatewayIP().toString();
    json["subnet"] = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
  }

  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());


}

void callback(char* topic, byte* payload, unsigned int length) {
  payload[length] = '\0'; // this is the main point
  String Top = topic;
  recv_packet = String((char *)payload);

  Serial.print(topic);
  Serial.print(" : ");
  Serial.println(recv_packet);

  if (strcmp(topic, "cmnd/AC/Fan") == 0) {
    if (recv_packet == "high") {

      ac.setFan(kGoodweatherFanHigh);
      ac.send();

    }
    else if (recv_packet == "medium") {


      ac.setFan(kGoodweatherFanMed);
      ac.send();


    }
    else if (recv_packet == "low") {

      ac.setFan(kGoodweatherFanLow);
      ac.send();

    }
  }

  if (strcmp(topic, "cmnd/AC/Swing") == 0) {
    if (recv_packet == "1") {

      ac.setSwing(kGoodweatherSwingFast);
      ac.send();

    }
    else if (recv_packet == "0") {

      ac.setSwing(kGoodweatherSwingOff);
      ac.send();

    }
  }


  if (strcmp(topic, "cmnd/AC/Power") == 0) {
    Serial.println (topic);
    if (recv_packet == "on") {
      Serial.println("ac on");
      client.publish("stat/AC/Result", "on");
      //  client.publish("cmnd/4channel_1CFE7E/POWER1", "ON");
      ac.on();
      ac.send();


    }
    else if (recv_packet == "off") {
      Serial.println("ac off");
      client.publish("stat/AC/Result", "off");

      ac.off();
      ac.send();


    }
  }
  else if (strcmp(topic, "data/range/result") == 0) {
    setcool = recv_packet.toInt();

    ac.setTemp(setcool);
    ac.send();

  }
  else if (strcmp(topic, "cmnd/AC/Upper") == 0) {
    count1 = 0;
    upper_limit = recv_packet.toInt();
  }
  else if (strcmp(topic, "cmnd/AC/Lower") == 0) {
    count = 0;
    lower_limit = recv_packet.toInt();
  }

}





void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    String clientId = "AC_IR_Bridge-";
    clientId += String(random(0xffff), HEX);
    digitalWrite(Bz, HIGH);
    delay(500);
    digitalWrite(Bz, LOW);
    delay(500);

    if (client.connect(clientId.c_str(), "admin", "admin")) {
      Serial.println("connected");
      // publish the data
      client.subscribe("cmnd/AC/Power");
      client.subscribe("data/range/result");
      client.subscribe("stat/AC/Result");
      client.subscribe("cmnd/AC/Upper");
      client.subscribe("cmnd/AC/Lower");
      client.subscribe("cmnd/AC/Fan");
      client.subscribe("cmnd/AC/Swing");
      client.subscribe("stat/room/temp");



    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);
  setup_wifi();
  pinMode(Bz, OUTPUT);
  client.setServer(mqtt_server, 1883);
  delay(1000);
  Serial.println(mqtt_server);
  client.setCallback(callback);


  dht.begin();
  ac.begin();

  Serial.println("Default state of the remote.");
  printState();
  Serial.println("Setting initial state for A/C.");
  ac.off();
  ac.setTemp(setcool);
  ac.send();
  ac.setFan(kGoodweatherFanLow);
  ac.send();
  ac.setMode(kGoodweatherCool);
  ac.send();
  ac.setSwing(kGoodweatherSwingSlow);
  ac.send();
  printState();


}

void loop() {

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  float f = dht.readTemperature(true);
  //
  //  if (isnan(h) || isnan(t) || isnan(f)) {
  //    Serial.println(F("Failed to read from DHT sensor!"));
  //    return;
  //  }

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // debug_temperature
    /*
      Serial.print(F("Humidity: "));
      Serial.print(h);
      Serial.print(F("% | Temperature: "));
      Serial.println(t);
    */

    // to use c++ declaration c_str for convert string

    client.publish("stat/room/temp", String(t).c_str(), true);

    /*
      client.publish("stat/range/result", String(setcool).c_str(), true);
      client.publish("stat/AC/Upper", String(lower_limit).c_str(), true);
      client.publish("stat/AC/Lower", String(upper_limit).c_str(), true);
    */

    /*
      Serial.print(F("UpperLimit: "));
      Serial.print(upper_limit);
      Serial.print(F("| | LowerLimit: "));
      Serial.println(lower_limit);
    */

    if (t <= lower_limit) {
      Serial.println(count);
      count++;
      state = true;
      if (state == true && count == 4) {
        client.publish("cmnd/AC/Power", "off");
        delay(5000);
        client.publish("cmnd/AC/Power", "off");
        state = false;
      }

    }
    else count = 0;


    if (t >= upper_limit) {
      count1++;
      Serial.println(count1);
      state = false;
      if (state == false && count1 == 4) {
        client.publish("cmnd/AC/Power", "on");
        delay(5000);
        client.publish("cmnd/AC/Power", "on");
        state = true;
      }
    }
    else count1 = 0;

  }
}
