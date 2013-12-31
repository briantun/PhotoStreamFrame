
#include <Adafruit_CC3000.h>
#include <ccspi.h>
#include <SPI.h>
#include <SD.h>
#include <string.h>
#include "utility/debug.h"
#include <SoftwareSerial.h>

// Strings

static const int kFilePathLen = 32;
static const int kStrLen = 32;

// Image serving ----------------------------------------------------------------------------------------------------------

SoftwareSerial screenSerial(50, 51); // RX, TX

int _nextImageNumber = 0;
int _imageCount = 0;

// SD ---------------------------------------------------------------------------------------------------------------------

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
Adafruit_CC3000_Client www;

// ------------------------------------------------------------------------------------------------------------------------

#pragma mark - Support (Utility)

char *itoa(int val, int base) {
	
	static char buf[kStrLen] = {0};
	int i = sizeof(buf) - 2;
	
	for(; val > 0 ; --i, val /= base) {
        buf[i] = "0123456789abcdef"[val % base];
    }
    
	return &buf[i+1];
}

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
    }
    else {
        Serial.println("SD initialization done.");
    }
}

void p_deleteFileIfExists(char *name)
{
    boolean success = SD.remove(name);
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
    
    // TODO: add DHCP timeout
    while (!cc3000.checkDHCP())
    {
        delay(500);
        Serial.println(F("waiting on DHCP..."));
    }
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
    Serial.print("\n");
    
    www = cc3000.connectTCP(ip, 80);
    Serial.println(F("connected."));
}

void p_disconnect()
{
    Serial.println(F("\n\ndisconnecting..."));
    
    www.close();
    www = NULL;
    
    cc3000.disconnect();
    
    Serial.println(F("disconnected."));
}

#pragma mark - Support (file download)

boolean p_downloadFileNamed(char *name, boolean writeToConsole)
{
    boolean fileReady = false;
    File outFile;
    
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
        return false;
    }
    
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
            
            // read contents
            if (!inHeader) {
                
                // prep output file
                if (!fileReady) {
                    
                    Serial.println(F("prepping output file..."));
                    
                    p_deleteFileIfExists(name);
                    outFile = SD.open(name, FILE_WRITE);
                    
                    if (!outFile) {
                        Serial.println(F("unable to provisiong output file."));
                        return false;
                    }
                    
                    fileReady = true;
                }
                
                outFile.write(c);
                
                if (writeToConsole) {
                    Serial.print(c);
                }
            }
            
            lastRead = millis();
            
            // we've reached the end of the HTTP header
            if (inHeader && lastCharWasNewlineInHeader && c == 13) {
                inHeader = false;
                www.read();
            }
            
            // track when we see newlines in the header
            if (inHeader) {
                lastCharWasNewlineInHeader = (c == 10);
            }
        }
    }
    
    if (writeToConsole) {
        Serial.println(F("\n-------------------------------------\n"));
    }
    
    if (fileReady) {
        outFile.close();
    }
    
    return true;
}

#pragma mark - Support (Sync)

void p_processFileNamed(String fileName)
{
    char fileNameAsChar[kFilePathLen];
    fileName.toCharArray(fileNameAsChar, kFilePathLen);
    
    Serial.print(F("processing file: "));
    Serial.print(fileName);
    Serial.print("...\n");
    
    boolean shouldDownload = true;
    
    if (SD.exists(fileNameAsChar)) {
        
        Serial.println(F("file exists."));
        
        // check size of file
        File file = SD.open(fileNameAsChar, FILE_READ);
        boolean isEmpty = (file.size() == 0);
        file.close();
        
        if (isEmpty) {
            Serial.println(F("file existed, but was empty... deleting first."));
            SD.remove(fileNameAsChar);
        }
        else {
            // file exists and has contents
            shouldDownload = false;
        }
    }
    
    // download if needed
    if (shouldDownload) {
        Serial.println(F("getting file..."));
        p_downloadFileNamed(fileNameAsChar, false);
    }
}

boolean p_syncFiles()
{
    Serial.println(F("starting sync..."));
    
    // first try to refresh the file list
    p_downloadFileNamed("filelist.txt", true);
    
    // try to open it
    File listFile = SD.open("filelist.txt");
    
    // early exit if we can't open the file list
    if (!listFile) {
        Serial.println(F("can't open filelist.txt."));
        return false;
    }
    
    // iterate the files in the file list
    char c;
    String line = "";
    
    while (listFile.available()) {
        
        c = listFile.read();
        
        if (c != '\n') {
            line.concat(c);
        }
        else {
            
            // process line
            p_processFileNamed(line);
            
            // clear line
            line = "";
        }
    }
    
    // process the last item
    if (line.length() > 0) {
        p_processFileNamed(line);
    }
    
    listFile.close();
    
    Serial.println(F("sync done."));
    
    return true;
}

#pragma mark - Support (image serving)

void p_startServingImages()
{
    screenSerial.begin(57600);
}

void p_checkForImageRequest()
{
    Serial.println(F("checking for image req..."));
    
    if (screenSerial.available()) {
        
        char c = screenSerial.read();
        
        // 'A'
        if (c == 65) {
            Serial.println("request acknowledged.");
            p_transmitNextImage();
        }
    }
}

void p_transmitImageNamed(char *name)
{
    File entry = SD.open(name);
    
    if (!entry) {
        Serial.println(F("couldn't open file."));
        return;
    }
    
    uint32_t length = entry.size();
    
    // log
    
    Serial.print(F("sending: "));
    Serial.print(entry.name());
    Serial.print(F(" ("));
    Serial.print(length, 10);
    Serial.println(F(")..."));
    
    // transmit file size first
    
    screenSerial.write((const uint8_t*) &length, 4);
    
    // transmit file data next
    
    while (entry.available()) {
        screenSerial.write(entry.read());
    }
    
    entry.close();
    
    // log
    
    Serial.println(F("transmission done."));
}

void p_transmitNextImage()
{
    if (_imageCount == 0) {
        Serial.println(F("no images to send."));
        return;
    }
    
    // build a filename
    
    char *str = (char *) calloc(1, kFilePathLen);
    
    strcat(str, itoa(_nextImageNumber, 10));
    strcat(str, ".jpg");
    
    // send
    
    p_transmitImageNamed(str);
    
    // increment
    
    _nextImageNumber++;
    
    if (_nextImageNumber > (_imageCount - 1)) {
        _nextImageNumber = 0;
    }
}

void p_determineImageCount()
{
    _imageCount = 0;
    
    // try to open it
    File listFile = SD.open("filelist.txt");
    
    // early exit if we can't open the file list
    if (!listFile) {
        Serial.println(F("can't open filelist.txt."));
        return;
    }
    
    char c;
    
    if (listFile.available()) {
        _imageCount++;
    }
    
    while (listFile.available()) {
        
        c = listFile.read();
        
        if (c == '\n') {
            _imageCount++;
        }
    }
    
    listFile.close();
    
    Serial.print(F("image count: "));
    Serial.print(_imageCount, 10);
    Serial.println(F(""));
}

#pragma mark - Setup and main loop

void setup(void)
{
    Serial.begin(115200);
    
    p_initSD();
    //p_initWiFi();
    
    // Sync
    //p_connect();
    //p_syncFiles();
    //p_disconnect();
    
    // Serve up files
    p_determineImageCount();
    p_startServingImages();
}

void loop(void)
{
    p_checkForImageRequest();
    
    delay(2000);
}

