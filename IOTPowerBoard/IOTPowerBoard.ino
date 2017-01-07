/*
   1MB flash sizee
   sonoff header
   1 - vcc 3v3
   2 - rx
   3 - tx
   4 - gnd
   5 - gpio 14
   esp8266 connections
   gpio  0 - button
   gpio 12 - relay
   gpio 13 - green led - active low
   gpio 14 - pin 5 on header
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include "appconfig.h"
#include "wificonfig.h"
#include <EventManager.h>
#include <myPushButton.h>
// MQTT
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"

/* ----------------------------------------------------------- */

char versionText[] = "IOT Power Board v1.3.0";

/* ----------------------------------------------------------- */

#define SONOFF_BUTTON    0
#define SONOFF_RELAY    12
#define SONOFF_LED      13
#define SONOFF_INPUT    14

#define BUTTON          0
#define EXT_BUTTON      14
#define RELAY           12
#define LED_PIN         13
#define LED_ON          LOW
#define LED_OFF         HIGH

EventManager sEM;

#define CH_EXT_SWITCH   0
#define CH_BUTTON       1
#define CH_RELAY        2
#define CH_TIMEOUT      3


struct Channel {
    int index;
    int state;
    int eventCode;
};

Channel ch[10] 
{
    {
        CH_RELAY,
        1,
        0
    },
    {
        CH_TIMEOUT,
        0,
        EventManager::kEventUser2
    }
};

#define SECONDS     1000        // ms
#define MINUTES     60*SECONDS
long timedPeriod = 5 * MINUTES;


WiFiClient client;

Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_USERNAME, AIO_KEY);

Adafruit_MQTT_Publish iotPowerBoardLog = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/IOTPowerBoardLog");

/* ----------------------------------------------------------- */

void listener_Button(int eventCode, int eventParams);

/* ----------------------------------------------------------- */

myPushButton button(EXT_BUTTON, true, 2000, 1, listener_Button);

int val = 0;
int extSwVal = 1;
bool hasBeenReleased = true;

/* ----------------------------------------------------------- */

void listener_Button(int eventCode, int eventParams) {

    switch (eventParams) {
        
        case button.EV_BUTTON_PRESSED:
            setRelay(0);
            sEM.removeListener(ch[CH_TIMEOUT].eventCode, listener_TimeOut);
            Serial.println("EV_BUTTON_PRESSED");
            logMessage("EV_BUTTON_PRESSED");
            break;          
        
        case button.EV_HELD_FOR_LONG_ENOUGH:
            sEM.addListener(ch[CH_TIMEOUT].eventCode, listener_TimeOut);
            ch[CH_TIMEOUT].state = millis() + timedPeriod;
            setRelay(1);
            Serial.println("EV_HELD_FOR_LONG_ENOUGH");
            logMessage("EV_HELD_FOR_LONG_ENOUGH");
            break;
        
        case button.EV_RELEASED:
            Serial.println("EV_RELEASED");
            logMessage("EV_RELEASED");

            break;
    }
}

void listener_TimeOut(int event, int state) {

    Serial.println("Timed out");
    setRelay(0);
    sEM.removeListener(ch[CH_TIMEOUT].eventCode, listener_TimeOut);
}

void logMessage(char* message) {

    if (! iotPowerBoardLog.publish(message)) {
        Serial.println(F("Failed"));
    } else {
        Serial.println(F("OK!"));
    }
}


/* ------------------------------------------------------------- */

void setup() {

    Serial.begin(9600);
    delay(200);
    Serial.println("Booting");
    Serial.println(versionText);

    setupWifi();

    setupOTA("IOTPowerBoard");

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);

    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, 0);
    
    Serial.println("Ready");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    ch[CH_RELAY].state = 0;     // off
    setRelay(ch[CH_RELAY].state);
}

/* ----------------------------------------------------------- */

void loop() {

    MQTT_connect();

    ArduinoOTA.handle();
    
    button.serviceEvents();
    serviceEvents(CH_TIMEOUT);

    delay(100);
}

/* ----------------------------------------------------------- */

void serviceEvents(int st) {

    bool relayOn = ch[CH_RELAY].state == 1;

    switch (st) {
        case CH_TIMEOUT: {
            if (sEM.isListenerEnabled(ch[CH_TIMEOUT].eventCode, listener_TimeOut)) {
                if (millis() > ch[CH_TIMEOUT].state) {
                    sEM.queueEvent(ch[CH_TIMEOUT].eventCode, 0);
                }
            }
        }
    }
    sEM.processEvent();
}

void setLED(int val) {
    digitalWrite(LED_PIN, !val);
}

void setRelay(int val) {
    ch[CH_RELAY].state = val;
    digitalWrite(RELAY, val);
    setLED(val);
    Serial.print("Setting relay: "); Serial.println(val);
}

void toggleRelay() {
    if (ch[CH_RELAY].state == 1) {
        ch[CH_RELAY].state = 0;
    }
    else {
        ch[CH_RELAY].state = 1;
    }
    setRelay(ch[CH_RELAY].state);
}

void MQTT_connect() {

    int8_t ret;

    // Stop if already connected.
    if (mqtt.connected()) {
        return;
    }

    Serial.print("Connecting to MQTT... ");

    uint8_t retries = 3;
    while ((ret = mqtt.connect()) != 0) {       // connect will return 0 for connected
        Serial.println(mqtt.connectErrorString(ret));
        Serial.println("Retrying MQTT connection in 5 seconds...");
        mqtt.disconnect();
        delay(5000);  // wait 5 seconds
        retries--;
        if (retries == 0) {
            // basically die and wait for WDT to reset me
            while (1);
        }
    }
    Serial.println("MQTT Connected!");
}

void setupWifi() {

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }
}

void setupOTA(char* host) {
    
    ArduinoOTA.setHostname(host);
    ArduinoOTA.onStart([]() {
        Serial.println("OTA Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nOTA End");
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

    ArduinoOTA.begin();
}
