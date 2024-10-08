#include <Arduino.h>
#include "myota.h"
#include <WiFi.h>
//#include <fauxmoESP.h>
#include "fauxmoESP.h"
#include "ESPTelnet.h"
#include <Preferences.h>

Preferences preferences;

#define DEVNAME "lici"
#define PWM_NBIT 12
#define PWM_FREQ 2000
#define MAXVALUE 3800
const char* ssid = "";
const char* password = "";

bool last_state;
bool switch_power=false;
fauxmoESP fauxmo;
const int ledPin = 16; 

float minratio = .2;
int maxIntensity=MAXVALUE;
float minfreq=.2;
float maxfreq=5;
int stp=-1;
bool deviceState;

// HSV-RGB stuff 
uint8_t redLed;
uint8_t greenLed;
uint8_t blueLed;

unsigned char value; unsigned int hue; unsigned int saturation;

ESPTelnet telnet;
IPAddress ip;
uint16_t  port = 23;


/*--------------------------------------------*/
#include <map>
#include <iostream>


enum Colors { Red,Purple,Green,Crimson, Blue,Salmon, DefaultColor,Orange,Gold,Yellow };
enum Moods { clear, gentlefire, roaringfire, smoothcandle,smoothercandle,crazysmall};

struct sett { float min; float minfreq; float maxfreq; };

std::map<int, Colors> colorMap = {
    {0, Red}, {50426, Purple}, {21845, Green},
    {63351, Crimson}, {43690, Blue}, {3095, Salmon},
    {7100, Orange},{9102,Gold},{10923,Yellow},
};

/*sett={min,minfreq,maxfreq
 * min=min as percentage of maximum
 * minfreq=minimum frequency
 * maxfreq=maximum frequency
 */
std::map<Moods, sett> fireSettings = {
    {clear, {1,1,1}},
    {gentlefire,{.5,1,3}},
    {roaringfire,{.1,2,5}},
    {smoothcandle,{.5,1,1}},
    {smoothercandle,{.8,.8,1}},
    {crazysmall,{.8,2,4}},
};

std::map<Colors,Moods> colormood ={
    {DefaultColor,clear},
    {Red,clear},
    {Crimson,gentlefire},
    {Salmon,roaringfire},
    {Orange,smoothcandle},
    {Yellow,smoothercandle},
    {Gold,crazysmall},
};


void setmood(Moods md){
    auto it3 = fireSettings.find(md);
    sett set = (it3 != fireSettings.end()) ? it3->second : fireSettings[clear];

    minratio=set.min;
    minfreq=set.minfreq;
    maxfreq=set.maxfreq;
}
 

/***************************************** */


bool isHueInRange(unsigned int hue, unsigned int targetHue, unsigned int tolerance = 182) {
    unsigned int lowerBound = (targetHue >= tolerance) ? targetHue - tolerance : 65536 - (tolerance - targetHue);
    unsigned int upperBound = (targetHue + tolerance) % 65536;
    
    if (lowerBound < upperBound) {
        return (hue >= lowerBound && hue <= upperBound);
    } else {
        return (hue >= lowerBound || hue <= upperBound);
    }
}

Colors getColorFromHue(unsigned int hue) {
    if (isHueInRange(hue, 0)) return Red;
    if (isHueInRange(hue, 50426)) return Purple;
    if (isHueInRange(hue, 21845)) return Green;
    if (isHueInRange(hue, 63351)) return Crimson;
    if (isHueInRange(hue, 43690)) return Blue;
    if (isHueInRange(hue, 3095)) return Salmon;
    if (isHueInRange(hue, 7100)) return Orange;
    if (isHueInRange(hue, 9102)) return Gold;
    if (isHueInRange(hue, 10923)) return Yellow;
    return DefaultColor;
}

/*------------------------------------------*/

bool StartsWith(String a, String b)
{
   if(strncmp(a.c_str(), b.c_str(), strlen(b.c_str())) == 0) return 1;
   return 0;
}

