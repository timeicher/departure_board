//#include <WiFi.h>
#include <TinyXML.h>
#include <WiFiS3.h>
#include <TM1637Display.h>

#include "arduino_secrets.h"

char ssid[] = SECRET_SSID;      // your network SSID (name)
char pass[] = SECRET_PASS;      // your network password (use for WPA, or use as key for WEP)
const char* apiKey = API_KEY;   // your personal API Key

char server[] = "api.opentransportdata.swiss";
char page[] = "/trias2020";

int status = WL_IDLE_STATUS;
int port = 443;

WiFiSSLClient client;

// Prepare XML data
const char* xmlData = XML_REQ;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    ; // wait for serial port to connect. Needed for native USB port only
  }

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to WiFi");
  printWifiStatus();

  // setup http connection
  sendPostRequest();
}

void sendPostRequest() {
  Serial.println("making POST request...");


  if (client.connect(server,port)) {
    Serial.println("connected to server");
    client.println("POST " + String(page) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/xml");
    client.println("Accept: application/xml");
    client.println("Authorization: Bearer " + String(apiKey));
    client.print("Content-Length: ");
    client.println(strlen(xmlData));
    client.println();
    client.println(xmlData);

    // Read the response from the server
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }
    Serial.println("\nDisconnecting from server.");
    client.stop();
  } else {
    Serial.println("Connection failed.");
  }

  Serial.println("Wait five seconds");
  delay(5000);
}

void loop() {
  // Keep the program alive
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}