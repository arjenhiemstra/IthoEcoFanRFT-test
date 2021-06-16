/*
   Original Author: Klusjesman

   Tested with STK500 + ATMega328P
   GCC-AVR compiler

   Modified by supersjimmie:
   Code and libraries made compatible with Arduino and ESP8266
   Tested with Arduino IDE v1.6.5 and 1.6.9
   For ESP8266 tested with ESP8266 core for Arduino v 2.1.0 and 2.2.0 Stable
   (See https://github.com/esp8266/Arduino/ )

*/
/*
  CC11xx pins    ESP pins Arduino pins  Description
  1 - VCC        VCC      VCC           3v3
  2 - GND        GND      GND           Ground
  3 - MOSI       13=D7    Pin 11        Data input to CC11xx
  4 - SCK        14=D5    Pin 13        Clock pin
  5 - MISO/GDO1  12=D6    Pin 12        Data output from CC11xx / serial clock from CC11xx
  6 - GDO2       04=D2    Pin 2?        Serial data to CC11xx
  7 - GDO0       ?        Pin  ?        output as a symbol of receiving or sending data
  8 - CSN        15=D8    Pin 10        Chip select / (SPI_SS)
*/

#include <ESP8266mDNS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <Ethernet.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <TimeLib.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>


#include <SPI.h>
#include "IthoCC1101.h"
#include "IthoPacket.h"
// webserver
ESP8266WebServer  server(80);
MDNSResponder   mdns;
WiFiClient client;
IthoCC1101 rf;
IthoPacket packet;

/*

   BEGIN USER SETTINGS

*/
#define SOFTWARE_VERSION "0.1"
#define DEBUG 2
//#define MQTT_ENABLE

#define ITHO_IRQ_PIN D2

// WIFI
#define CONFIG_WIFI_SSID "SSID"
#define CONFIG_WIFI_PASS "PASS0000"
#define CONFIG_HOST_NAME "ESP-ITHO"
// MQTT

#if defined (MQTT_ENABLE)
IPAddress mqttIP(192, 168, 0, 2);
#define CONFIG_MQTT_HOST "" //if host is left empty mqttIP is used
#define CONFIG_MQTT_USER ""
#define CONFIG_MQTT_PASS ""
#define CONFIG_MQTT_CLIENT_ID "MQTTITHO02" // Must be unique on the MQTT network

// MQTT Topics
#define CONFIG_MQTT_TOPIC_STATE "espitho/fan/state"
#define CONFIG_MQTT_TOPIC_COMMAND "espitho/fan/cmd"
#define CONFIG_MQTT_TOPIC_IDINDEX "espitho/fan/LastIDindex" //"1" if external remote triggered state topic update, "0" if state topic updated from API or MQTT cmd topic
#endif

//Itho
char * origin[3] = {"remote", "itho", "rft-rv"};

const uint8_t RFTid[] = {224, 88, 81}; // remote
const uint8_t RFTidItho[] = {80, 5, 196}; // itho
const uint8_t RFTidRFTRV[] = {151, 149, 65}; // rft-rv

// pick three numbers for the device ID of this 'remote'
#define DEVICE_ID1 13
#define DEVICE_ID2 123
#define DEVICE_ID3 42

/*

   END USER SETTINGS

*/


// WIFI
String ssid    = CONFIG_WIFI_SSID;
String password = CONFIG_WIFI_PASS;
String espName    = CONFIG_HOST_NAME;

String Version = SOFTWARE_VERSION;
String ClientIP;
#if defined (MQTT_ENABLE)
// MQTT
const char* mqttHost = CONFIG_MQTT_HOST;
const char* mqttUsername = CONFIG_MQTT_USER;
const char* mqttPassword = CONFIG_MQTT_PASS;
const char* mqttClientId = CONFIG_MQTT_CLIENT_ID;
PubSubClient mqttclient(client);
long lastMsg = 0;
char msg[50];


