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

// times to display (planned and actual); if "NO_RESULT" there is no valid time; format is "hh:mm:ss" - hours, minutes, seconds
String pln1;
String pln2;
String act1;
String act2;

// Prepare XML data
const char* xmlData = XML_REQ;

// initialisiation of the displays
#define CLK_T1 2
#define DIO_T1 3
TM1637Display dis_time1(CLK_T1, DIO_T1);

#define CLK_D1 4
#define DIO_D1 5
TM1637Display dis_delay1(CLK_D1, DIO_D1);

#define CLK_T2 7
#define DIO_T2 6
TM1637Display dis_time2(CLK_T2, DIO_T2);

#define CLK_D2 12
#define DIO_D2 11
TM1637Display dis_delay2(CLK_D2, DIO_D2);

bool display_debug = true;

void setup() {
  //Initialize serial and wait for port to open:
  Serial.begin(9600);
  int brightness = 0;
  dis_time1.setBrightness(brightness);
  dis_time1.clear();
  dis_delay1.setBrightness(brightness);
  dis_delay1.clear();
  dis_time2.setBrightness(brightness);
  dis_time2.clear();
  dis_delay2.setBrightness(brightness);
  dis_delay2.clear();

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

  setdisplay(pln1, act1, dis_time1, dis_delay1);
}

// Sets a display; if exp_time is true we treat it as a delay display
void setdisplay(String plnstr, String actstr, TM1637Display dis_time, TM1637Display dis_delay) {
  uint8_t TimeDisp[] = {0x00, 0x00, 0x00, 0x00};

  if (actstr == "NO_RESULT") {
    dis_delay.clear();
  }
  // if there is no expected departure time display error
  if (plnstr == "NO_RESULT") {
    const uint8_t dis_err[] = {
      SEG_A | SEG_F | SEG_G | SEG_E | SEG_D, // E
      SEG_G | SEG_E, // R
      SEG_G | SEG_E, // R
      0
    };
    dis_time.setSegments(dis_err);
    dis_delay.clear();
    return;
  }
  

  unsigned pln_len = plnstr.length();
  char num[pln_len];
  char del[pln_len];
  plnstr.toCharArray(num,pln_len);
  actstr.toCharArray(del,pln_len);

  // TODO
  int time_diff = 0;

  // convert chars for planned departure time
  int h10 = int(num[0] + 48);
  int h01 = int(num[1] + 48); // hours
  int m10 = int(num[3] + 48);
  int m01 = int(num[4] + 48); // minutes
  int s10 = int(num[6] + 48);
  int s01 = int(num[7] + 48); // seconds


  TimeDisp[0] = dis_time.encodeDigit(h10); // 1st digit
  TimeDisp[1] = dis_time.encodeDigit(h01); // 2nd digit
  TimeDisp[2] = dis_time.encodeDigit(m10); // 3rd digit
  TimeDisp[3] = dis_time.encodeDigit(m01); // 4th digit

  // Enable colon by setting bit 0x80 on middle segment
  TimeDisp[1] |= 0x80;

  dis_time.setSegments(TimeDisp); // Display the time with colon
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

void parseResponse(String pln, String act) {
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

  pln = find_time("trias:TimetabledTime", chptr, 32);
  act = find_time("trias:EstimatedTime", chptr, 31);

  pln1 = pln;
  act1 = act;

  Serial.println(pln);
  Serial.println(act);
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
    
    parseResponse(pln1, act1);

  } else {
    DEBUG_PRINT_LN("Connection failed.");
  }

  //delay(5000);
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