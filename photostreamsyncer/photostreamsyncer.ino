
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>
#include "utility/debug.h"

// SD ---------------------------------------------------------------------------------------------------------------------

File root;

// WiFi -------------------------------------------------------------------------------------------------------------------

#define ADAFRUIT_CC3000_IRQ   3  // MUST be an interrupt pin!
#define ADAFRUIT_CC3000_VBAT  5
#define ADAFRUIT_CC3000_CS    10

// Use hardware SPI for the remaining pins
// On an UNO, SCK = 13, MISO = 12, and MOSI = 11
Adafruit_CC3000 cc3000 = Adafruit_CC3000(ADAFRUIT_CC3000_CS, ADAFRUIT_CC3000_IRQ, ADAFRUIT_CC3000_VBAT, SPI_CLOCK_DIV2); // you can change this clock speed

#define WLAN_SSID       "bt iphone5S"           // cannot be longer than 32 characters!
#define WLAN_PASS       "stuffage9"
#define WLAN_SECURITY   WLAN_SEC_WPA2
#define IDLE_TIMEOUT_MS  3000      // Amount of time to wait (in milliseconds) with no data
// received before closing the connection.  If you know the server
// you're accessing is quick to respond, you can reduce this value.

#define WEBSITE      "fbits-photostream.s3.amazonaws.com"
#define WEBPAGE      "/filelist.txt"

uint32_t ip;

// ------------------------------------------------------------------------------------------------------------------------

#pragma mark - Support (SD)

void p_initSD()
{
    // On the Ethernet Shield, CS is pin 4. It's set as an output by default.
    // Note that even if it's not used as the CS pin, the hardware SS pin
    // (10 on most Arduino boards, 53 on the Mega) must be left as an output
    // or the SD library functions will not work.
    pinMode(10, OUTPUT);
    
    if (!SD.begin(4)) {
        Serial.println("SD initialization failed!");
        return;
    }
    root = SD.open("/");
    Serial.println("SD initialization done.");
}

void p_deleteFileIfExists(char *name)
{
    
}

#pragma mark - Support (Comm)

void p_initWiFi()
{
    Serial.println(F("\nInitializing..."));
    if (!cc3000.begin())
    {
        Serial.println(F("Couldn't begin()! Check your wiring?"));
        while(1);
    }
    
    if (!cc3000.connectToAP(WLAN_SSID, WLAN_PASS, WLAN_SECURITY)) {
        Serial.println(F("Failed!"));
        while(1);
    }
    
    Serial.println(F("Connected!"));
    Serial.println(F("Request DHCP"));
    while (!cc3000.checkDHCP())
    {
        delay(100); // ToDo: Insert a DHCP timeout!
    }
}

void p_downloadFileNamed(char *name, boolean writeToConsole)
{
    Adafruit_CC3000_Client www = cc3000.connectTCP(ip, 80);
    
    // issue request
    if (www.connected()) {
        www.fastrprint(F("GET /"));
        www.fastrprint(name);
        www.fastrprint(F(" HTTP/1.1\r\n"));
        www.fastrprint(F("Host: "));
        www.fastrprint(WEBSITE);
        www.fastrprint(F("\r\n"));
        www.fastrprint(F("\r\n"));
        www.println();
    }
    else {
        Serial.println(F("Connection failed"));
        return;
    }
    
    // prepare file for writing
    p_deleteFileIfExists(name);
    File myFile = SD.open(name, FILE_WRITE);
    
    // read data until either the connection is closed, or the idle timeout is reached
    unsigned long lastRead = millis();
    
    if (writeToConsole) {
        Serial.println(F("-------------------------------------\n"));
    }
    
    // prep a simple state machine
    boolean inHeader = true;
    boolean lastCharWasNewlineInHeader = false;
    
    while (www.connected() && (millis() - lastRead < IDLE_TIMEOUT_MS)) {
        
        while (www.available()) {
            
            char c = www.read();
            
            // if we're in the header, and we see 2 newlines, that means we've reached the body
            if (inHeader && lastCharWasNewlineInHeader && c == '\n') {
                inHeader = false;
            }
            
            if (!inHeader) {
                
                myFile.write(c);
                
                if (writeToConsole) {
                    Serial.print(c);
                }
            }
            
            lastRead = millis();
            
            // if we're in the header, remember when we see a newline
            if (inHeader && c == '\n') {
                lastCharWasNewlineInHeader = true;
            }
        }
    }
    
    if (writeToConsole) {
        Serial.println(F("\n-------------------------------------\n"));
    }
    
    myFile.close();
    www.close();
}

void p_connect()
{
    ip = 0;
    
    // Try looking up the website's IP address
    Serial.print(WEBSITE);
    Serial.print(F(" -> "));
    while (ip == 0) {
        if (!cc3000.getHostByName(WEBSITE, &ip)) {
            Serial.println(F("Couldn't resolve!"));
        }
        delay(500);
    }
    
    cc3000.printIPdotsRev(ip);
}

void p_disconnect()
{
    Serial.println(F("\n\ndisconnecting..."));
    cc3000.disconnect();
}

#pragma mark - Setup and main loop

void setup(void)
{
    Serial.begin(115200);
    
    p_initSD();
    p_initWiFi();
    p_connect();
    p_downloadFileNamed("filelist.txt", true);
    p_disconnect();
}

void loop(void)
{
    delay(1000);
    Serial.println("looping...");
}

