/**
 * \file Main.cpp
 * \mainpage Ballmer Peak Detector
 *
 */
#include <Esp.h>

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include "../include/ESP8266mDNS.h" // bug https://github.com/esp8266/Arduino/issues/2211

#include <EEPROM.h> 				// Store some data in the EEPROM

#include <SFE_MicroOLED.h>  		// Support for the attached OLED screen
#include <Wire.h>  					// Required for I2C and the screen

const char* host = "bpd"; 			//!< HTTP server host name
const int port = 80;				//!< HTTP port number

const char* ssids[] = { "guest" };
const char* passwords[] = { "password" };

#define PIN_RESET 255  				//!< Connect RST to pin 9
#define DC_JUMPER 0

ESP8266WebServer httpServer(port);
ESP8266HTTPUpdateServer httpUpdater;

MicroOLED oled(PIN_RESET, DC_JUMPER);

char buffer[10];
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
	string += itoa(bac,buffer,10);
	oled.clear(PAGE);
	oled.setFontType(0);
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

void handleRoot(){
	httpServer.send(200, "text/plain", "Hello from esp8266! Go to <a href='/update'>/update</a> to upload new firmware.");
}

void setup() {

	// handle the root of the web server
	httpServer.on("/", HTTP_GET, handleRoot);

	oled.begin();    	// initialize the OLED
	oled.clear(ALL); 	// clear the display's internal memory
	oled.clear(PAGE); 	// clear the buffer.

	// read the current AP index from flash memory
	EEPROM.begin(64);
	int i = (int)EEPROM.read(0);
	if (i > (sizeof(ssids)/sizeof(*ssids))) i=0;

	// connect to the AP
	printText("Connecting\n");
	printText(ssids[i]);
	WiFi.mode(WIFI_AP_STA);
	WiFi.begin(ssids[i], passwords[i]);
	while (WiFi.waitForConnectResult() != WL_CONNECTED) {
		printTitle("Failed!",0);
		i++; // try the next access point
		EEPROM.write(0,i);
		EEPROM.commit();
		delay(2000);
		ESP.restart();
	}
	MDNS.begin(host);

	httpUpdater.setup(&httpServer);
	httpServer.begin();

	MDNS.addService("http","tcp",port);

	// prepare to read the analog input
	pinMode(A0,INPUT);
}

int out;
void loop() {
	IPAddress a = WiFi.localIP();
	for (int i=0;i<2000;i++){
		httpServer.handleClient();
		delay(1);
	}
	out = (unsigned int) analogRead(A0);
	printBAC(out,1);

}