void errorMsg(String error, bool restart = true) {
  Serial.println(error);
  if (restart) {
    Serial.println("Rebooting now...");
    delay(2000);
    ESP.restart();
    delay(2000);
  }
}


// (optional) callback functions for telnet events
void onTelnetConnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" connected");
  
  telnet.println("\nWelcome " + telnet.getIP());
  telnet.println("(Use ^] + q  to disconnect.)");
}

void onTelnetDisconnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" disconnected");
}

void onTelnetReconnect(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip) {
  Serial.print("- Telnet: ");
  Serial.print(ip);
  Serial.println(" tried to connected");
}

void onTelnetInput(String str) {
  // checks for a certain command
  if (str == "ping") {
        telnet.println("> pong");
        Serial.println("- Telnet: pong");
  // disconnect the client
  } else if (str == "bye") {
        telnet.println("> disconnecting you...");
        telnet.disconnectClient();
   }else if (StartsWith(str,"min ")){float i;sscanf(str.c_str(),"%*s %f",&i);
        minratio=i>1?1:(i<0?0:i); 
        stp=-1;
   }else if (StartsWith(str,"max ")){int i;sscanf(str.c_str(),"%*s %d",&i);
        maxIntensity=i>100?100:i; 
        maxIntensity=float(maxIntensity)*MAXVALUE/100;
		telnet.printf("set maxIntensity to %d\n",maxIntensity);
        stp=-1;
   }else if (StartsWith(str,"minfreq ")){float i;sscanf(str.c_str(),"%*s %f",&i);
        minfreq=i>20?20:(i<.1?.1:i); 
        stp=-1;
   }else if (StartsWith(str,"maxfreq ")){float i;sscanf(str.c_str(),"%*s %f",&i);
        maxfreq=i>20?20:(i<.1?.1:i); 
        stp=-1;
   }else if (str=="help" || str =="?"){
      telnet.println(   "****** HELP ******\n\n"
            "help or ? - this help message\n"
            "ping - responds with pong\n"
            "bye - disconects telnet\n"
            "min <x> - set the minimum value for the light as percentage of max (range 0-1) \n"
            "max <x> - set the maximum value for the light 0-100\n"
            "minfreq <x> - set the minimum frequency of flickering  (.1-20) \n"
            "maxfreq <x> - set the maximum frequency of flickering  (.1-20) \n"
          );     
   }


}



void setupTelnet() {  
  // passing on functions for various telnet events
  telnet.onConnect(onTelnetConnect);
  telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  telnet.onReconnect(onTelnetReconnect);
  telnet.onDisconnect(onTelnetDisconnect);
  telnet.onInputReceived(onTelnetInput);

  Serial.print("- Telnet: ");
  if (telnet.begin(port)) {
    Serial.println("running");
  } else {
    Serial.println("error.");
    errorMsg("Will reboot...");
  }
}

/* -------------------------------------*/

void load_settings(){
    minratio=preferences.getFloat("minratio",.2);
    maxIntensity=preferences.getFloat("maxIntensity",MAXVALUE);
    minfreq=preferences.getFloat("minfreq",.2);
    maxfreq=preferences.getFloat("maxfreq",5);
    deviceState=preferences.getBool("deviceState",true);
}

void save_settings(){
    telnet.println("\nsaving settings\n");
    preferences.putFloat("minratio",minratio);
    preferences.putFloat("maxIntensity",maxIntensity);
    preferences.putFloat("minfreq",minfreq);
    preferences.putFloat("maxfreq",maxfreq);
    preferences.putBool("deviceState",deviceState);
}

/*------------------------------------------*/


