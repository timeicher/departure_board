#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINT_LN(x) Serial.println(x)


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

// times to display; if -1 no valid time; format is hhmmss - hours, minutes, seconds
int planned_1 = -1;
int actual_1 = -1;
int planned_2 = -1;
int actual_2 = -1;

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
    DEBUG_PRINT_LN("Communication with WiFi module failed!");
    // don't continue
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    DEBUG_PRINT_LN("Please upgrade the firmware");
  }

  // attempt to connect to WiFi network:
  while (status != WL_CONNECTED) {
    DEBUG_PRINT("Attempting to connect to SSID: ");
    DEBUG_PRINT_LN(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // wait 10 seconds for connection:
    delay(10000);
  }
  DEBUG_PRINT_LN("Connected to WiFi");
  printWifiStatus();

  // setup http connection
  sendPostRequest(xmlData);
}

// finds time coming after find string; returns NO_RESULT if nothing is found
String find_time(char* find, char* expression, int offset) {
  char* idx_time = strstr(expression,find);
  if (idx_time != NULL) {
    char time[8];
    strncpy(time,(idx_time + offset), 8); // 32/31 (timetable/expected) characters offset to get to time
    time[8] = 0;
    String timetabled(time);
    return timetabled;
  }
  else return "NO_RESULT";
}

void parseResponse() {
  String response = "";
  while (client.connected()) {
    if (client.available()) {
      response += (char)client.read();
    }
  }

  client.stop();
  Serial.println("Disconnected from server.");

  char chptr[response.length()];
  response.toCharArray(chptr, response.length());

  String pln = find_time("trias:TimetabledTime", chptr, 32);
  String exp = find_time("trias:EstimatedTime", chptr, 31);
  Serial.println(pln);
  Serial.println(exp);
  //Serial.println(response);
}

void sendPostRequest(const char* xml_Data) {
  DEBUG_PRINT_LN("making POST request...");


  if (client.connect(server,port)) {
    DEBUG_PRINT_LN("connected to server");
    client.println("POST " + String(page) + " HTTP/1.1");
    client.println("Host: " + String(server));
    client.println("Content-Type: application/xml");
    client.println("Accept: application/xml");
    client.println("Authorization: Bearer " + String(apiKey));
    client.print("Content-Length: ");
    client.println(strlen(xml_Data));
    client.println();
    client.println(xml_Data);
    parseResponse();
    /* Read the response from the server
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
      }
    }
    DEBUG_PRINT_LN("\nDisconnecting from server.");
    client.stop();*/
  } else {
    DEBUG_PRINT_LN("Connection failed.");
  }

  DEBUG_PRINT_LN("Wait five seconds");
  delay(5000);
}

void loop() {
  // Keep the program alive
}

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  DEBUG_PRINT("SSID: ");
  DEBUG_PRINT_LN(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  DEBUG_PRINT("IP Address: ");
  DEBUG_PRINT_LN(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  DEBUG_PRINT("signal strength (RSSI):");
  DEBUG_PRINT(rssi);
  DEBUG_PRINT_LN(" dBm");
}