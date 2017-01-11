/**
 * \file Main.cpp
 * \mainpage Ballmer Peak Detector
 *
 */
#include <Esp.h>
#include <stdio.h>

#include <PubSubClient.h>					// Required for MQTT support

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>		// Enable update over HTTP
#include "../include/ESP8266mDNS.h" 		// Bug https://github.com/esp8266/Arduino/issues/2211

#include <EEPROM.h> 						// Store some data in the EEPROM

#include <SFE_MicroOLED.h>  				// Support for the attached OLED screen
#include <Wire.h>  							// Required for I2C and the screen

const char* host = "bpd"; 					//!< HTTP server host name
const int port = 80;						//!< HTTP port number

const char* ssids[] = {  };
const char* passwords[] = {  };
//const char* ssids[] = { "" };				//!< List of WLAN SSIDs
//const char* passwords[] = { "" };			//!< List of WLAN passwords

#define PIN_RESET 255  						//!< Connect RST to pin 9
#define DC_JUMPER 0

ESP8266WebServer httpServer(port);
ESP8266HTTPUpdateServer httpUpdater;

MicroOLED oled(PIN_RESET, DC_JUMPER);

const char* mqttServer = "m20.cloudmqtt.com";	//!< MQTT server address
const char* mqttUser = "henqvddw";					//!< MQTT client user identifier
const char* mqttPassword = "QgzLStw8NTOy";				//!< MQTT client password
const long mqttPort = 15555;					//!< MQTT server port
#define MQTTTOPIC "ballmerpeakdetector/bac" //!< MQTT topic for BAC measurement

// MQTT Status
#define MQTT_STATUS_UNKNOWN 0				//!< MQTT undefined status
#define MQTT_STATUS_SENDING 1				//!< MQTT sending status
#define MQTT_STATUS_ACKNOWLEDGED 2
#define MQTT_STATUS_FAILED 3
unsigned int mqttStatus = MQTT_STATUS_UNKNOWN;
const char* mqttTopicInAck = MQTTTOPIC "/ack";
const char* mqttTopicOutConnecting = MQTTTOPIC "/connecting";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

char buffer[10];					//!< Number conversion character buffer
/**
 * Center and print a small title
 *
 * \param title	the title string
 * \param font 	font size (0-2)
 */
void printTitle(String title, int font) {
	int middleX = oled.getLCDWidth() / 2;
	int middleY = oled.getLCDHeight() / 2;

	oled.clear(PAGE);
	oled.setFontType(font);
	// try to set the cursor in the middle of the screen
	oled.setCursor(middleX - (oled.getFontWidth() * (title.length() / 2)),
			middleY - (oled.getFontWidth() / 2));
	// print the title:
	oled.print(title);
	oled.display();
	delay(1500);
	oled.clear(PAGE);
}

/**
 * Prints the BAC indicator at the center of the screen
 *
 * \param bac	level from 0-1023
 * \param font	the font to use (0-2)
 */
void printBAC(int bac, int font) {
	// find the middle of the screen
	int middleX = oled.getLCDWidth() / 2;
	int middleY = oled.getLCDHeight() / 2;

	String string = "";
	string += itoa(bac, buffer, 10);
	oled.clear(PAGE);
	oled.setFontType(font);
	// try to set the cursor in the middle of the screen
	oled.setCursor(middleX - (oled.getFontWidth() * (string.length() / 2)),
			middleY - (oled.getFontWidth() / 2));
	// print the title:
	oled.print(string);
	oled.display();
}

void printText(String text) {
	oled.setFontType(0);
	oled.print(text);
	oled.display();
}

void printText(char* text) {
	oled.setFontType(0);
	oled.print(text);
	oled.display();
}

void handleRoot() {
	httpServer.send(200, "text/plain",
			"Hello from esp8266! Go to <a href='/update'>/update</a> to upload new firmware.");
}

