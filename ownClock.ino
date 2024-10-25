///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/* === CYD ROLLING CLOCK ===
This example shows a digital clock with a rolling effect as the digits change.
Most of the code are borrowed from other examples. Thanks Internet!

You will have to modify the PREFERENCES section in RollingClock.ino to your WiFi and TimeZone.
*/

#include <Arduino.h>
#include <TimeLib.h> // Time Library provides Time and Date conversions
#include <WiFi.h>    // To connect to WiFi
#include <WiFiUdp.h> // To communicate with NTP server
#include <Timezone.h>

#define TOUCH_CS // This sketch does not use touch, but this is defined to quiet the warning about not defining touch_cs.

/*-------- DEBUGGING ----------*/
void Debug(String label, int val)
{
  Serial.print(label);
  Serial.print("=");
  Serial.println(val);
}

/*-------- TIME SERVER ----------*/
// NTP Servers:
static const char ntpServerName[] = "us.pool.ntp.org";
// static const char ntpServerName[] = "time.nist.gov";
// static const char ntpServerName[] = "time-a.timefreq.bldrdoc.gov";
// static const char ntpServerName[] = "time-b.timefreq.bldrdoc.gov";
// static const char ntpServerName[] = "time-c.timefreq.bldrdoc.gov";

WiFiUDP Udp;
unsigned int localPort = 8888; // local port to listen for UDP packets
TimeChangeRule *tcr;           // pointer to the time change rule, use to get TZ abbrev

/*-------- PREFERENCES ----------*/
String credentials[][2] = {
    {":)", "123456789"},
    {"OptionalOtherSSID", "OptionalOtherSSDPassword"},
};
const bool SHOW_24HOUR = true;
const bool SHOW_AMPM = false;
const bool NOT_US_DATE = true;

// Info about these settings at https://github.com/JChristensen/Timezone#coding-timechangerules
TimeChangeRule myStandardTime = {"CEST", First, Sun, Nov, 2, 1 * 60};
TimeChangeRule myDaylightSavingsTime = {"CET", Second, Sun, Mar, 2, 2 * 60};

// TimeChangeRule myStandardTime = {"GMT", First, Sun, Nov, 2, 0};
// TimeChangeRule myDaylightSavingsTime = {"IST", Second, Sun, Mar, 2, 1 * 60};
Timezone myTZ(myStandardTime, myDaylightSavingsTime);
static const int ntpSyncIntervalInSeconds = 300; // How often to sync with time server (300 = every five minutes)

/*-------- CYD (Cheap Yellow Display) ----------*/
#include <TFT_eSPI.h> // Hardware-specific library
#include <SPI.h>
TFT_eSPI tft = TFT_eSPI();              // Invoke custom library
TFT_eSprite sprite = TFT_eSprite(&tft); // Sprite class

int clockFont = 1;
int clockSize = 6;
int clockDatum = TL_DATUM;
uint16_t clockBackgroundColor = TFT_BLACK;
uint16_t clockFontColor = TFT_GREEN;
int prevDay = 0;

void SetupCYD()
{
  tft.init();
  tft.fillScreen(clockBackgroundColor);
  tft.setTextColor(clockFontColor, clockBackgroundColor);

  tft.setRotation(1);
  tft.setTextFont(clockFont);
  tft.setTextSize(clockSize);
  tft.setTextDatum(clockDatum);

  sprite.createSprite(tft.textWidth("8"), tft.fontHeight());
  sprite.setTextColor(clockFontColor, clockBackgroundColor);
  sprite.setRotation(1);
  sprite.setTextFont(clockFont);
  sprite.setTextSize(clockSize);
  sprite.setTextDatum(clockDatum);
}

/*-------- Digits ----------*/
#include "Digit.h"
Digit *digs[6];
int colons[2];
int timeY = 50;
int ampm[2]; // X, Y of the AM or PM indicator
bool ispm;

void CalculateDigitOffsets()
{
  int y = timeY;
  int width = tft.width();
  int DigitWidth = tft.textWidth("8");
  int colonWidth = tft.textWidth(":");
  int left = SHOW_AMPM ? 10 : (width - DigitWidth * 6 - colonWidth * 2) / 2;
  digs[0]->SetXY(left, y);                      // HH
  digs[1]->SetXY(digs[0]->X() + DigitWidth, y); // HH

  colons[0] = digs[1]->X() + DigitWidth; // :

  digs[2]->SetXY(colons[0] + colonWidth, y); // MM
  digs[3]->SetXY(digs[2]->X() + DigitWidth, y);

  colons[1] = digs[3]->X() + DigitWidth; // :

  digs[4]->SetXY(colons[1] + colonWidth, y); // SS
  digs[5]->SetXY(digs[4]->X() + DigitWidth, y);

  ampm[0] = digs[5]->X() + DigitWidth + 4;
  ampm[1] = y - 2;
}

