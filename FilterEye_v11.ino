#include <Wire.h>
#include "Adafruit_TCS34725.h"
#include <Adafruit_DPS310.h>

#include "SafeString.h"
#include "BLEPeripheral.h"

//TO DO:
//CHECK DEVICE MANAGER BAUD RATE
//MAKE SURE BLE_ADVERT_MS > DELAY_TIME

//add calibration
//pressure offset
//integrate button
//check 3d print

BLEPeripheral ble;
const size_t MAX_DATA_LEN = 26;
cSF(sfData, MAX_DATA_LEN); // Local name and formatted measurements SafeString will limit data to this length. Excess data is dropped NOT truncated see SafeString docs/tutorials
unsigned short ADVERT_INTERVAL = 750; // default it 500, 750ms gives 13 advert packets in the 10 secs

lp_timer BLE_Timer;
//const unsigned long BLE_ADVERT_MS = 15ul * 1000; // advertise for 15sec at 750ms intervals i.e. ~13 advert packets

Adafruit_TCS34725 tcs = Adafruit_TCS34725(TCS34725_INTEGRATIONTIME_614MS, TCS34725_GAIN_1X);

Adafruit_DPS310 dps; //address 0x77, remote
Adafruit_DPS310 dps2; //address 0x76, local

#define vccPin 8
#define buttonPin 11
#define buzzerPin 12
//#define DEBUG

lp_timer sleepTimer;
const unsigned long DELAY_TIME = 20000;
//const unsigned long BEEP_DELAY_TIME = 30000;

#define pressureThreshold 2.05
#define lightThreshold -10
#define ambientThreshold 200

const unsigned long buzzTime = 50;

int programmingMode;
bool changeFlag = false;
int lux;
int luxOffset = 0;
float pressureOffset = 0.00;

void beep(int times, int short_wait, int long_wait){
  for (int i=0; i<times; i++){
    digitalWrite(buzzerPin, HIGH);
    delay(short_wait);
    digitalWrite(buzzerPin, LOW);

    delay(long_wait);
  }
}

void turn_off(){
  while (1){
    tcs.setInterrupt(true); //turn off built-in LED
    digitalWrite(buzzerPin, LOW);
  }
}

void setAdvertData(int lux, float diff, bool alarmState){
  //update bluetooth --------------------------------------
  sfData = "QI";
  sfData += '_';
  sfData.print(lux); // only one decimal place
  sfData += '*';
  sfData.print(diff,2); // only one decimal place
  sfData += '@';

  if (alarmState){
    sfData += "T";
  } else{
    sfData += "F";
  }
  
  ble.setAdvertisedName(sfData.c_str());
  
#ifdef DEBUG
  Serial.print("set data to ");
  Serial.println(sfData);
#endif
}



/*void stopAdvert() {
  ble.end();
#ifdef DEBUG
  Serial.print("stop ble "); Serial.println(millis());
#endif
}*/