void reconnectMqtt() {
	/*
	 * This code is copied from an example.
	 * Not sure it is the optimal usage of the client id.
	 */

	// Loop until we're reconnected
	while (!mqttClient.connected()) {
		printText("Attempting MQTT connection...\n");
		Serial.print("Attempting MQTT connection...");
		// Create a random client ID
		String clientId = "bpd-";
		clientId += String(random(0xffff), HEX);
		// Attempt to connect
		if (mqttClient.connect(clientId.c_str(), mqttUser, mqttPassword)) {
//		if (mqttClient.connect(clientId.c_str())) {
			Serial.println("connected");
			// Once connected, publish an announcement...
			if (mqttStatus == MQTT_STATUS_UNKNOWN) {
				mqttClient.publish(mqttTopicOutConnecting, "BPD Starting");
				Serial.println("Connecting");
			} else {
				mqttClient.publish(mqttTopicOutConnecting, "BPD Reconnecting");
				Serial.println("Reconnecting");
			}

			// ... and resubscribe
			if (mqttClient.subscribe(mqttTopicInAck, 1)) {
				Serial.println("Subscribe ok");
			} else {
				Serial.println("Subscribe failed");
			}
		} else {
			Serial.println("Reattempting connection again in 2 seconds");
			printText("Failed connecting to MQTT");
			delay(2000);
		}
	}
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
	/*
	 * Callback for subscribed messages
	 */
	Serial.print("Message arrived [");
	Serial.print(topic);
	Serial.print("] ");
	for (int i = 0; i < length; i++) {
		Serial.print((char) payload[i]);
	}

	// Set status to turn on green led if ack is received
	if (strcmp(topic, mqttTopicInAck) == 0) {
		Serial.println("MQTT_STATUS_ACKNOWLEDGED Received");
		mqttStatus = MQTT_STATUS_ACKNOWLEDGED;
	}

}

void writeMqtt(int bac) {
	mqttStatus = MQTT_STATUS_SENDING;
	reconnectMqtt();
	Serial.println("Sending MQTT data");
	mqttClient.publish(MQTTTOPIC, String(bac).c_str());
}

void setup() {

	Serial.begin(9600);
	// handle the root of the web server
	//httpServer.on("/", HTTP_GET, handleRoot);

	oled.begin();    	// initialize the OLED
	oled.clear(ALL); 	// clear the display's internal memory
	oled.clear(PAGE); 	// clear the buffer.

	// read the current AP index from flash memory
	EEPROM.begin(64);
	int i = (int) EEPROM.read(0);
	if (i > (sizeof(ssids) / sizeof(*ssids)))
		i = 0;

	// connect to the AP
	printText("Connecting\n");
	printText(ssids[i]);
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssids[i], passwords[i]);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		printTitle("Failed!", 0);
		i++; // try the next access point
		EEPROM.write(0, i);
		EEPROM.commit();
		delay(2000);
		ESP.restart();
	}
	MDNS.begin(host);

	httpUpdater.setup(&httpServer);
	httpServer.begin();

	MDNS.addService("http", "tcp", port);

	// MQTT
	mqttClient.setServer(mqttServer, mqttPort);
	mqttClient.setCallback(mqttCallback);

	// prepare to read the analog input
	pinMode(A0, INPUT);
	// and the button
	pinMode(D7, INPUT_PULLUP);

}

int current;
unsigned int out;
unsigned int high = 0;
void loop() {
	printTitle("Press\nbutton", 0);
	current = digitalRead(D7);
	while (digitalRead(D7) == HIGH) {
		// keep it warm
		out = (unsigned int) analogRead(A0);
		mqttClient.loop();
		yield();
	}
	for (int i = 0; i < 50; i++) {
		out = (unsigned int) analogRead(A0);
		if (out > high){
			high = out;
		}
		printBAC(out, 2);
		delay(20);
		yield();
	}
	writeMqtt(high);
	printTitle("OK", 1);
	delay(1000);
	printBAC(high, 2);
	delay(5000);
	high = 0;
}

