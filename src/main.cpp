#include <Arduino.h>
#include "handle.h"

void setup() {
    Serial.begin(115200);
    initWiFi();
    initMQTT();
}

void loop() {
    mqttLoop();
    loopFSM();
}