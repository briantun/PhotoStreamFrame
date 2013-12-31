#include <ezLCDLib.h>
#include <SoftwareSerial.h>

ezLCD3 lcd;
SoftwareSerial m_serial = SoftwareSerial(10, 11);

void p_checkForImage()
{
    Serial.println(F("checking for image..."));
    
    // request 'A'
    m_serial.write(65);
    
    // wait until a response is found
    int delayCount = 0;
    
    while (!m_serial.available()) {
        
        delay(500);
        delayCount++;
        
        // check for timeout
        if (delayCount > 10) {
            Serial.println(F("timeout reached."));
            break;
        }
    }
    
    // start writing a file
    
    lcd.sendCommand("sdfsdf");
    
    int result = lcd.FSremove("tempimage.jpg");
    result = lcd.FSOpen("tempimage.jpg", "wh");
    
    uint32_t imageLength = 0;
    uint32_t countRead = 0;
    
    byte b[4];
    
    while (m_serial.available()) {
        
        char c = m_serial.read();
        
        // read image length
        if (countRead < 4) {
            b[countRead] = c;
        }
        else {
            FSWrite(&c, 1);
        }
        
        countRead++;
    }
    
    result = FSclose();
    
    // read data
    if (m_serial.available()) {
        
        char c = screenSerial.read();
        
        // 'A'
        if (c == 65) {
            Serial.println("request acknowledged.");
            p_transmitNextImage();
        }
    }
}

void setup()
{
    lcd.begin( EZM_BAUD_RATE );
    lcd.cls();
    
    m_serial.begin(57600);
}

void loop()
{
    p_checkForImage();
    
    delay(2000);
    
    lcd.picture(0,0,"jump.jpg");
    delay(100);
    
    lcd.picture(0,0,"alec.jpg");
    delay(100);
    
    lcd.picture(0,0,"didiball.jpg");
    delay(100);
}