void SetupDigits()
{
  tft.fillScreen(clockBackgroundColor);
  tft.setTextFont(clockFont);
  tft.setTextSize(clockSize);
  tft.setTextDatum(clockDatum);

  for (size_t i = 0; i < 6; i++)
  {
    digs[i] = new Digit(0);
    digs[i]->Height(tft.fontHeight());
  }

  //-- Measure font widths --
  // Debug("1", tft.textWidth("1"));
  // Debug(":", tft.textWidth(":"));
  // Debug("8", tft.textWidth("8"));

  CalculateDigitOffsets();
}

/*-------- DRAWING ----------*/
void DrawColons()
{
  tft.setTextFont(clockFont);
  tft.setTextSize(clockSize);
  tft.setTextDatum(clockDatum);
  tft.drawChar(':', colons[0], timeY);
  tft.drawChar(':', colons[1], timeY);
}

void DrawAmPm()
{
  if (SHOW_AMPM)
  {
    tft.setTextSize(3);
    tft.drawChar(ispm ? 'P' : 'A', ampm[0], ampm[1]);
    tft.drawChar('M', ampm[0], ampm[1] + tft.fontHeight());
  }
}

void DrawADigit(Digit *digg); // Without this line, compiler says: error: variable or field 'DrawADigit' declared void.

void DrawADigit(Digit *digg)
{
  if (digg->Value() == digg->NewValue())
  {
    sprite.drawNumber(digg->Value(), 0, 0);
    sprite.pushSprite(digg->X(), digg->Y());
  }
  else
  {
    for (size_t f = 0; f <= digg->Height(); f++)
    {
      digg->Frame(f);
      sprite.drawNumber(digg->Value(), 0, -digg->Frame());
      sprite.drawNumber(digg->NewValue(), 0, digg->Height() - digg->Frame());
      sprite.pushSprite(digg->X(), digg->Y());
      delay(5);
    }
    digg->Value(digg->NewValue());
  }
}

void DrawDigitsAtOnce()
{
  tft.setTextDatum(TL_DATUM);
  for (size_t f = 0; f <= digs[0]->Height(); f++) // For all animation frames...
  {
    for (size_t di = 0; di < 6; di++) // for all Digits...
    {
      Digit *dig = digs[di];
      if (dig->Value() == dig->NewValue()) // If Digit is not changing...
      {
        if (f == 0) //... and this is first frame, just draw it to screeen without animation.
        {
          sprite.drawNumber(dig->Value(), 0, 0);
          sprite.pushSprite(dig->X(), dig->Y());
        }
      }
      else // However, if a Digit is changing value, we need to draw animation frame "f"
      {
        dig->Frame(f);                                                       // Set the animation offset
        sprite.drawNumber(dig->Value(), 0, -dig->Frame());                   // Scroll up the current value
        sprite.drawNumber(dig->NewValue(), 0, dig->Height() - dig->Frame()); // while make new value appear from below
        sprite.pushSprite(dig->X(), dig->Y());                               // Draw the current animation frame to actual screen.
      }
    }
    delay(5);
  }

  // Once all animations are done, then we can update all Digits to current new values.
  for (size_t di = 0; di < 6; di++)
  {
    Digit *dig = digs[di];
    dig->Value(dig->NewValue());
  }
}

void DrawDigitsWithoutAnimation()
{
  for (size_t di = 0; di < 6; di++)
  {
    Digit *dig = digs[di];
    dig->Value(dig->NewValue());
    dig->Frame(0);
    sprite.drawNumber(dig->NewValue(), 0, 0);
    sprite.pushSprite(dig->X(), dig->Y());
  }
}

void DrawDigitsOneByOne()
{
  tft.setTextDatum(TL_DATUM);
  for (size_t i = 0; i < 6; i++)
  {
    DrawADigit(digs[5 - i]);
  }
}

void ParseDigits(time_t utc)
{
  time_t local = myTZ.toLocal(utc, &tcr);
  digs[0]->NewValue((SHOW_24HOUR ? hour(local) : hourFormat12(local)) / 10);
  digs[1]->NewValue((SHOW_24HOUR ? hour(local) : hourFormat12(local)) % 10);
  digs[2]->NewValue(minute(local) / 10);
  digs[3]->NewValue(minute(local) % 10);
  digs[4]->NewValue(second(local) / 10);
  digs[5]->NewValue(second(local) % 10);
  ispm = isPM(local);
}

