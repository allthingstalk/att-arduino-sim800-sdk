/*
   Copyright 2014-2016 AllThingsTalk

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
*/

#include <SoftwareSerial.h>
#include "Adafruit_FONA.h"
#include "ATT_MQTT.h"
#include "ATT_MQTT_SIM800.h"

#include "ATT_IOT_GPRS.h"
#include <SPI.h>                //required to have support for signed/unsigned long type.

/*
  AllThingsTalk Makers Arduino Example 

  ### Instructions

  1. Setup the Arduino hardware
    - USB2Serial
    - Grove kit shield
    - Potentiometer to A0
    - Led light to D8
  2. Add 'allthingstalk_arduino_standard_lib' library to your Arduino Environment. [Try this guide](http://arduino.cc/en/Guide/Libraries)
  3. Fill in the missing strings (deviceId, clientId, clientKey, mac) and optionally change/add the sensor & actuator names, ids, descriptions, types
     For extra actuators, make certain to extend the callback code at the end of the sketch.
  4. Upload the sketch

  ### Troubleshooting

  1. 'Device' type is reported to be missing. 
  - Make sure to properly add the arduino/libraries/allthingstalk_arduino_standard_lib/ library
  2. No data is showing up in the cloudapp
  - Make certain that the data type you used to create the asset is the expected data type. Ex, when you define the asset as 'int', don't send strings or boolean values.
*/

#define deviceId "rswdURafZc1qPCmFmGwNnKl6"
#define clientId "testjan"
#define clientKey "5i4duakv2bq"

#define FONA_APN       "internet.be"
#define FONA_USERNAME  ""
#define FONA_PASSWORD  ""

#define FONA_RX 2
#define FONA_TX 3
#define FONA_RST 5
SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);



ATTDevice Device(deviceId, clientId, clientKey);            //create the object that provides the connection to the cloud to manager the device.
#define httpServer "api.AllThingsTalk.io"                  // HTTP API Server host                  
#define mqttServer httpServer                				// MQTT Server Address 

int knobPin = 1;                                            // Analog 0 is the input pin + identifies the asset on the cloud platform
int ledPin = 2;                                             // Pin 8 is the LED output pin + identifies the asset on the cloud platform

//required for the device
void callback(char* topic, byte* payload, unsigned int length);
// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_FONA fona = Adafruit_FONA(FONA_RST);
ATT_MQTT_SIM800 mqtt(&fona, mqttServer, 1883);

void setup()
{
	mqtt.setCallback(callback);
	pinMode(ledPin, OUTPUT);                                  // initialize the digital pin as an output.         
	fonaSS.begin(19200); // if you're using software serial
	Serial.begin(57600);                                       // init serial link for debugging
  
	while (!Serial){}											//block if the serial connection is not open ==>> ONLY FOR DEBUGGING
  
  
	while (! Device.InitGPRS(&fona, FONA_APN, FONA_USERNAME, FONA_PASSWORD)) {
		Serial.println("Retrying FONA");
	}
	Serial.println(F("Connected to Cellular!"));
	delay(2000);  										// wait a few seconds to stabilize connection
  
	while(!Device.Connect(httpServer))            // connect the device with the IOT platform.
		Serial.println("retrying");
	Device.AddAsset(knobPin, "knob", "rotary switch",false, "{\"type\": \"integer\", \"minimum\": 0, \"maximum\": 1023}");
	Device.AddAsset(ledPin, "led", "light emitting diode", true, "boolean");
	while(!Device.Subscribe(mqtt))                          // make certain that we can receive message from the iot platform (activate mqtt)
		Serial.println("retrying"); 
}

unsigned long time;                                         //only send every x amount of time.
unsigned int prevVal =0;
void loop()
{
  unsigned long curTime = millis();
  if (curTime > (time + 3000))                              // publish light reading every 5 seconds to sensor 1
  {
    unsigned int lightRead = analogRead(knobPin);           // read from light sensor (photocell)
    if(prevVal != lightRead){
      Device.Send(String(lightRead), knobPin);
      prevVal = lightRead;
    }
    time = curTime;
  }
  Device.Process(); 
}


// Callback function: handles messages that were sent from the iot platform to this device.
void callback(char* topic, byte* payload, unsigned int length) 
{ 
  String msgString; 
  {                                                     //put this in a sub block, so any unused memory can be freed as soon as possible, required to save mem while sending data
    char message_buff[length + 1];                      //need to copy over the payload so that we can add a /0 terminator, this can then be wrapped inside a string for easy manipulation.
    strncpy(message_buff, (char*)payload, length);      //copy over the data
    message_buff[length] = '\0';                        //make certain that it ends with a null         
    msgString = String(message_buff);
  }
  int* idOut = NULL;
  {                                                     //put this in a sub block, so any unused memory can be freed as soon as possible, required to save mem while sending data
    int pinNr = Device.GetPinNr(topic, strlen(topic));
    
    Serial.print("Payload: ");                          //show some debugging.
    Serial.println(msgString);
    Serial.print("topic: ");
    Serial.println(topic);
    
    if (pinNr == ledPin)       
    {
		msgString.toLowerCase();                            //to make certain that our comparison later on works ok (it could be that a 'True' or 'False' was sent)
		if (msgString == "false") {
			digitalWrite(ledPin, LOW);                      //change the led    
			idOut = &ledPin;                                
		}
		else if (msgString == "true") {
			digitalWrite(ledPin, HIGH);
			idOut = &ledPin;
		}
    }
  }
  if(idOut != NULL)                                     //also let the iot platform know that the operation was succesful: give it some feedback. This also allows the iot to update the GUI's correctly & run scenarios.
    Device.Send(msgString, *idOut);    
}