void setup() {

  pinMode(ledPin, OUTPUT);  // Initialize the LED pin as an output
  ledcSetup(1, PWM_FREQ, PWM_NBIT);  // channel 0, 2000 Hz, NBIT resolution
  ledcAttachPin(ledPin, 1);  // Attach the LED to channel 0
  ledcWrite(1,0);

  Serial.begin(115200);

  preferences.begin(DEVNAME, false);
  load_settings();

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Print IP Address
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  otasetup(DEVNAME);

    // Fauxmo setup
    fauxmo.addDevice(DEVNAME);
    fauxmo.setPort(80);
    fauxmo.enableMDNS(DEVNAME);
    fauxmo.enable(true);

fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char _value, unsigned int _hue, unsigned int _saturation, unsigned int ct) {
        hue = _hue;
        saturation = _saturation;
        value = _value;
        deviceState = state;

        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d hue: %u saturation: %u ct: %u\n", device_id, device_name, state ? "ON" : "OFF", value, hue, saturation, ct);
        telnet.printf("[MAIN] Device #%d (%s) state: %s value: %d hue: %u saturation: %u ct: %u\n", device_id, device_name, state ? "ON" : "OFF", value, hue, saturation, ct);

        char colormode[3];
        fauxmo.getColormode(device_id, colormode, 3);
        Serial.printf("Colormode: %s\n", colormode);
        
        redLed = fauxmo.getRed(device_id);
        greenLed = fauxmo.getGreen(device_id);
        blueLed = fauxmo.getBlue(device_id);

        // Use the new color matching function
        Colors clr = getColorFromHue(hue);

        auto it2 = colormood.find(clr);
        Moods md = (it2 != colormood.end()) ? it2->second : clear;
        setmood(md);

        maxIntensity = value * MAXVALUE / 255;
        save_settings();
        stp = -1;
        
        Serial.printf("HSV: %d %d %d  RGB: %d %d %d\n", hue, saturation, value, redLed, greenLed, blueLed);
    });


/*
    fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
        Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);


        if (strcmp(device_name, DEVNAME) == 0) {
            int dutyCycle = map(value, 0, 255, 0, 150);  // Map the faux (0-255) to the value we write (0-150)
            ledcWrite(0, state ? dutyCycle : 0);  // Write the duty cycle to channel 0 if the state is ON, otherwise turn off
        }
    });
    */

 setupTelnet();
  // setmood(clear);

  load_settings();
}


bool shouldAttemptReconnect = false;
unsigned long lastAttemptTime = 0;
unsigned long firstAttemptTime = 0;  

void check_reboot_wifi(){
 if (WiFi.status() != WL_CONNECTED) {
    if (!shouldAttemptReconnect) {
      Serial.println("Lost connection. Will try to reconnect...");
      shouldAttemptReconnect = true;
      lastAttemptTime = millis();
    }

    if (shouldAttemptReconnect && millis() - lastAttemptTime > 5000) {  // Attempt every 5 seconds
      Serial.println("Attempting to reconnect...");
      WiFi.begin(ssid, password);
      lastAttemptTime = millis();
      
      if (WiFi.status() == WL_CONNECTED) {
        Serial.println("Reconnected!");
        shouldAttemptReconnect = false;
      }
    }
  }
  
  // If reconnection failed for too long, restart
  if (shouldAttemptReconnect && millis() - firstAttemptTime > 30000) {  // 30 seconds timeout
    Serial.println("Reconnect failed. Rebooting...");
    ESP.restart();
  }

}


#define LTIME 10
float trg;
float rate;

void loop() {

    check_reboot_wifi();
    if (stp<0){
        int ntrg=random(minratio* maxIntensity, maxIntensity);
        if (trg>maxIntensity || trg <minratio*maxIntensity){
            //the intensity changed 
            stp=200/LTIME;
            rate=(ntrg-trg)/stp;
        }else{
            int duration=random(1000/maxfreq,1000/minfreq);
            //a full signal would take that time to complete a full cycle
            //in the range of minIntensity,maxIntensity
            // so the rate of change would be per our LTIME cycle
            rate=float(maxIntensity-minratio*maxIntensity)*LTIME/duration;
            if(ntrg<trg){rate=-rate;}
            stp=(ntrg-trg)/rate;
            //Serial.printf("rate %f steps %d\n",rate,stp);
        }
    }else{
        stp-=1;
        trg+=rate;
    }

    if(deviceState){
        ledcWrite(1, trg);
        delay(LTIME);
    }else{
        ledcWrite(1, 0);
    }

    ArduinoOTA.handle();
    fauxmo.handle();
    telnet.loop();
}
