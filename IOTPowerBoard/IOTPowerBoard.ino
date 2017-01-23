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

#include "appconfig.h"
#include <EventManager.h>
#include <myPushButton.h>
#include <myWifiHelper.h>

/* ----------------------------------------------------------- */

char versionText[] = "IOT Power Board v1.4.1";

#define HOSTNAME "IOT-Power-Board"

MyWifiHelper wifiHelper(HOSTNAME);

#define     FEED_IOTPOWERBOARD  "dev/iot-power-board"

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

/* ----------------------------------------------------------- */

void listener_Button(int eventCode, int eventParams);

/* ----------------------------------------------------------- */

myPushButton button(EXT_BUTTON, true, 2000, 1, listener_Button);

int val = 0;
int extSwVal = 1;
bool hasBeenReleased = true;

/* ----------------------------------------------------------- */

void listener_Button(int eventCode, int eventParams);
void listener_TimeOut(int event, int state);
void logMessage(char* message);

/* ------------------------------------------------------------- */

void setup() {

    Serial.begin(9600);
    delay(200);
    Serial.println("Booting");
    Serial.println(versionText);

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_OFF);

    pinMode(RELAY, OUTPUT);
    digitalWrite(RELAY, 1);     // turn ON by default (incase it doens't work good)

    ch[CH_RELAY].state = 1;     // off
    setRelay(ch[CH_RELAY].state);

    button.serviceEvents();     // make sure we can turn the power board off during [wifi] startup

    wifiHelper.setupWifi();

    button.serviceEvents();

    wifiHelper.setupOTA(HOSTNAME);

    button.serviceEvents();

    wifiHelper.setupMqtt();

}

/* ----------------------------------------------------------- */

void loop() {

    wifiHelper.loopMqtt();

    ArduinoOTA.handle();
    
    button.serviceEvents();
    serviceEvents(CH_TIMEOUT);

    delay(20);
}

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

    wifiHelper.mqttPublish(FEED_IOTPOWERBOARD, message);
}

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