// MQTT Topics
const char* commandtopic = CONFIG_MQTT_TOPIC_COMMAND;
const char* statetopic = CONFIG_MQTT_TOPIC_STATE;
const char* idindextopic = CONFIG_MQTT_TOPIC_IDINDEX;
#endif
//HTML
String header       =  "<html lang='en'><head><title>Itho control panel</title><meta charset='utf-8'><meta name='viewport' content='width=device-width, initial-scale=1'><link rel='stylesheet' href='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/css/bootstrap.min.css'><script src='https://ajax.googleapis.com/ajax/libs/jquery/1.11.1/jquery.min.js'></script><script src='http://maxcdn.bootstrapcdn.com/bootstrap/3.3.4/js/bootstrap.min.js'></script></head><body>";
String navbar       =  "<nav class='navbar navbar-default'><div class='container-fluid'><div class='navbar-header'><a class='navbar-brand' href='/'>Itho control panel</a></div><div><ul class='nav navbar-nav'><li><a href='/'><span class='glyphicon glyphicon-question-sign'></span> Status</a></li><li class='dropdown'><a class='dropdown-toggle' data-toggle='dropdown' href='#'><span class='glyphicon glyphicon-cog'></span> Tools<span class='caret'></span></a><ul class='dropdown-menu'><li><a href='/api?action=reset&value=true'>Restart</a></ul></li><li><a href='https://github.com/incmve/Itho-WIFI-remote' target='_blank'><span class='glyphicon glyphicon-question-sign'></span> Help</a></li></ul></div></div></nav>  ";

String containerStart   =  "<div class='container'><div class='row'>";
String containerEnd     =  "<div class='clearfix visible-lg'></div></div></div>";
String siteEnd        =  "</body></html>";

String panelHeaderName    =  "<div class='col-md-4'><div class='page-header'><h1>";
String panelHeaderEnd   =  "</h1></div>";
String panelEnd       =  "</div>";

String panelBodySymbol    =  "<div class='panel panel-default'><div class='panel-body'><span class='glyphicon glyphicon-";
String panelBodyDuoSymbol    =  "</span></div><div class='panel-body'><span class='glyphicon glyphicon-";
String panelBodyName    =  "'></span> ";
String panelBodyValue   =  "<span class='pull-right'>";
String panelcenter   =  "<div class='row'><div class='span6' style='text-align:center'>";
String panelBodyEnd     =  "</span></div></div>";

String inputBodyStart   =  "<form action='' method='POST'><div class='panel panel-default'><div class='panel-body'>";
String inputBodyName    =  "<div class='form-group'><div class='input-group'><span class='input-group-addon' id='basic-addon1'>";
String inputBodyPOST    =  "</span><input type='text' name='";
String inputBodyClose   =  "' class='form-control' aria-describedby='basic-addon1'></div></div>";
String ithocontrol     =  "<a href='/button?action=Low'<button type='button' class='btn btn-default'> Low</button></a><a href='/button?action=Medium'<button type='button' class='btn btn-default'> Medium</button></a><a href='/button?action=High'<button type='button' class='btn btn-default'> High</button><a href='/button?action=Timer'<button type='button' class='btn btn-default'> Timer</button></a></a><br><a href='/button?action=Join'<button type='button' class='btn btn-default'> Join</button></a><a href='/button?action=Leave'<button type='button' class='btn btn-default'> Leave</button></a></div>";


int originIDX = 0;

bool ITHOhasPacket = false;
IthoCommand RFTcommand[3] = {IthoUnknown, IthoUnknown, IthoUnknown};
byte RFTRSSI[3] = {0, 0, 0};
byte RFTcommandpos = 0;
IthoCommand RFTlastCommand = IthoLow;
IthoCommand RFTstate = IthoUnknown;
IthoCommand savedRFTstate = IthoUnknown;
bool RFTidChk[3] = {false, false, false};
String Laststate;
String CurrentState;
char lastidindex[2];

unsigned long loopstart = 0;
unsigned long sendCmd = 0;