/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;     // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; // buffer to hold incoming & outgoing packets

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  memset(packetBuffer, 0, NTP_PACKET_SIZE); // set all bytes in the buffer to 0

  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011; // LI, Version, Mode
  packetBuffer[1] = 0;          // Stratum, or type of clock
  packetBuffer[2] = 6;          // Polling Interval
  packetBuffer[3] = 0xEC;       // Peer Clock Precision

  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12] = 49;
  packetBuffer[13] = 0x4E;
  packetBuffer[14] = 49;
  packetBuffer[15] = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); // NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

time_t getNtpTime()
{
  IPAddress ntpServerIP; // NTP server's ip address

  while (Udp.parsePacket() > 0)
    ; // discard any previously received packets

  Serial.println("Transmit NTP Request");
  WiFi.hostByName(ntpServerName, ntpServerIP);
  Serial.print(ntpServerName);
  Serial.print(": ");
  Serial.println(ntpServerIP);

  sendNTPpacket(ntpServerIP);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500)
  {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE)
    {
      // Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE); // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 = (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];

      // NTP server responds within 50ms, so it does not account for the one second lag.
      // unsigned long secsSinceNTPRequest = (millis() - beginWait);
      // Debug("secsSinceNTPRequest=",secsSinceNTPRequest);
      unsigned long hackFactor = 1; // Without this the clock is 1 second behind (even when drawing without animation)

      return secsSince1900 - 2208988800UL + hackFactor;
    }
  }
  Serial.println("No NTP Response :-(");
  return timeNotSet; // return 0 if unable to get the time
}

void DrawDate(time_t utc)
{
  time_t local = myTZ.toLocal(utc, &tcr);
  int dd = day(local);
  int mth = month(local);
  int yr = year(local);

  if (dd != prevDay)
  {
    tft.setTextDatum(BC_DATUM);
    char buffer[50];
    if (NOT_US_DATE)
    {
        sprintf(buffer, "%02d/%02d/%d", dd, mth, yr);
    }
    else
    {
        // MURICA!!
        sprintf(buffer, "%02d/%02d/%d", mth, dd, yr);
    }

    tft.setTextSize(4);
    int h = tft.fontHeight();
    tft.fillRect(0, 210 - h, 320, h, TFT_BLACK);

    tft.drawString(buffer, 320 / 2, 210);

    int dow = weekday(local);
    String dayNames[] = {"", "Domenica", "Lunedi", "Martedi", "Mercoledi", "Giovedi", "Venerdi", "Sabato"};
    tft.setTextSize(4);
    tft.fillRect(0, 170 - h, 320, h, TFT_BLACK);
    tft.drawString(dayNames[dow], 320 / 2, 170);
  }
}

void SetupWiFi()
{
  while (WiFi.status() != WL_CONNECTED)
  {
    int numSSIDs = sizeof(credentials) / sizeof(credentials[0]);
    for (size_t i = 0; (i < numSSIDs); i++)
    {
      String ssid = credentials[i][0];
      String pass = credentials[i][1];

      Serial.print("Connecting to ");
      Serial.println();
      Serial.print("pass: ");
      Serial.println(credentials[i][1]);

      tft.fillScreen(clockBackgroundColor);
      tft.setTextFont(4);
      tft.setTextSize(1);
      tft.setTextDatum(MC_DATUM);
      tft.drawString("Connecting to", 320 / 2, 240 / 2 - 40);
      tft.drawString(ssid, 320 / 2, 240 / 2);

      WiFi.begin(ssid.c_str(), pass.c_str());

      int tries = 5 * 2;
      String dots = "";
      while (WiFi.status() != WL_CONNECTED && tries-- > 0)
      {
        delay(500);
        Serial.print(".");
        dots = dots + ".";
        tft.drawString(dots, 320 / 2, 240 / 2 + 20);
      }

      if (WiFi.status() == WL_CONNECTED)
        break;
    }
  }
}

void SetupNTP()
{
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);
  setSyncInterval(ntpSyncIntervalInSeconds);
  while (timeStatus() == timeNotSet)
  {
    Serial.print(".");
    delay(100);
  }
}

time_t prevDisplay = 0; // when the Digital clock was displayed

