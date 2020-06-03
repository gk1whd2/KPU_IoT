#include <ESP8266WiFi.h>
#include <IBMIOTDevice2Gateway.h>

String user_html = ""
    "<p><input type='text' name='edgeAddress' placeholder='edgeAddress'>"
    "<p><input type='text' name='devType' placeholder='Device Type'>"
    "<p><input type='text' name='devId' placeholder='Device Id'>"
    "<p><input type='text' name='meta.pubInterval' placeholder='Publish Interval'>";

char*               ssid_pfix = (char*)"Johnny_IoT_B";
unsigned long       pubInterval;
unsigned long       lastPublishMillis = 0;
const int RELAY = 15;


void publishData() {
    StaticJsonBuffer<512> jsonOutBuffer;
    JsonObject& root = jsonOutBuffer.createObject();
    JsonObject& data = root.createNestedObject("d");

    // data["temperature"]  = yourData;
    data["valve"] = digitalRead(RELAY) ? "on" : "off";
    root.printTo(msgBuffer, sizeof(msgBuffer));
    client.publish(publishTopic, msgBuffer);
}

void handleUserCommand(JsonObject* root) {
    JsonObject& d = (*root)["d"];
    // user command

    if(d.containsKey("valve")){
      if(!strcmp(d["valve"],"on")){
        digitalWrite(RELAY,HIGH);
      }else{
        digitalWrite(RELAY,LOW);
      }
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
    } else if (!strncmp(strBuf, topic,strLen)) {            // strcmp return 0 if both string matches
        handleUserCommand(&root);
    }
    else{
      Serial.print("undefined Topic : ");
      Serial.println(topic);
    }
      Serial.print("Receive Topic : ");
      Serial.println(topic);
}

void setup() {
    Serial.begin(115200);
    pinMode(RELAY,OUTPUT);
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
    if(!espClient.connect(iot_server, 1883)) {        
      Serial.println("connection failed");
      return;
    }
    client.setServer(iot_server, 1883);   //IOT
    client.setCallback(message);

    while(!client.connected()){
        Serial.println("Connecting to MQTT ...");
        if(client.connect("ESP8266Client_B")){
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
        if(client.connect("ESP8266Client_B")){
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