void setup(void) {
  Serial.begin(115200);
  delay(500);
  Serial.println("setup begin");

#if defined (ESP8266)
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  delay(500);
  WiFi.hostname(espName);

  WiFi.begin(ssid.c_str(), password.c_str());
  int i = 0;
  while (WiFi.status() != WL_CONNECTED && i < 31)
  {
    delay(1000);
    Serial.print(".");
    ++i;
  }
  if (WiFi.status() != WL_CONNECTED && i >= 30)
  {
    WiFi.disconnect();
    delay(1000);
    Serial.println("");
    Serial.println("Couldn't connect to network :( ");
    Serial.println("Review your WIFI settings");

  }
  else
  {
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    Serial.print("Hostname: ");
    Serial.println(espName);

  }
  delay(2000);
  configTime(0, 0, "pool.ntp.org");
#endif


  Serial.println("\n*** setup RF ***\n");
  rf.setDeviceID(DEVICE_ID1, DEVICE_ID2, DEVICE_ID3); //DeviceID used to send commands, can also be changed on the fly for multi itho control
  rf.init();


  Serial.println("\n*** setup webserver ***\n");
  server.on("/", handle_root);
  server.on("/api", handle_api);
  server.on("/button", handle_buttons);


  if (!mdns.begin(espName.c_str(), WiFi.localIP())) {
    Serial.println("Error setting up MDNS responder!");
  }

  server.begin();
  Serial.println("HTTP server started");
  MDNS.addService("http", "tcp", 80);
#if defined (MQTT_ENABLE)
  Serial.println("\n*** setup MQTT ***\n");
  if (mqttHost == "") {
    mqttclient.setServer(mqttIP, 1883);
    Serial.println("setup MQTT using IP");
  }
  else {
    mqttclient.setServer(mqttHost, 1883);
    Serial.println("setup MQTT using hostname");
  }

  mqttclient.setCallback(callback);
#endif
  Serial.println("\n*** setup done ***\n");

  pinMode(ITHO_IRQ_PIN, INPUT);
  attachInterrupt(ITHO_IRQ_PIN, ITHOcheck, RISING);
  //rf.setCCcalEnable(1);

}

bool cccal = true;


void loop(void) {
  delay(0);
  server.handleClient();

  loopstart = millis();
//  if (loopstart - sendCmd > 1000 && cccal) {
//    sendCmd = loopstart;
//    if(rf.getCCcalFinised()) {
//      cccal = false;
//    }    
//    printf("getCCcalEnabled(): %d\n", rf.getCCcalEnabled());
//    printf("getCCcalFinised(): %d\n", rf.getCCcalFinised());
//    printf("cal_task timeout timer: %lu\n", rf.getCCcalTimer());
//
//  }
  // do whatever you want, check (and reset) the ITHOhasPacket flag whenever you like
  if (ITHOhasPacket) {
    showPacket();
  }
#if defined (MQTT_ENABLE)
  if (!mqttclient.connected()) {
    reconnect();
  }
  mqttclient.loop();
#endif
}