void handleSleepTimer(){
  //programmingMode 1 = calibration cycle
  //programmingMode 2 = regular cycle
  //programmingMode 3 = beeping cycle

  //init sensor variables
  sensors_event_t temp_event, pressure_event;
  sensors_event_t temp_event2, pressure_event2;
  uint16_t r, g, b, c, colorTemp, lux;
  
  if (changeFlag){
    programmingMode = 3;
  }
  else if (digitalRead(buttonPin) == HIGH){
    programmingMode = 1;
  }
  else{
    programmingMode = 2;
  }
  
#ifdef DEBUG
  Serial.println("WAKE");
  Serial.print("programmingMode: ");
  Serial.println(programmingMode);
#endif

  if (programmingMode == 1){ //calibration mode
    //beep
    beep(2, 250, 250);

    delay(4500);

    //init vcc, sda, scl
    digitalWrite(vccPin, HIGH);
    Wire.begin();
    delay(500);

    //init pressure (dps)-------------------------------------------------
     if (! dps.begin_I2C(0x76)) {             // Can pass in I2C address here
      //turn off vcc & wires
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return;
    }
    if (! dps2.begin_I2C(0x77)) {             // Can pass in I2C address here
      //turn off vcc & wires
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return;
    }
    
    dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
  
    dps2.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps2.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);

    //init light sensor----------------------------------------------
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    tcs.setInterrupt(true);
    
    //---------------------------------------------------------------

    //pressure sensors  --------------------------------------------------
    
    while (!dps.temperatureAvailable() || !dps.pressureAvailable() || !dps2.temperatureAvailable() || !dps2.pressureAvailable()) {
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return;
    }
    
    //take measurements
    dps.getEvents(&temp_event, &pressure_event);
    dps2.getEvents(&temp_event2, &pressure_event2);
  
#ifdef DEBUG
  Serial.println();
  Serial.println("CALIBRATION -----------------");
  Serial.print(pressure_event.pressure);
  Serial.print(" hPa (0x76), "); 

  Serial.print(pressure_event2.pressure);
  Serial.println(" hPa (0x77), "); 

  Serial.print(F("PRESSURE - CALIBRATION OFFSET: "));//0x77 sensor minus 0x76 sensor
  Serial.print(pressure_event2.pressure - pressure_event.pressure);
  Serial.println(" hPa (0x77-0x76)"); 
#endif

    pressureOffset = pressure_event2.pressure - pressure_event.pressure;

    //light sensors  --------------------------------------------------
    tcs.setInterrupt(false);
    delay(50);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    lux = tcs.calculateLux(r, g, b); //calculate ambient light
    tcs.setInterrupt(true);
  
 #ifdef DEBUG
     Serial.print("LUX - CALIBRATION OFFSET: "); 
     Serial.println(lux, DEC);
 #endif

    luxOffset = lux;
    programmingMode = 1;
    
    beep(2, 250, 250);

    //turn off vcc & wires
    digitalWrite(vccPin, LOW);
    Wire.end();
    pinMode(9, INPUT);
    pinMode(10, INPUT);
    
  }
  
  if (programmingMode == 2){ //regular cycle

    //init vcc, sda, scl
    digitalWrite(vccPin, HIGH);
    Wire.begin();
    delay(500);

    //init pressure (dps)-------------------------------------------------
     if (! dps.begin_I2C(0x76)) {             // Can pass in I2C address here
      //turn off vcc & wires
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return;
    }
    if (! dps2.begin_I2C(0x77)) {             // Can pass in I2C address here
      //turn off vcc & wires
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return;
    }
    
    dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
  
    dps2.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
    dps2.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);

    //init light sensor----------------------------------------------
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    if (tcs.begin()) { 
    } else {
      while (1);
    }
    tcs.setInterrupt(true);
    
    //---------------------------------------------------------------

    //pressure sensors  --------------------------------------------------
    while (!dps.pressureAvailable() || !dps2.pressureAvailable()) {
      digitalWrite(vccPin, LOW);
      Wire.end();
      pinMode(9, INPUT);
      pinMode(10, INPUT);
      return; // wait until there's something to read from *both* sensors
    }
    
    //take measurements
    dps.getEvents(&temp_event, &pressure_event);
    dps2.getEvents(&temp_event2, &pressure_event2);
  
#ifdef DEBUG
  Serial.print(pressure_event.pressure);
  Serial.print(" hPa (0x76), "); 

  Serial.print(pressure_event2.pressure);
  Serial.print(" hPa (0x77), "); 

  Serial.print(F("diff = "));//0x77 sensor minus 0x76 sensor
  Serial.print((pressure_event2.pressure - pressure_event.pressure) - pressureOffset);
  Serial.println(" hPa (0x77-0x76)"); 
