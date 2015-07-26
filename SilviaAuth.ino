#include <SPI.h>
#include <SD.h>
#include <Wire.h>
#include <RealTimeClockDS1307.h>
#include <MFRC522.h>
#include <sha1.h>

const int SD_SelectPin = 10;
const int RC522_SelectPin = 9;
const int RC522_ResetPin = 8;
const int relayPin = 3;
const int redPin = 4;
const int greenPin = 5;
const int bluePin = 6;
const int masterPin = 2;
const long interval = 45000;
//const long interval = 4000;
const int signalInterval = 4000;

const char masterFile[] = "master";
const char passwordFile[] = "password";
const char logFile[] = "access";

//#define DEBUG

MFRC522 mfrc522(RC522_SelectPin, RC522_ResetPin);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  SPI.begin(); // Init SPI bus.
  mfrc522.PCD_SetAntennaGain(mfrc522.RxGain_max); // Set receiver gain to max for good range.
  mfrc522.PCD_Init(); // Init MFRC522 card.

  pinMode(relayPin,OUTPUT);
  digitalWrite(relayPin,HIGH); // Set relay to closed
  
  pinMode(redPin,OUTPUT);
  pinMode(redPin+1,OUTPUT);
  pinMode(bluePin,OUTPUT);
  
  pinMode(SD_SelectPin,OUTPUT); // Fire up SD card
  if (!SD.begin(SD_SelectPin)) {
#ifdef DEBUG
    Serial.println("SD initialization failed!");
#endif
    digitalWrite(redPin,HIGH); //Indicate something is wrong

    
    return;
  }
  digitalWrite(redPin,LOW); // Set up LED interface
  digitalWrite(greenPin,LOW);
  digitalWrite(bluePin,LOW);
  
  pinMode(masterPin,INPUT_PULLUP);
#ifdef DEBUG
  if( digitalRead(masterPin); == LOW ) Serial.println("Master mode enabled.");
  Serial.println("Setup done.");
#endif
}

void loop() {
  unsigned long currentMillis, previousMillis;
  bool isMaster = false, isUser = false;
  File dateFile;
  byte hash[HASH_LENGTH];
  
  if(! scanCard(hash) ) return;

  if( digitalRead(masterPin) == LOW ) {
    printHash(hash,masterFile,"N"); // Write hash to passwordfile
    flashLed(bluePin);
    return; // Don't do anything else in MASTER mode
  }
  
  isUser = false;
  isMaster = IsHashInFile(hash,masterFile); // Check if we have a master
  if(!isMaster) isUser = IsHashInFile(hash,passwordFile); // Check if we have a user

  #ifdef DEBUG
  if(isMaster) Serial.println("Master card");
  if(isUser) Serial.println("User card");
  if(!isMaster && !isUser) Serial.println("Unknown card");
  #endif
  
  if( isMaster || isUser ) { // Authorised card has been scanned
    digitalWrite(relayPin,LOW); // Switch on relay
    digitalWrite(greenPin,HIGH); // Indicate that machine is operational
    
    printHash(hash,logFile,"A"); // Write to access log
    
    previousMillis = currentMillis = millis(); // Start timing
    while(currentMillis - previousMillis < interval) { // Keep counting for "interval"
      if( isMaster ) { // If we have master, we might have other things to do
        delay(signalInterval); // Wait a bit
        if(scanCard(hash)) { // In the meantime, look for a new card if we have a master
        
          #ifdef DEBUG
          Serial.println("New card detected");
          #endif
                        
          if(!IsHashInFile(hash,masterFile)) { // Check if master card was scanned twice accidentally.
            printHash(hash,passwordFile,"N"); // Write hash to passwordfile
            
            digitalWrite(greenPin,LOW); // Indicate success.
            flashLed(bluePin);
            break; // Finish our business, no coffee for master this time.
          }
        }
      }
      currentMillis = millis();
    }
    digitalWrite(relayPin,HIGH); // Switch off relay.
    digitalWrite(greenPin,LOW); // Switch off green light.
  } else {
    printHash(hash,logFile,"D"); // Write to access log
    flashLed(redPin); // Sorry, no coffee this time.
  }
}

void flashLed(int pin) {
  digitalWrite(pin,HIGH);
  delay(signalInterval);
  digitalWrite(pin,LOW);
}

void printHash(uint8_t* hash, const char* file, const char* remark) {
  File myFile;
  char dateStamp[] = "00-00-00 00:00:00x";

  RTC.readClock();
  RTC.getFormatted(dateStamp);
  
  myFile = SD.open(file,FILE_WRITE);
  for(int i=0; i<HASH_LENGTH; i++) {
    myFile.print("0123456789abcdef"[hash[i]>>4]);
    myFile.print("0123456789abcdef"[hash[i]&0xf]);
  }
  myFile.print("   ");
  myFile.print(dateStamp);
  myFile.print("   ");
  myFile.print(remark);
  myFile.println();
  myFile.close();
}

bool scanCard(byte *cardHash) {
  byte *hash;
  if( mfrc522.PICC_IsNewCardPresent() ) {      // Wait until card is presented.
    if( mfrc522.PICC_ReadCardSerial() ) {;        // Read card's serial number.
    
    Sha1.init();                                         // Initialise new sha1 hash
    for (byte i = 0; i < mfrc522.uid.size; i++)
      Sha1.write(mfrc522.uid.uidByte[i]);                // Add the serial number to the hash function
      
    hash = Sha1.result();
    for(byte i = 0; i < HASH_LENGTH;i++) cardHash[i] = hash[i];
    
    return true;
    }
  } else return false;
}

bool IsHashInFile(uint8_t* hash, const char* file) {
  File myFile;
  bool match;
  char a, b;
  
  myFile = SD.open(file,FILE_READ);
  while (myFile.available()) {
    match = true;
    for(int i=0; i<HASH_LENGTH; i++) {
      a=myFile.read();
      b=myFile.read();
      match = match && ( a == "0123456789abcdef"[hash[i]>>4] );
      match = match && ( b == "0123456789abcdef"[hash[i]&0xf] );
    }
    while (myFile.read() != 0x0a); // Go to the end of the line

    if(match) break; // Get out of here if we have match
  }
  myFile.close();
  return match;
}
