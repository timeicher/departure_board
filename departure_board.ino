#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINT_LN(x) Serial.println(x)
#define XML_BUFF_SIZE 1500  // Adjust size based on expected maximum XML length

#include <WiFiS3.h>
#include <TM1637Display.h>

#include "arduino_secrets.h"
#include "NTP.h"
#include "helpers.h"

char ssid[] = SECRET_SSID;      // your network SSID (name)
char pass[] = SECRET_PASS;      // your network password (use for WPA, or use as key for WEP)
const char* apiKey = API_KEY;   // your personal API Key

char server[] = "api.opentransportdata.swiss";
char page[] = "/trias2020";

int status = WL_IDLE_STATUS;
int port = 443;

int request_offset = 4; // how many minutes the request is set to the future (such that the next connection is shown sooner)
int cur_hour;
int cur_minute;
int cur_second;

WiFiSSLClient client;

// For updating the time
WiFiUDP wifiUdp;
NTP ntp(wifiUdp);
int diff; // difference from zulu time to current timezone

// times to display (planned and actual); if "NO_RESULT" there is no valid time; format is "hh:mm:ss" - hours, minutes, seconds
String pln1;
String pln2;
String act1;
String act2;

// delay of corresponding connection in seconds
int delay1;
int delay2;

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

// some states for the display
const uint8_t dis_err[] = {
  SEG_A | SEG_F | SEG_G | SEG_E | SEG_D, // E
  SEG_G | SEG_E, // r
  SEG_G | SEG_E, // r
  0
};

const uint8_t double_apastr = SEG_F | SEG_B; // ''

const uint8_t l_apastr = SEG_F; // ' at top left

const uint8_t minus_sign =  SEG_G;

void setup() {
  // Initialize serial and wait for port to open:
  Serial.begin(9600);

  // initialise displays
  int brightness = 0;
  uint8_t eight = dis_time1.encodeDigit(8);
  uint8_t eights[] = {eight, eight, eight, eight};
  eights[1] |= 0x80; // set semicolon

  dis_time1.setBrightness(brightness);
  dis_time1.setSegments(eights);
  dis_delay1.setBrightness(brightness);
  dis_delay1.setSegments(eights);
  dis_time2.setBrightness(brightness);
  dis_time2.setSegments(eights);
  dis_delay2.setBrightness(brightness);
  dis_delay2.setSegments(eights);

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

  // get current difference in time zone
  ntp.begin();
  int zulu_hour = atoi(ntp.formattedTime("%H"));
  while (atoi(ntp.formattedTime("%M")) == 59) {
    ntp.update();
    zulu_hour = atoi(ntp.formattedTime("%H"));
    delay(1000);
  }
  ntp.ruleDST("CEST", Last, Sun, Mar, 2, 120); // last sunday in march 2:00, timetone +120min (+1 GMT + 1h summertime offset)
  ntp.ruleSTD("CET", Last, Sun, Oct, 3, 60); // last sunday in october 3:00, timezone +60min (+1 GMT)
  ntp.begin();
  int zone_hour = atoi(ntp.formattedTime("%H"));
  diff = zone_hour - zulu_hour;
  DEBUG_PRINT_LN(diff);

  //============================================================================================
  // creating first requests and setting their respective displays
  //============================================================================================
  create_xml_request(pln1, act1, 1); // request current service information for departure board 1
  setdisplays(pln1, act1, dis_time1, dis_delay1, delay1); // setting display 1

  create_xml_request(pln2, act2, 2); // same for departure 2
  setdisplays(pln2, act2, dis_time2, dis_delay2, delay2);
}