#endif

    //light sensors  --------------------------------------------------
    /*tcs.setInterrupt(true);
    tcs.getRawData(&r, &g, &b, &c);
    lux = tcs.calculateLux(r, g, b); //calculate ambient light
  
  #ifdef DEBUG
    Serial.print("Ambient Lux: "); Serial.println(lux, DEC);
  #endif
    
    if (lux < ambientThreshold){*/
    
    tcs.setInterrupt(false); //inverted, false = on; true = off
    delay(50);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    tcs.getRawData(&r, &g, &b, &c);
    lux = tcs.calculateLux(r, g, b);
    tcs.setInterrupt(true);
  
  #ifdef DEBUG
      Serial.print("Lux Change: "); Serial.println(lux - luxOffset, DEC);
      Serial.print("raw r: "); Serial.println(r);
      Serial.print("raw g: "); Serial.println(g);
      Serial.print("raw b: "); Serial.println(b);
      Serial.print("raw c: "); Serial.println(c);
  #endif
    //}
  

    //turn off vcc & wires
    digitalWrite(vccPin, LOW);
    Wire.end();
    pinMode(9, INPUT);
    pinMode(10, INPUT);
    
    
    //buzzer --------------------------------------------------
    if ((lux - luxOffset <= lightThreshold) && (pressure_event2.pressure - pressure_event.pressure - pressureOffset >= pressureThreshold)){
      digitalWrite(buzzerPin, HIGH);
      delay(500);
      digitalWrite(buzzerPin, LOW);
      changeFlag = true;
      setAdvertData((lux - luxOffset), (pressure_event2.pressure - pressure_event.pressure - pressureOffset), true);
      //sleepTimer.startTimer(BEEP_DELAY_TIME, handleSleepTimer);
    } else {
      setAdvertData((lux - luxOffset), (pressure_event2.pressure - pressure_event.pressure - pressureOffset), false);  // pick up latest values
    }
    delay(500);
    ble.begin();
  
  #ifdef DEBUG
    Serial.print("start ble "); Serial.println(millis());
  #endif
  
    //BLE_Timer.startDelay(BLE_ADVERT_MS, stopAdvert); // stop in 15sec
  }

  if (programmingMode == 3){
    beep(1, buzzTime, 0);

    if (digitalRead(buttonPin) == HIGH){

#ifdef DEBUG
  Serial.println("BUZZER EXIT");
#endif

      changeFlag = false;
      //sleepTimer.startTimer(DELAY_TIME, handleSleepTimer);
      beep(3, 50, 2000);
    }
    
  }
  
}




void setup() {
  // put your setup code here, to run once:

  //init sleep timer -------------------------------------------
  sleepTimer.startTimer(DELAY_TIME, handleSleepTimer);
  
  //init buzzer -------------------------------------------
  pinMode(buzzerPin, OUTPUT_D0H1);
  pinMode(vccPin, OUTPUT_D0H1);
  digitalWrite(vccPin, HIGH);
  pinMode(buttonPin, INPUT);
  
  //init serial ------------------------------------------- 

#ifdef DEBUG
  //Serial.setPins(30, 29);
  Serial.begin(115200); //init serial monitor
  //while (!Serial) delay(10);
#endif

  delay(1000);

#ifdef DEBUG
  Serial.println();
  /*for (int i = 10; i > 0; i--) {
    Serial.print(' '); Serial.print(i);
    delay(500);
  }*/
  Serial.println();
#endif

  // set advertised name
  ble.setConnectable(false);
  ble.setTxPower(+4);
  ble.setAdvertisingInterval(ADVERT_INTERVAL);

  handleSleepTimer(); // sets name and measurement data
  
  ble.begin();
  
#ifdef DEBUG
  Serial.println(" BLE started");
  Serial.println("setup() finished");
#endif
}

void loop() {
#ifdef DEBUG
  Serial.println("SLEEP");
  delay(100);
#endif
  sleep(); //sleep while waiting for timer to trigger
}