#if defined (ESP8266)
ICACHE_RAM_ATTR void ITHOcheck() {
#else
void ITHOcheck() {
#endif
  if (rf.receivePacket()) {
    ITHOhasPacket = true;
  }
}


ICACHE_RAM_ATTR void showPacket() {
  ITHOhasPacket = false;
  if (rf.checkForNewPacket()) {
    IthoCommand cmd = rf.getLastCommand();
    if (++RFTcommandpos > 2) RFTcommandpos = 0;  // store information in next entry of ringbuffers
    RFTcommand[RFTcommandpos] = cmd;
    RFTRSSI[RFTcommandpos]    = rf.ReadRSSI();
    bool chk = false;
    if (rf.checkID(RFTid)) {
      chk = true;
      originIDX = 0;
    }
    else if (rf.checkID(RFTidItho)) {
      chk = true;
      originIDX = 1;
    }
    else if (rf.checkID(RFTidRFTRV)) {
      chk = true;
      originIDX = 2;
    }
    RFTidChk[RFTcommandpos]   = chk;
    if (DEBUG == 1 && chk) { //only act on know remote ID to filter results
      printTime();
      Serial.print("Origin:");
      Serial.println(origin[originIDX]);
      printTime();
      if (cmd != IthoUnknown) {
        Serial.print("command:");
        switch (cmd) {
          case IthoStandby:
            Serial.print("standby/auto\n");
            break;
          case IthoLow:
            Serial.print("low\n");
            break;
          case IthoMedium:
            Serial.print("medium\n");
            break;
          case IthoHigh:
            Serial.print("high\n");
            break;
          case IthoFull:
            Serial.print("full\n");
            break;
          case IthoTimer1:
            Serial.print("timer1\n");
            break;
          case IthoTimer2:
            Serial.print("timer2\n");
            break;
          case IthoTimer3:
            Serial.print("timer3\n");
            break;
          case IthoJoin:
            Serial.print("join\n");
            break;
          case IthoLeave:
            Serial.print("leave\n");
            break;
        }
      }
      else {
        Serial.println(rf.LastMessageDecoded());
      }
    }
    if (DEBUG == 2) { //activate DEBUG=2 if you want to see all remotes and commands back in the log
//      if (cmd != IthoUnknown) {
//        printTime();
//        if (chk) {
//          Serial.print(F("Known remote: "));
//          Serial.print(origin[originIDX]);
//        }
//        else {
//          Serial.print(F("Unknown remote"));
//        }
//        Serial.print(F(" / known command: "));
//        switch (cmd) {
//          case IthoStandby:
//            Serial.print("standby/auto\n");
//            break;
//          case IthoLow:
//            Serial.print("low\n");
//            break;
//          case IthoMedium:
//            Serial.print("medium\n");
//            break;
//          case IthoHigh:
//            Serial.print("high\n");
//            break;
//          case IthoFull:
//            Serial.print("full\n");
//            break;
//          case IthoTimer1:
//            Serial.print("timer1\n");
//            break;
//          case IthoTimer2:
//            Serial.print("timer2\n");
//            break;
//          case IthoTimer3:
//            Serial.print("timer3\n");
//            break;
//          case IthoJoin:
//            Serial.print("join\n");
//            break;
//          case IthoLeave:
//            Serial.print("leave\n");
//            break;
//        }
//      }
//      else {
//        printTime();
//        if (chk) {
//          Serial.print(F("Known remote: "));
//          Serial.print(origin[originIDX]);
//          Serial.println(F(" / unknown command:"));
//        }
//        else {
//          Serial.println(F("Unknown remote / unknown command:"));
//        }
//      }
      printTime();
      Serial.print(rf.LastMessageDecoded());
    }
  }
}
uint8_t findRFTlastCommand() {
  if (RFTcommand[RFTcommandpos] != IthoUnknown)               return RFTcommandpos;
  if ((RFTcommandpos == 0) && (RFTcommand[2] != IthoUnknown)) return 2;
  if ((RFTcommandpos == 0) && (RFTcommand[1] != IthoUnknown)) return 1;
  if ((RFTcommandpos == 1) && (RFTcommand[0] != IthoUnknown)) return 0;
  if ((RFTcommandpos == 1) && (RFTcommand[2] != IthoUnknown)) return 2;
  if ((RFTcommandpos == 2) && (RFTcommand[1] != IthoUnknown)) return 1;
  if ((RFTcommandpos == 2) && (RFTcommand[0] != IthoUnknown)) return 0;
  return -1;
}

void sendJoin() {
  rf.sendCommand(IthoJoin);
  Serial.println("sending join done.");
}
void sendLeave() {
  rf.sendCommand(IthoLeave);
  Serial.println("sending leave done.");
}
void sendStandbySpeed() {
  rf.sendCommand(IthoStandby);
  CurrentState = "Standby";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending standby done.");
}

void sendLowSpeed() {
  rf.sendCommand(IthoLow);
  CurrentState = "Low";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending low done.");
}

void sendMediumSpeed() {
  rf.sendCommand(IthoMedium);
  CurrentState = "Medium";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending medium done.");
}

void sendHighSpeed() {
  rf.sendCommand(IthoHigh);
  CurrentState = "High";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending high done.");
}

void sendFullSpeed() {
  rf.sendCommand(IthoFull);
  CurrentState = "Full";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending FullSpeed done.");
}

void sendTimer1() {
  rf.sendCommand(IthoTimer1);
  CurrentState = "Timer1";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending timer1 done.");
}
void sendTimer2() {
  rf.sendCommand(IthoTimer2);
  CurrentState = "Timer2";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending timer2 done.");
}
void sendTimer3() {
  rf.sendCommand(IthoTimer3);
  CurrentState = "Timer3";
#if defined (MQTT_ENABLE)
  mqttclient.publish(statetopic, CurrentState.c_str());
  mqttclient.publish(idindextopic, "0");
#endif
  Serial.println("sending timer3 done.");
}

void printTime() {
  time_t now;
  struct tm * timeinfo;
  time(&now);
  timeinfo = localtime(&now);

  char timeStringBuff[50];  // 50 chars should be enough
  strftime(timeStringBuff, sizeof(timeStringBuff), "%F %T ", timeinfo);
  Serial.print(timeStringBuff);

}


// ROOT page
void handle_root()
{
  // get IP
  IPAddress ip = WiFi.localIP();
  ClientIP = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  delay(500);

  String title1     = panelHeaderName + String("Itho WIFI remote") + panelHeaderEnd;
  String IPAddClient    = panelBodySymbol + String("globe") + panelBodyName + String("IP Address") + panelBodyValue + ClientIP + panelBodyEnd;
  String ClientName   = panelBodySymbol + String("tag") + panelBodyName + String("Client Name") + panelBodyValue + espName + panelBodyEnd;
  String ithoVersion   = panelBodySymbol + String("ok") + panelBodyName + String("Version") + panelBodyValue + Version + panelBodyEnd;
  String State   = panelBodySymbol + String("info-sign") + panelBodyName + String("Current state") + panelBodyValue + CurrentState + panelBodyEnd;
  String Uptime     = panelBodySymbol + String("time") + panelBodyName + String("Uptime") + panelBodyValue + hour() + String(" h ") + minute() + String(" min ") + second() + String(" sec") + panelBodyEnd + panelEnd;


  String title3 = panelHeaderName + String("Commands") + panelHeaderEnd;

  String commands = panelBodySymbol + panelBodyName + panelcenter + ithocontrol + panelBodyEnd;


  server.send ( 200, "text/html", header + navbar + containerStart + title1 + IPAddClient + ClientName + ithoVersion + State + Uptime + title3 + commands + containerEnd + siteEnd);
}
void handle_api()
{
  // Get var for all commands
  String action = server.arg("action");
  String value = server.arg("value");
  String api = server.arg("api");

  if (action == "Receive")
  {
    rf.initReceive();
    server.send ( 200, "text/html", "Receive mode");
  }

  if (action == "High")
  {
    sendFullSpeed();
    server.send ( 200, "text/html", "Full Powerrr!!!");
  }

  if (action == "Medium")
  {
    sendMediumSpeed();
    server.send ( 200, "text/html", "Medium speed selected");
  }

  if (action == "Low")
  {
    sendLowSpeed();
    server.send ( 200, "text/html", "Slow speed selected");
  }

  if (action == "Timer")
  {
    sendTimer1();
    server.send ( 200, "text/html", "Timer1 on selected");
  }

  if (action == "Join")
  {
    sendJoin();
    server.send ( 200, "text/html", "Send join command OK");
  }

  if (action == "Leave")
  {
    sendLeave();
    server.send ( 200, "text/html", "Send leave command OK");
  }

  if (action == "reset" && value == "true")
  {
    server.send ( 200, "text/html", "Reset ESP OK");
    delay(500);
    Serial.println("RESET");
    ESP.restart();
  }

}

void handle_buttons()
{
  // Get vars for all commands
  String action = server.arg("action");
  String api = server.arg("api");

  if (action == "High")
  {
    sendFullSpeed();
    handle_root();
  }

  if (action == "Medium")
  {
    sendMediumSpeed();
    handle_root();
  }

  if (action == "Low")
  {
    sendLowSpeed();
    handle_root();
  }

  if (action == "Timer")
  {
    sendTimer1();
    handle_root();
  }

  if (action == "Join")
  {
    sendJoin();
    handle_root();
  }

  if (action == "Leave")
  {
    sendLeave();
    handle_root();
  }

}
#if defined (MQTT_ENABLE)
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  payload[length] = '\0';
  String strPayload = String((char*)payload);

  if (strPayload == "Join") {
    sendJoin();
  }
  else if (strPayload == "Leave") {
    sendLeave();
  }
  else if (strPayload == "Standby") {
    sendStandbySpeed();
  }
  else if (strPayload == "Low") {
    sendLowSpeed();
  }
  else if (strPayload == "Medium") {
    sendMediumSpeed();
  }
  else if (strPayload == "High") {
    sendHighSpeed();
  }
  else if (strPayload == "Full") {
    sendFullSpeed();
  }
  else if (strPayload == "Timer1") {
    sendTimer1();
  }
  else if (strPayload == "Timer2") {
    sendTimer2();
  }
  else if (strPayload == "Timer3") {
    sendTimer3();
  }
  else if (strPayload == "Reset") {
    ESP.restart();
  }
  else {
    Serial.println("Payload unknown");
  }
}



void reconnect() {
  // Loop until we're reconnected
  while (!mqttclient.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqttclient.connect(mqttClientId, mqttUsername, mqttPassword)) {
      Serial.println("connected");
      // subscribe
      mqttclient.subscribe(commandtopic);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttclient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
#endif
