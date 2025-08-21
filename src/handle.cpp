#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "handle.h"

const char *ssid = "Galaxy"; //shuttleC
const char *password = "halamadrid"; //thaco@1234
const char *mqtt_server = "broker.hivemq.com"; // 10.14.64.11
const int mqtt_port = 1883;                     //1991
const char* mqtt_user = "thaco";
const char* mqtt_pass = "thaco1234";
const char *mqtt_topic_1 = "shuttle/information";
const char *mqtt_topic_2 = "shuttle/run";
const char *mqtt_topic_3 = "shuttle/report";
const char *mqtt_topic_4 = "shuttle/handle";

WiFiClient espClient;
PubSubClient client(espClient);

const char* jsonRun   = "{\"status\":1}";
const char* jsonCmd1  = "{\"totalStep\":4,\"step1\":\"X0000Y0017>3:16\",\"step2\":\"X0000Y0016>4:10\",\"step3\":\"X0000Y0011>4:14\",\"step4\":\"X0000Y0010>4:15\"}";
const char* jsonCmd2  = "{\"totalStep\":4,\"step1\":\"X0000Y0010>2:10\",\"step2\":\"X0000Y0015>2:14\",\"step3\":\"X0000Y0016>1:16\",\"step4\":\"X0000Y0017>1:15\"}";

struct ShuttleData {
  int shuttleMode;
  int shuttleStatus;
  String qrCode;
  } shuttle;

enum State {
  IDLE,                           //check thong tin gui ve tu topic shuttle/information
  SEND_RUN1, WAIT_RUN1_RESP,      // gui lenh chay run  1 vao shuttle/run roi check thong tin phan hoi
  SEND_CMD1, WAIT_CMD1_RESP,      //gui lenh chay vao shuttle/handle roi check thong tin phan hoi
  WAIT_FOR_QR2,                   
  SEND_RUN2, WAIT_RUN2_RESP,
  SEND_CMD2, WAIT_CMD2_RESP
};

static State currentState = IDLE;
static int retryCount = 0;
static String lastSent;
static unsigned long retryTime = 0;
static String ReceiveString;

int newmsg = 0;

void parseJsonData(const char *jsonString)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, jsonString);
    if (error)
    {
        Serial.print(F("Lỗi parse JSON: "));
        Serial.println(error.f_str());
        return;
    }

    shuttle.shuttleMode = doc["shuttleMode"];
    shuttle.shuttleStatus = doc["shuttleStatus"];
    shuttle.qrCode = doc["qrCode"].as<String>();
    }

void sendJson(const char* topic, const char* json) {
    lastSent = json;
    client.publish(topic, json);
    Serial.print("Gửi JSON tới: ");
    Serial.print(topic);
    Serial.print(": ");
    Serial.println(json);
}

void callback(char* topic, byte* payload, unsigned int length) {
    char jsonString[2048];
    memcpy(jsonString, payload, length);
    jsonString[length] = '\0';
    if (String(topic) == mqtt_topic_1)
    {
        Serial.println("MQTT nhận từ topic: shuttle/infor");
        Serial.println(jsonString);
        parseJsonData(jsonString);          
    }
    else if(String(topic) == mqtt_topic_3)
      {
        ReceiveString = String(jsonString);
        newmsg = 1;
        retryTime = millis();
       }
 }

void initWiFi()
{
    WiFi.begin(ssid, password);
    Serial.print("Đang kết nối WiFi");
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi đã kết nối, IP: " + WiFi.localIP().toString());
}

void reconnectMQTT()
{
    while (!client.connected())
    {
        Serial.print("Kết nối lại MQTT...");
        String clientId = "ESP32Client-" + String(random(0xffff), HEX);
        if (client.connect(clientId.c_str(), mqtt_user, mqtt_pass))
        {
            Serial.println("Thành công!");
            client.subscribe(mqtt_topic_1);
            client.subscribe(mqtt_topic_3);
        }
        else
        {
            Serial.print("Thất bại, mã lỗi: ");
            Serial.println(client.state());
            delay(2000);
        }
    }
}

void initMQTT()
{
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);
    reconnectMQTT();
}

void mqttLoop() {
    if (!client.connected()) {
        initMQTT();
    }
    client.loop();
}

void loopFSM() {
  if (newmsg) {
        newmsg = 0;
        Serial.println(ReceiveString);
        if (ReceiveString == lastSent) {
            Serial.println("JSON phan hoi khop");
            retryCount = 0; 
             retryTime = 0;           
            switch (currentState) {
                case WAIT_RUN1_RESP: currentState = SEND_CMD1; break;
                case WAIT_CMD1_RESP: currentState = WAIT_FOR_QR2; break;
                case WAIT_RUN2_RESP: currentState = SEND_CMD2; break;
                case WAIT_CMD2_RESP: currentState = IDLE; break;
                default: break;
            }
        } else {
            if ((++retryCount <= 5) && ((millis() - retryTime) <= 10000)) {
                Serial.println(" JSON phan hoi sai, gui lai...");
                sendJson((currentState == WAIT_RUN1_RESP || currentState == WAIT_RUN2_RESP) ? mqtt_topic_2 : mqtt_topic_4, lastSent.c_str());
                } else {
                Serial.println("Quá 5 lần, dừng chương trình.");
                while (true);
            }
        }
    }

  switch (currentState) {
     case IDLE:   
      if (shuttle.shuttleMode == 0 && shuttle.shuttleStatus == 8 && ((shuttle.qrCode == "X0000Y0017") || (shuttle.qrCode =="X0000Y0010"))) {
        sendJson(mqtt_topic_2,jsonRun);
        currentState = WAIT_RUN1_RESP;       
      }
      break;

    case SEND_CMD1:
      sendJson(mqtt_topic_4,jsonCmd1);
      currentState = WAIT_CMD1_RESP;
      break;

    case WAIT_FOR_QR2:
       // Serial.println("check dieu kien");
      //if (shuttle.shuttleMode == 1 && shuttle.shuttleStatus == 1 && ((shuttle.qrCode == "X0000Y0017") || (shuttle.qrCode =="X0000Y0010"))) {
        if (shuttle.shuttleMode == 0 && shuttle.shuttleStatus == 8 &&  (shuttle.qrCode =="X0000Y0010")) {
        sendJson(mqtt_topic_2,jsonRun);
        currentState = WAIT_RUN2_RESP;
      }
      break;

    case SEND_CMD2:
      sendJson(mqtt_topic_4,jsonCmd2);
      currentState = WAIT_CMD2_RESP;
      break;
     default: break;
  }
}
