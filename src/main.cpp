#include "secrets.h"
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SPI.h>
#include <MFRC522.h>
#include <iostream>
#include <string>


#define RST_PIN         22           // Configurable, see typical pin layout above
#define SS_PIN          21          // Configurable, see typical pin layout above

MFRC522 mfrc522(SS_PIN, RST_PIN);   // Create MFRC522 instance.

MFRC522::MIFARE_Key key;

MFRC522::StatusCode status;
 
#define AWS_IOT_PUBLISH_TOPIC   "ESP32_01/pub"
#define AWS_IOT_SUBSCRIBE_TOPIC "ESP32_01/sub"

#define SIZE_BUFFER 18
#define MAX_SIZE_BLOCK 16
  
WiFiClientSecure net = WiFiClientSecure();
PubSubClient client(net);
 
#define LED_ON HIGH
#define LED_OFF LOW

constexpr uint8_t redLedPin = 14;
constexpr uint8_t greenLedPin = 26;
constexpr uint8_t blueLedPin = 33;
constexpr uint8_t buzzerPin = 32;


void publishMessage(std::string  username)
{
  StaticJsonDocument<200> doc;
  doc["username"] = username;
  char jsonBuffer[128];
  serializeJson(doc, jsonBuffer); // print to client
  client.publish(AWS_IOT_PUBLISH_TOPIC, jsonBuffer);
  Serial.println("Published to " + String(AWS_IOT_PUBLISH_TOPIC));
}
 
void messageHandler(char* topic, byte* payload, unsigned int length)
{
  Serial.print("incoming: ");
  Serial.println(topic);
 
  StaticJsonDocument<200> doc;
  deserializeJson(doc, payload);
  const char* message = doc["message"];
  Serial.println(message);
}

void connectAWS()
{

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
 
  Serial.println("Connecting to Wi-Fi");
 
  while (WiFi.status() != WL_CONNECTED)
  { 
    digitalWrite(redLedPin, LED_ON);
    digitalWrite(greenLedPin, LED_ON);
    delay(500);
    Serial.print(".");
    digitalWrite(redLedPin, LED_OFF);
    digitalWrite(greenLedPin, LED_OFF);
  }
   digitalWrite(redLedPin, LED_OFF);
   digitalWrite(greenLedPin, LED_OFF);
   digitalWrite(blueLedPin, LED_ON);
 
  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);
 
  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.setServer(AWS_IOT_ENDPOINT, 8883);
 
  // Create a message handler
  client.setCallback(messageHandler);
  client.setKeepAlive(10800);
 
  Serial.println("Connecting to AWS IOT");
 
  while (!client.connect(THINGNAME))
  {
    digitalWrite(blueLedPin, LED_ON);
    Serial.print(".");
    delay(100);
    digitalWrite(blueLedPin, LED_OFF);
  }
  digitalWrite(blueLedPin, LED_OFF);

  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    return;
  }
 
  // Subscribe to a topic
  client.subscribe(AWS_IOT_SUBSCRIBE_TOPIC);
 
  Serial.println("AWS IoT Connected!");
}
  

std::string cleanNonReadableBytes(const std::string& input) {
    std::string cleaned;
    for (char c : input) {
        if (c >= 32 && c <= 126) {
            cleaned += c;
        }
    }
    return cleaned;
}



void leituraDados(){
    mfrc522.PICC_DumpDetailsToSerial(&(mfrc522.uid)); //dump some details about the card

    for (byte i = 0; i < 6; i++) key.keyByte[i] = 0xFF;

    byte buffer[SIZE_BUFFER] = {0};

    byte block = 1;
    byte size = SIZE_BUFFER;

    status = mfrc522.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, block, &key,&(mfrc522.uid));
    if(status != MFRC522::STATUS_OK){
      Serial.print(F("Falha na leitura: "));
      Serial.println(mfrc522.GetStatusCodeName(status));
      return;
    }

    status = mfrc522.MIFARE_Read(block,buffer,&size);


    Serial.println(F("Dados: "));
    Serial.println(block);
    
    for (uint8_t i = 0; i < MAX_SIZE_BLOCK; i++ ){
        Serial.write(buffer[i]);
    }

    Serial.println("");

    std::string cleanedData = cleanNonReadableBytes(reinterpret_cast<const char*>(buffer));
    
    publishMessage(cleanedData); 
}

void setup() {
  pinMode(redLedPin, OUTPUT);
  pinMode(greenLedPin, OUTPUT);
  pinMode(blueLedPin, OUTPUT);
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  digitalWrite(redLedPin, LED_OFF);
  digitalWrite(greenLedPin, LED_OFF);
  digitalWrite(blueLedPin, LED_OFF);

  Serial.begin(115200);
  connectAWS();                                           // Initialize serial communications with the PC
  SPI.begin();                                                  // Init SPI bus
  mfrc522.PCD_Init();                                              // Init MFRC522 card
  Serial.println(F("Aproxime seu cartão:"));    //shows in serial that it is ready to read
}

//*****************************************************************************************//
void loop() {

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! mfrc522.PICC_IsNewCardPresent()) {
    return;
  }

  // Select one of the cards
  if ( ! mfrc522.PICC_ReadCardSerial()) {
    return;
  }

  Serial.println(F("**Card Detected:**"));

  //-------------------------------------------

  if (!client.connected())
  {
    Serial.println("AWS IoT Timeout!");
    digitalWrite(redLedPin, LED_ON);
    connectAWS();
    digitalWrite(redLedPin, LED_OFF);
  }

  digitalWrite(greenLedPin, LED_ON);
  digitalWrite(buzzerPin, HIGH);

  leituraDados();
  
  Serial.println(F("\n**End Reading**\n"));

  delay(200); //change value if you want to read cards faster
  digitalWrite(greenLedPin, LED_OFF);
  digitalWrite(buzzerPin, LOW);
  client.loop();
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();
}
//*****************************************************************************************//