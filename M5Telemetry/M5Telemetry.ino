#include "M5Unified.h"
#include "SimpleWifi.h"
#include "PubSubClient.h"
#include <Preferences.h>
Preferences preferences;
M5Canvas canvas(&M5.Display); 
WiFiClient wifiClient; 
PubSubClient mqttClient(wifiClient); 

char username[50];
char password[50];
char ssid[50];

char * MOVEMENT_TOPIC = "elee2045sp25/kjohnsen/movement";
char * BATTERY_TOPIC = "elee2045sp25/kjohnsen/battery";
char * SOUND_TOPIC = "elee2045sp25/kjohnsen/sound";
char * RATE_TOPIC = "elee2045sp25/kjohnsen/rate";
uint32_t TRANSMIT_INTERVAL = 0; //note, set by preferences later

//this will hold our microphone samples
#define NUM_SAMPLES 400
int16_t samples[NUM_SAMPLES];  //1/40th of a second

unsigned long last_attempt_time = 0;
//this will compute a normalized (between 0 and 1) microphone volume from the samples
float getAverageMicVolume(){
  
    M5.Mic.record(samples, NUM_SAMPLES); //you have to call this a few times before there is any data in there
    M5.Mic.record(samples, NUM_SAMPLES);
    M5.Mic.record(samples, NUM_SAMPLES);
    M5.Mic.record(samples, NUM_SAMPLES);
    M5.Mic.record(samples, NUM_SAMPLES);

    int32_t average_volume = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
      int16_t sample = samples[i];
      average_volume += abs(sample);
    }
    average_volume /= NUM_SAMPLES;
    return average_volume/(float)(0x7FFF); //2^15 (highest possible value), so convert to 0..1
  
}

void mqttMessageCallback(char * topic, uint8_t* payload, unsigned int length){
  if(strcmp(topic,RATE_TOPIC) == 0 && length == sizeof(uint32_t)){
    memcpy(&TRANSMIT_INTERVAL,payload,length);
    preferences.putULong("interval",TRANSMIT_INTERVAL);
    canvas.clear();
    canvas.setCursor(10,10); canvas.print("Got Rate Message: "); canvas.println(TRANSMIT_INTERVAL);
    canvas.pushSprite(0,0);
  }
} 

void connectToMQTT() {
  canvas.clear();
  canvas.setCursor(10,10); canvas.println("connecting mqtt");
  if (!mqttClient.connect("","giiuser","giipassword")) { // must be unique username
    canvas.setCursor(10,30); canvas.print("mqtt failed, rc="); canvas.println(mqttClient.state());
  } else{
    canvas.setCursor(10,30); canvas.println("mqtt connected");
    mqttClient.subscribe(RATE_TOPIC);
  }
  canvas.pushSprite(0,0); 
}

void setup() {
  M5.begin();
  M5.Mic.begin();
  Serial.begin(115200);
  M5.Display.setRotation(1); 
  canvas.createSprite(M5.Display.width(),M5.Display.height()); 

  mqttClient.setServer("eduoracle.ugavel.com",1883);
  mqttClient.setCallback(mqttMessageCallback);

  preferences.begin("wifi", false); 
  preferences.getString("ssid",ssid,50);
  preferences.getString("username",username,50);
  preferences.getString("password",password,50);
  TRANSMIT_INTERVAL = preferences.getULong("interval",5000); //we use a small default value to be responsive at first
}

void loop() {
  //connect to WiFi and MQTT
  while(WiFi.status() != WL_CONNECTED || !mqttClient.connected()) {
    connectToWifi(ssid,username,password);
    connectToMQTT();
  }
  //get the sensor data
  float averageMicVolume = getAverageMicVolume(); 


  float accel_gyro[6];
  M5.Imu.getAccel(accel_gyro,accel_gyro+1,accel_gyro+2); 
  M5.Imu.getGyro(accel_gyro+3,accel_gyro+4,accel_gyro+5);
  int16_t battery = M5.Power.getBatteryVoltage();

  mqttClient.publish(MOVEMENT_TOPIC,(uint8_t*)accel_gyro,sizeof(float)*6);
  mqttClient.publish(BATTERY_TOPIC,(uint8_t*)&battery,sizeof(int16_t));
  mqttClient.publish(SOUND_TOPIC,(uint8_t*)&averageMicVolume,sizeof(float));

  Serial.print("Checking Transmit Interval "); Serial.println(TRANSMIT_INTERVAL);Serial.flush();
  if(TRANSMIT_INTERVAL < 15000){  //no sleeping state, just delay
    while(millis() - last_attempt_time < TRANSMIT_INTERVAL){
      M5.delay(1);
      mqttClient.loop(); //handle any messages
    }
    mqttClient.loop(); //just in case.  Could also use a do while loop to ensure this happens once
    last_attempt_time = millis(); //reset this timer
    return; 
  }
  Serial.print("Waiting for 5s "); Serial.println();Serial.flush();
  //we should sleep, but before we do, we need to wait at least 5 seconds
  unsigned long start_wait_time = millis();
  while(millis()-start_wait_time < 5000){
    M5.delay(1);
    mqttClient.loop();
  }
  
  unsigned long sleep_ms = 1; //by default, we don't sleep long, just enough to reset
  unsigned long curr_time = millis(); //we need to assign the time again, beause we already waited for up to 5 seconds
  unsigned long elapsed_time = curr_time - last_attempt_time; //this is how much time has already passwed
  if(elapsed_time < TRANSMIT_INTERVAL){ 
    
    sleep_ms = TRANSMIT_INTERVAL - elapsed_time; //but if there is time to kill, we need to accurately calculate it
    Serial.print("Will sleep for "); Serial.println(sleep_ms);Serial.flush();
  }
  
  gpio_hold_en((gpio_num_t) GPIO_NUM_4);
  gpio_deep_sleep_hold_en();
  M5.Power.deepSleep(sleep_ms*1000); //deep sleep is in uS
  
  
}
