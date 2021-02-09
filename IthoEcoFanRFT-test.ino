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

#define DEBUG 2
#if defined (ESP8266)
#define STASSID "your-ssid"
#define STAPSK  "your-password"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ESPAsyncTCP.h>
#include <Hash.h>
#endif

#include <SPI.h>
#include "IthoCC1101.h"
#include "IthoPacket.h"

#define ITHO_IRQ_PIN D2

IthoCC1101 rf;
IthoPacket packet;

int originIDX = 0;
char * origin[3] = {"remote", "itho", "rft-rv"};

const uint8_t RFTid[] = {224, 88, 81}; // remote
const uint8_t RFTidItho[] = {80, 5, 196}; // itho
const uint8_t RFTidRFTRV[] = {151, 149, 65}; // rft-rv

bool ITHOhasPacket = false;
IthoCommand RFTcommand[3] = {IthoUnknown, IthoUnknown, IthoUnknown};
byte RFTRSSI[3] = {0, 0, 0};
byte RFTcommandpos = 0;
IthoCommand RFTlastCommand = IthoLow;
IthoCommand RFTstate = IthoUnknown;
IthoCommand savedRFTstate = IthoUnknown;
bool RFTidChk[3] = {false, false, false};

void setup(void) {
  Serial.begin(115200);
  delay(500);
  Serial.println("setup begin");

#if defined (ESP8266)
  WiFi.persistent(false);
  WiFi.mode(WIFI_STA);
  WiFi.begin(STASSID, STAPSK);
  delay(2000);
  configTime(0, 0, "pool.ntp.org");
#endif

  rf.init();
  Serial.println("setup done");
  sendRegister();
  Serial.println("join command sent");
  pinMode(ITHO_IRQ_PIN, INPUT);
  attachInterrupt(ITHO_IRQ_PIN, ITHOcheck, FALLING);
}

void loop(void) {
  // do whatever you want, check (and reset) the ITHOhasPacket flag whenever you like
  if (ITHOhasPacket) {
    showPacket();
  }
}

#if defined (ESP8266)
ICACHE_RAM_ATTR void ITHOcheck() {
#else
void ITHOcheck() {
#endif
  ITHOhasPacket = true;
}


void showPacket() {
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
        Serial.println(rf.LastMessageDecode());
      }
    }
    if (DEBUG == 2) { //activate DEBUG=2 if you want to see all remotes and commands back in the log
      if (cmd != IthoUnknown) {
        printTime();
        if (chk) {
          Serial.print(F("Known remote: "));
          Serial.print(origin[originIDX]);
        }
        else {
          Serial.print(F("Unknown remote"));
        }
        Serial.print(F(" / known command: "));
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
        printTime();
        if (chk) {
          Serial.print(F("Known remote: "));
          Serial.print(origin[originIDX]);
          Serial.println(F(" / unknown command:"));
        }
        else {
          Serial.println(F("Unknown remote / unknown command:"));
        }
      }
      printTime();
      Serial.println(rf.LastMessageDecode());
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

void sendRegister() {
  Serial.println("sending join...");
  rf.sendCommand(IthoJoin);
  Serial.println("sending join done.");
}

void sendStandbySpeed() {
  Serial.println("sending standby...");
  rf.sendCommand(IthoStandby);
  Serial.println("sending standby done.");
}

void sendLowSpeed() {
  Serial.println("sending low...");
  rf.sendCommand(IthoLow);
  Serial.println("sending low done.");
}

void sendMediumSpeed() {
  Serial.println("sending medium...");
  rf.sendCommand(IthoMedium);
  Serial.println("sending medium done.");
}

void sendHighSpeed() {
  Serial.println("sending high...");
  rf.sendCommand(IthoHigh);
  Serial.println("sending high done.");
}

void sendFullSpeed() {
  Serial.println("sending FullSpeed...");
  rf.sendCommand(IthoFull);
  Serial.println("sending FullSpeed done.");
}

void sendTimer() {
  Serial.println("sending timer...");
  rf.sendCommand(IthoTimer1);
  Serial.println("sending timer done.");
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
