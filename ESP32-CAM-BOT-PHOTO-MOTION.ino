// LV 7/Dic/23 Taken from https://randomnerdtutorials.com/telegram-esp32-cam-photo-arduino/
// LV Added a better flash and toggle of LED 33

/*
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/telegram-esp32-cam-photo-arduino/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*/

// Code partially taken from https://microcontrollerslab.com/hc-sr04-ultrasonic-esp32-tutorial/


#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

const char* ssid = "your SSID";
const char* password = "your passwd";

// Initialize Telegram BOT
String BOTtoken = "your bot token from Telegram";  // your Bot Token (Get from Botfather)

// Use @myidbot to find out the chat ID of an individual or a group
// Also note that you need to click "start" on a bot before it can
// message you
String CHAT_ID = "your id from Telegram";

bool sendPhoto = false;
bool pirEnable = false;
bool pirAlert = false;
bool pirAlertPrevious = LOW;
bool pirAlertCurrent = LOW;

WiFiClientSecure clientTCP;
UniversalTelegramBot bot(BOTtoken, clientTCP);

#define FLASH_LED_PIN 4
#define PIR_PIN  12 // GPIO12 pin connected to OUTPUT pin of sensor
bool flashState = false;

//LV 12/12/23
#define LED_PIN 33
bool ledState = HIGH;

//Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

//CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22


void configInitCamera(){
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_LATEST;

  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;  //0-63 lower number means higher quality
    config.fb_count = 1;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    delay(1000);
    ESP.restart();
  }
}

void handleNewMessages(int numNewMessages) {
  Serial.print("Handle New Messages: ");
  Serial.println(numNewMessages);

  for (int i = 0; i < numNewMessages; i++) {
    String chat_id = String(bot.messages[i].chat_id);
    //if (chat_id != CHAT_ID){
    //  bot.sendMessage(chat_id, "Unauthorized user", "");
    //  continue;
    //}
    
    // Print the received message
    String text = bot.messages[i].text;
    Serial.println(text);
    
    String from_name = bot.messages[i].from_name;
    if (text == "/start") {
      String welcome = "Welcome , " + from_name + "\n";
      welcome += "Use the following commands to interact with the ESP32-CAM \n";
      welcome += "/photo : takes a new photo\n";
      //LV 12/12/23
      welcome += "/led : toggles LED on back \n";
      welcome += "/flash : toggles flash  \n";
      welcome += "/pir : toggles PIR Detector \n";
      bot.sendMessage(CHAT_ID, welcome, "");
    }
     
    if (text == "/flash") {
      flashState = !flashState;
      bot.sendMessage(CHAT_ID, "Flash set to "+ String(flashState), "");
    }
     
    //LV 12/12/23
      if (text == "/led") {
      ledState = !ledState;
      digitalWrite(LED_PIN, ledState);
      Serial.println("Change 33 LED state");
      
      bot.sendMessage(CHAT_ID, "LED 33 set to "+ String(!ledState), "");
    }
    if (text == "/photo") {
      
      //Serial.println("New photo request");
      bot.sendMessage(CHAT_ID, "Photo requested ...", "");
      sendPhoto = true;
    }

     if (text == "/pir") {
      pirEnable = !pirEnable;
      bot.sendMessage(CHAT_ID, "Changed PIR enable to "+String(pirEnable), "");
    }
  }
}