void create_xml_request(String& pln, String& act, int information) {
  static char xmlData[XML_BUFF_SIZE];  // Static buffer to prevent frequent allocation
  memset(xmlData, 0, XML_BUFF_SIZE);  // Clear buffer

  // if current time + request_offset is still in the current day, add offset
  cur_hour = atoi(ntp.formattedTime("%H"));
  cur_minute = atoi(ntp.formattedTime("%M"));
  cur_second = atoi(ntp.formattedTime("%S"));
  if (!((cur_hour == 23) && (cur_minute >= 59 - request_offset))) {
    cur_minute += request_offset;
    if (cur_minute >= 60) {
      cur_hour++;
      cur_minute %= 60;
    } 
  }

  String cur_time = String(ntp.formattedTime("%Y")) + "-" + String(ntp.formattedTime("%m")) 
                    + "-" + String(ntp.formattedTime("%d")) + "T"
                    // add a leading zero if necessary
                    + ((cur_hour < 10) ? "0" + String(cur_hour) : String(cur_hour)) + ":" 
                    + ((cur_minute < 10) ? "0" + String(cur_minute) : String(cur_minute)) + ":"
                    + ((cur_second < 10) ? "0" + String(cur_second) : String(cur_second));


  DEBUG_PRINT_LN(cur_time);
  switch (information) {
    case 1:
      snprintf(xmlData, XML_BUFF_SIZE, "%s<DepArrTime>%s</DepArrTime>%s", XML_REQ1_1, cur_time.c_str(), XML_REQ1_2);
      break;
    case 2:
      snprintf(xmlData, XML_BUFF_SIZE, "%s<DepArrTime>%s</DepArrTime>%s", XML_REQ2_1, cur_time.c_str(), XML_REQ2_2);
    break;
  }
  sendPostRequest(xmlData, pln, act);
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

// sets one display to the desired state as this: 
// first|second|third|fourth
void setdisplay(uint8_t States[], bool colon, TM1637Display dis) {
  if (colon) States[1] |= 0x80; // Enable colon by setting bit 0x80 on middle segment if enabled

  dis.setSegments(States); // Display the time
}

// Sets a display; if exp_time is true we treat it as a delay display
void setdisplays(String plnstr, String actstr, TM1637Display dis_time, TM1637Display dis_delay, int delay) {
  uint8_t TimeDisp[] = {0x00, 0x00, 0x00, 0x00};
  uint8_t DelDisp[] = {0x00, 0x00, 0x00, 0x00};

  // if there is no expected departure time display error and return
  if (plnstr == "NO_RESULT") {
    dis_time.setSegments(dis_err);
    dis_delay.clear();
    return;
  }


  unsigned pln_len = plnstr.length() + 1;
  char num[pln_len]; // char* holding planned time
  char del[pln_len]; // char* holding actual time
  plnstr.toCharArray(num,pln_len);
  actstr.toCharArray(del,pln_len);


  // convert chars for planned departure time
  int h10 = int(num[0] + 48);
  int h01 = int(num[1] + 48); // hours
  int m10 = int(num[3] + 48);
  int m01 = int(num[4] + 48); // minutes
  int s10 = int(num[6] + 48);
  int s01 = int(num[7] + 48); // seconds

  // if expected time is available calculate delay
  if (actstr != "NO_RESULT") {
    // convert chars for actual departure time
    int ah10 = int(del[0] + 48);
    int ah01 = int(del[1] + 48); // hours
    int am10 = int(del[3] + 48);
    int am01 = int(del[4] + 48); // minutes
    int as10 = int(del[6] + 48);
    int as01 = int(del[7] + 48); // seconds
    
    // delay in seconds
    delay = (as01 - s01) + 10 * (as10 - s10) +
            60 * (am01 - m01) + 10 * 60 * (am10 - m10) +
            60 * 60 * (ah01 - h01) + 10 * 60 * 60 * (ah10 - h10)
            ;
  }
  else { // if not clear delay display
    dis_delay.clear();
    delay = 0;
  }

  // Display planned time
  set_time_diff(diff, h10, h01);
  encode_ints(h10, h01, m10, m01, TimeDisp);
  setdisplay(TimeDisp, true, dis_time);

  if (delay < 30) delay = 0; // ignore delays smaller than 30 seconds, since this can be withing the normal timetable

  // Display delay
  if (delay != 0) {
    int a, b, c, d;
    if (delay < 0) { // too early departure 2 digits of either seconds or minutes displayed
      if (delay > -60) {
        a = 0;
        b = abs(delay / 10 % 10);
        c = abs(delay / 1 % 10);
        d = 0;
        encode_ints(a,b,c,d,DelDisp);
        DelDisp[3] = double_apastr;
      }
      else {
        int del_min = abs(delay / 60);
        a = 0;
        b = del_min / 10 % 10;
        c = del_min / 1 % 10;
        d = 0;
        encode_ints(a,b,c,d,DelDisp);
        DelDisp[3] = l_apastr;
      }
      DelDisp[0] = minus_sign;
    }
    else if (delay < 60) { // less than a minute delay
      a = 0;
      b = delay / 10 % 10;
      c = delay / 1 % 10;
      d = 0;
      encode_ints(a,b,c,d,DelDisp);
      DelDisp[0] = 0x00;
      DelDisp[3] = double_apastr;
    }
    else if (delay < 600) { // less than 10 minutes delay
      a = delay / 60;
      b = 0;
      int del_sec = delay - (a * 60);
      c = del_sec / 10 % 10;
      d = del_sec % 10;
      encode_ints(a,b,c,d,DelDisp);
      DelDisp[1] = l_apastr;
    }
    else { // more than ten minutes delay
      int del_min = delay / 60;
      a = del_min / 100 % 10;
      b = del_min / 10 % 10;
      c = del_min / 1 % 10;
      encode_ints(a,b,c,d,DelDisp);
      DelDisp[3] = l_apastr;
    }
    setdisplay(DelDisp, false, dis_delay);
  }
  else {
    dis_delay.clear();
  }
  
}

// always the first display is encoded for; it is assumed that all displays are identical
void encode_ints(int first, int second, int third, int fourth, uint8_t States[]) {
  States[0] = dis_time1.encodeDigit(first); // 1st digit
  States[1] = dis_time1.encodeDigit(second); // 2nd digit
  States[2] = dis_time1.encodeDigit(third); // 3rd digit
  States[3] = dis_time1.encodeDigit(fourth); // 4th digit
}

void parseResponse(String& pln, String& act) {
  unsigned long start_time = millis();
  String response = "";
  while (client.connected()) {
    if (client.available()) {
      response += (char)client.read();
    }
  }
  unsigned long end_time = millis();
  //DEBUG_PRINT_LN(String(end_time - start_time) + "ms");

  client.stop();
  DEBUG_PRINT_LN("Disconnected from server.");

  char chptr[response.length()];
  response.toCharArray(chptr, response.length());

  pln = find_time("trias:TimetabledTime", chptr, 32);
  act = find_time("trias:EstimatedTime", chptr, 31);

  //pln1 = pln;
  //act1 = act;

  DEBUG_PRINT_LN(pln);
  DEBUG_PRINT_LN(act);
  if (pln == "NO_RESULT") DEBUG_PRINT_LN(response);
}

void sendPostRequest(const char* xml_Data, String& pln, String& act) {
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
    
    parseResponse(pln, act);


  } else {
    DEBUG_PRINT_LN("Connection failed.");
    while (true);
  }

  //delay(5000);
}

void loop() {
  // TODO: make more adaptable for different number of displays
  delay(5000);
  ntp.update();
  create_xml_request(pln1, act1, 1); // request current service information for departure board 1
  setdisplays(pln1, act1, dis_time1, dis_delay1, delay1); // setting display 1

  delay(5000);
  ntp.update();
  create_xml_request(pln2, act2, 2); // same for departure 2
  setdisplays(pln2, act2, dis_time2, dis_delay2, delay2);
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

