#include <ESP8266WiFi.h>
#include <IBMIOTDevice2Gateway.h>

#include <OLED32.h>
#include <Wire.h>
#include <DHT.h>

String user_html = ""
    "<p><input type='text' name='edgeAddress' placeholder='edgeAddress'>"
    "<p><input type='text' name='devType' placeholder='Device Type'>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

char*               ssid_pfix = (char*)"Johnny_IoT_A";
unsigned long       pubInterval;
unsigned long       lastPublishMillis = 0;

unsigned long lastClicked = 0;
int           clicked =0;
const int pulseA = 12;
const int pulseB = 13;
const int pushSW = 2;
volatile int lastEncoded =0;
volatile int encoderValue = 70;

OLED display(4,5);
#define DHTTYPE DHT22
#define DHTPIN 14
DHT dht(DHTPIN,DHTTYPE,11);

float humidity;
float temp_c;
unsigned long lastDHTReadMillis =0;
#define interval 2000
float tgtT;


float myMap(float t, int s0,int e0, int s1, int e1){
      float ret = (((float)t-s0)/(e0-s0))*(e1-s1) + s1;
      if(ret <= s1) ret = s1;
      if(ret >=e1) ret = e1;
      return ret;
}
void handleRotary(){
  // Never put any long instruction 
  int MSB = digitalRead(pulseA);
  int LSB = digitalRead(pulseB);

  int encoded = (MSB<<1)|LSB;
  int sum = (lastEncoded <<2) | encoded;
  if( sum == 0b1101 || sum == 0b0100 || sum == 0b0010 || sum == 0b1011) encoderValue++;
  if( sum == 0b1110 || sum == 0b0111 || sum == 0b0001 || sum == 0b1000) encoderValue--;
  lastEncoded= encoded;
  if(encoderValue >255){
    encoderValue = 255;
  }
  else if(encoderValue <0){
    encoderValue = 0;
  }
  lastPublishMillis = millis() - pubInterval + 200;
}
void publishData() {
    StaticJsonBuffer<512> jsonOutBuffer;
    JsonObject& root = jsonOutBuffer.createObject();
    JsonObject& data = root.createNestedObject("d");

    // data["temperature"]  = yourData;
    if(gettemperature()){
      char dht_buffer[10];
      char dht_buffer2[10];
      sprintf(dht_buffer,"%2.1f",temp_c);
      display.print(dht_buffer,2,10);
      data["temperature"] = dht_buffer;
      tgtT = myMap(encoderValue,0,255,10,50);
      sprintf(dht_buffer2,"%2.1f",tgtT);
      data["target"] = dht_buffer2;
      display.print(dht_buffer2,3,10);
      root.printTo(msgBuffer, sizeof(msgBuffer));
      client.publish(publishTopic, msgBuffer);
    }

}

void handleUserCommand(JsonObject* root) {
    JsonObject& d = (*root)["d"];
    // user command

    if(d.containsKey("target")){
      tgtT = atof(d["target"]);
      encoderValue = myMap(tgtT,10,50,0,255);
      lastPublishMillis =0; 
    }
}

void message(char* topic, byte* payload, unsigned int payloadLength) {
    StaticJsonBuffer<512> jsonInBuffer;
    char strBuf[200];
    int strLen; 
    byte2buff(msgBuffer, payload, payloadLength);
    JsonObject& root = jsonInBuffer.parseObject((char*)msgBuffer);
    if (!root.success()) {
        Serial.println("handleCommand: payload parse FAILED");
        return;
    }
    handleIOTCommand(topic, &root);
    sprintf(strBuf,"iot-2/type/%s/id/%s/cmd/",(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    strLen = strlen(strBuf);
    if (!strcmp(updateTopic, topic)) {
        pubInterval = (*cfg)["meta"]["pubInterval"];
    } else if (!strncmp(strBuf, topic, strLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
}

void setup() {
    Serial.begin(115200);
    pinMode(pushSW,INPUT_PULLUP);
    pinMode(pulseA, INPUT_PULLUP);
    pinMode(pulseB, INPUT_PULLUP);
    attachInterrupt(pushSW,buttonClicked, FALLING);
    attachInterrupt(pulseA,handleRotary,CHANGE);
    attachInterrupt(pulseB,handleRotary,CHANGE);
    display.begin();
    display.print("IoT Thermostat");
    display.print("Current :",2,2);
    display.print("Target :",3,2);
    
    initCheck();
    // *** If no "config" is found or "config" is not "done", run configDevice ***
    if(!cfg->containsKey("config") || strcmp((const char*)(*cfg)["config"], "done")) {
         configDevice();
    }
    
    sprintf(publishTopic,t_publishTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(infoTopic,t_infoTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(commandTopic,t_commandTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(responseTopic,t_responseTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(manageTopic,t_manageTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(updateTopic,t_updateTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(rebootTopic,t_rebootTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    sprintf(resetTopic,t_resetTopic,(const char*)(*cfg)["devType"],(const char*)(*cfg)["devId"]);
    
    WiFi.mode(WIFI_STA);    
    WiFi.begin((const char*)(*cfg)["ssid"], (const char*)(*cfg)["w_pw"]);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    // main setup
    Serial.printf("\nIP address : "); Serial.println(WiFi.localIP());
    JsonObject& meta = (*cfg)["meta"];
    pubInterval = meta.containsKey("pubInterval") ? atoi((const char*)meta["pubInterval"]) : 0;

    sprintf(iot_server, "%s", (const char*)(*cfg)["edgeAddress"]);
    if (!espClient.connect(iot_server, 1883)) {
        Serial.print((const char*)iot_server);
        Serial.println(" connection failed");
        return;
    }
    client.setServer(iot_server, 1883);   //IOT
    client.setCallback(message);

    while(!client.connected()){
        Serial.println("Connecting to MQTT ...");
        if(client.connect("ESP8266Client_A")){
          Serial.println("Connected");
        } else {
          Serial.print("Failed with state "); Serial.println(client.state());
          delay(2000);
        }
    }

    subscribeTopic(responseTopic);
    subscribeTopic(rebootTopic);
    subscribeTopic(resetTopic);
    subscribeTopic(updateTopic);
    subscribeTopic(commandTopic);
}

void loop() {
    if (!client.connected()) {        
        Serial.println("Connecting to MQTT ...");
        if(client.connect("ESP8266Client_A")){
          Serial.println("Connected");
          
          subscribeTopic(responseTopic);
          subscribeTopic(rebootTopic);
          subscribeTopic(resetTopic);
          subscribeTopic(updateTopic);
          subscribeTopic(commandTopic);
        } else {
          Serial.print("Failed with state "); Serial.println(client.state());
          delay(2000);
        }
    }
    client.loop();
    if ((pubInterval != 0) && (millis() - lastPublishMillis > pubInterval)) {
        publishData();
        lastPublishMillis = millis();
    }
}

void buttonClicked(){
  if(millis() - lastClicked >= 300){
    lastClicked = millis();
    clicked = 1;
    lastPublishMillis = 0;
  }
}

bool gettemperature(){
  unsigned long currentMillis = millis();
  if(currentMillis - lastDHTReadMillis >= interval){
    lastDHTReadMillis = currentMillis;
      humidity = dht.readHumidity();
      temp_c = dht.readTemperature(false);
      if(isnan(humidity) || isnan(temp_c))
        return false;
    return true;
  }
  return false;
}