String sendPhotoTelegram() {
  const char* myDomain = "api.telegram.org";
  String getAll = "";
  String getBody = "";

//LV 12/12/23
ledcSetup(15, 5000, 8);
ledcAttachPin(FLASH_LED_PIN, 15);
if (flashState){
  ledcWrite(15, 255);
} 



  //Dispose first picture because of bad quality
  camera_fb_t * fb = NULL;
  fb = esp_camera_fb_get();
  esp_camera_fb_return(fb); // dispose the buffered image
  


  // Take a new photo
  fb = NULL;  
  fb = esp_camera_fb_get();  
  if(!fb) {
    Serial.println("Camera capture failed");
    delay(1000);
    ESP.restart();
    return "Camera capture failed";
  }  
  //LV 12/12/23
  ledcWrite(15, 0);


  Serial.println("Connect to " + String(myDomain));


  if (clientTCP.connect(myDomain, 443)) {
    Serial.println("Connection successful");
    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"chat_id\"; \r\n\r\n" + CHAT_ID + "\r\n--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"photo\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";

    size_t imageLen = fb->len;
    size_t extraLen = head.length() + tail.length();
    size_t totalLen = imageLen + extraLen;
  
    clientTCP.println("POST /bot"+BOTtoken+"/sendPhoto HTTP/1.1");
    clientTCP.println("Host: " + String(myDomain));
    clientTCP.println("Content-Length: " + String(totalLen));
    clientTCP.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    clientTCP.println();
    clientTCP.print(head);
  
    uint8_t *fbBuf = fb->buf;
    size_t fbLen = fb->len;
    for (size_t n=0;n<fbLen;n=n+1024) {
      if (n+1024<fbLen) {
        clientTCP.write(fbBuf, 1024);
        fbBuf += 1024;
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        clientTCP.write(fbBuf, remainder);
      }
    }  
    
    clientTCP.print(tail);
    
    esp_camera_fb_return(fb);
    
    int waitTime = 10000;   // timeout 10 seconds
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + waitTime) > millis()){
      Serial.print(".");
      delay(100);      
      while (clientTCP.available()) {
        char c = clientTCP.read();
        if (state==true) getBody += String(c);        
        if (c == '\n') {
          if (getAll.length()==0) state=true; 
          getAll = "";
        } 
        else if (c != '\r')
          getAll += String(c);
        startTimer = millis();
      }
      if (getBody.length()>0) break;
    }
    clientTCP.stop();
    Serial.println(getBody);
  }
  else {
    getBody="Connected to api.telegram.org failed.";
    Serial.println("Connected to api.telegram.org failed.");
  }
  return getBody;
}

void setup(){

//LV 18/12/23
    pinMode(PIR_PIN , INPUT);
 
    String welcome ="Camera is ready ..\n";
       welcome += "Use the following commands to interact with the ESP32-CAM \n";
      welcome += "/start : to display this message \n";
      welcome += "/photo : takes a new photo \n";
      //LV 12/12/23
      welcome += "/led : toggles LED \n";
      welcome += "/flash : toggles flash  \n";
      welcome += "/pir : toggles PIR Detector \n";

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  // Init Serial Monitor
  Serial.begin(115200);

 
//LV 12/12/23
  // Set LED 33 as output
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, ledState);


  // Config and init the camera
  configInitCamera();

  // Connect to Wi-Fi
  WiFi.mode(WIFI_STA);
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  clientTCP.setCACert(TELEGRAM_CERTIFICATE_ROOT); // Add root certificate for api.telegram.org
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("ESP32-CAM IP Address: ");
  Serial.println(WiFi.localIP()); 

  bot.sendMessage(CHAT_ID, welcome, "");
}

void loop() {

pirAlertPrevious = pirAlertCurrent; // store old state
  pirAlertCurrent = digitalRead(PIR_PIN);   // read new state

  if (pirAlertPrevious == LOW && pirAlertCurrent == HIGH) {   // pin state change: LOW -> HIGH
    Serial.println("Motion detected!");

    if (pirEnable){
    bot.sendMessage(CHAT_ID, "Motion detected ...", "");
    sendPhoto = true;}
  }
  else
  if (pirAlertPrevious == HIGH && pirAlertCurrent == LOW) {   // pin state change: HIGH -> LOW
    Serial.println("Motion stopped!");
    
  }


  if (sendPhoto) {

    //Serial.println("Preparing photo");
    bot.sendMessage(CHAT_ID, "Preparing photo ...", "");
    sendPhotoTelegram(); 
    
    sendPhoto = false; 
  }
  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }
}