bool connected=0;
bool drawnClock=0;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/*******************************************************************
    TFT_eSPI button example for the ESP32 Cheap Yellow Display.

    https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display

    Written by Claus Näveke
    Github: https://github.com/TheNitek
 *******************************************************************/



// Make sure to copy the UserSetup.h file into the library as
// per the Github Instructions. The pins are defined in there.

// ----------------------------
// Standard Libraries
// ----------------------------

#include <SPI.h>

// ----------------------------
// Additional Libraries - each one of these will need to be installed.
// ----------------------------

#include <XPT2046_Bitbang.h>
// A library for interfacing with the touch screen
//
// Can be installed from the library manager (Search for "XPT2046 Slim")
// https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library

#include <TFT_eSPI.h>
// A library for interfacing with LCD displays
//
// Can be installed from the library manager (Search for "TFT_eSPI")
// https://github.com/Bodmer/TFT_eSPI


// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
// ----------------------------

XPT2046_Bitbang ts(XPT2046_MOSI, XPT2046_MISO, XPT2046_CLK, XPT2046_CS);

//TFT_eSPI tft = TFT_eSPI();

#define numKeys 3
TFT_eSPI_Button key[numKeys];





void drawButtons() {
  uint16_t bWidth = TFT_HEIGHT/3;
  uint16_t bHeight = TFT_WIDTH;
  // Generate buttons with different size X deltas
  for (int i = 0; i < numKeys; i++) {
    key[i].initButton(&tft,
                      bWidth * (i%3) + bWidth/2,
                      bHeight/2,
                      bWidth,
                      bHeight,
                      TFT_BLACK, // Outline
                      TFT_BLUE, // Fill
                      TFT_BLACK, // Text
                      "",
                      1);

    key[i].drawButton(false, String(i+1));
  }
}

bool drawnBut=0;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define nSchermate 2

/*-------- SETUP & LOOP ----------*/
void setup()
{
  Serial.begin(115200);

  //SetupCYD();
  //SetupWiFi();
  //SetupNTP();
  //SetupDigits();


  
  // Start the SPI for the touch screen and init the TS library
  ts.begin();
  //ts.setRotation(1);

  // Start the tft display and set it to black
  tft.init();
  tft.setRotation(1); //This is the display in landscape

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);
  //tft.setFreeFont(&FreeMono18pt7b);
  
}


int schermata=1;
void loop()
{
  time_t current = now();
  if(schermata!=0)
  {
    drawnClock=0;
  }
  if(schermata!=0)
  {
    drawnBut=0;
  }
  switch(schermata)
  {
    case 0:
      if(connected==0)
      {
        SetupWiFi();
        SetupNTP();
        connected=1;
      }

      if(drawnClock==0)
      {
        SetupCYD();
        SetupDigits();
        drawnClock=1;
      }
      if (current != prevDisplay)
      {
        prevDisplay = current;
        ParseDigits(prevDisplay);
        DrawDigitsAtOnce();    // Choose one: DrawDigitsWithoutAnimation(), DrawDigitsAtOnce(), DrawDigitsOneByOne()
        DrawDate(prevDisplay); // Draw Date and day of the week.
        DrawColons();
        DrawAmPm();
      }
      delay(100);
    break;

    case 1:
        {
          if(drawnBut==0)
          {
            drawButtons();
            drawnBut=1;
          }

          TouchPoint p = ts.getTouch();
          Serial.println(p.x);
          // Adjust press state of each key appropriately
          for (uint8_t b = 0; b < numKeys; b++) {
            if ((p.zRaw > 0) && key[b].contains(p.x, p.y)) {
              key[b].press(true);  // tell the button it is pressed
            } else {
              key[b].press(false);  // tell the button it is NOT pressed
            }
          }

          // Check if any key has changed state
          for (uint8_t b = 0; b < numKeys; b++) {
            // If button was just pressed, redraw inverted button
            if (key[b].justPressed()) {
              Serial.printf("Button %d pressed\n", b);
              key[b].drawButton(true, String(b+1));
            }

            // If button was just released, redraw normal color button

            if (key[b].justReleased()) {
              Serial.printf("Button %d released\n", b);
              Serial.println("Button " + (String)b + " released");
              key[b].drawButton(false, String(b+1));
              
              switch(b)
              {
                case 0://prev
                  schermata=(schermata-1)%nSchermate;
                break;

                case 1://sel

                break;

                case 2://next
                  schermata=(schermata+1)%nSchermate;
                break;
              }
            }
          }
          delay(50);
      }
    break;
  }
}