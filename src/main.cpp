// INCLUDES ///////////////////////////
#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <MPU6050_tockn.h>
#include <AHT10.h>
#include <LSM303.h>

// DEFINITIONS ////////////////////////
#define SERIAL_BAUD 115200
#define SAMPLES 100

// GLOBAL VARS ////////////////////////
TFT_eSPI tft = TFT_eSPI();
MPU6050 mpu6050(Wire);
AHT10 aht10(AHT10_ADDRESS_0X38);
LSM303 compass;
LSM303::vector<int16_t> running_min = {32767, 32767, 32767}, running_max = {-32768, -32768, -32768};

int temp=0, humidity=0, g=1, heading=0; // temp x 10. Divide by 10 to get correct value, g x 10
const char* dirs[] = {"N", "NW", "W", "SW", "S", "SE", "E", "NE"};
float gX, gY, gZ;
long timer1 = 0, timer2 = 250, timer3 = 600;
float histX[SAMPLES], histY[SAMPLES], histZ[SAMPLES];
char str[40];
char report[80];

// SETUP //////////////////////////////
void setup() {
  Serial.begin(SERIAL_BAUD);
  tft.begin();
  tft.setRotation(1); // Rotate the screen 90 degress
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);

  // Draw Splash Screen
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.setTextSize(6);
  tft.drawString("CarDash", 40, 50);
  tft.setTextSize(2);
  delay(100);
  tft.drawString("Checking peripherals", 40, 120);
  Wire1.begin();
  tft.drawString("AHT10...", 20, 150);
  int i=4;
  while (i>0) {
    if (aht10.begin() == true) break;
    Serial.println(F("AHT10 not connected or fail to load calibration coefficient")); //(F()) save string to flash & keeps dynamic memory free
    i--;
    delay(1000);
  }
  if (i>0) {
    tft.setTextColor(TFT_GREEN,TFT_BLACK);
    tft.drawString("OK", 200, 150);
  } else {
    tft.setTextColor(TFT_RED,TFT_BLACK);
    tft.drawString("BAD", 200, 150);
  }
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("Compass...", 20, 170);
  Wire.begin();
  delay(300);
  compass.init();
  compass.enableDefault();
  // min: {  -506,   -545,    -21}    max: {  +408,   +383,    +16}
  compass.m_min = (LSM303::vector<int16_t>){-506, -545, -21};
  compass.m_max = (LSM303::vector<int16_t>){+408, +383, +16};
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.drawString("OK", 200, 170);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.drawString("MPU6050...", 20, 190);
  mpu6050.begin();
  delay(300);
  tft.setTextColor(TFT_GREEN,TFT_BLACK);
  tft.drawString("OK", 200, 190);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  delay(1000);
  tft.fillScreen(TFT_BLACK);

  // Draw static UI
  // Temp
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  tft.fillRoundRect(5,5,100,50,4,TFT_RED);
  tft.drawString("C", 90, 10);
  tft.drawCircle(85,8,2,TFT_WHITE);
  tft.setTextColor(TFT_WHITE, TFT_RED);
  // Humidity
  tft.fillRoundRect(110,5,115,50,4,TFT_BLUE);
  tft.setTextColor(TFT_WHITE, TFT_BLUE);
  tft.drawString("%", 210, 10);
  // G
  tft.fillRoundRect(230,5,85,50,4,TFT_GREEN);
  tft.setTextColor(TFT_BLACK, TFT_GREEN);
  tft.drawString("G", 300, 10);
  tft.setTextSize(3);
  // Compass
  tft.fillRect(0,60,320,30, TFT_ORANGE);
  tft.fillRect(158,85,4,20,TFT_RED);
}

// FUNCTIONS //////////////////////////

void updateTemp(int newTemp) {
  if (newTemp != temp) {
    sprintf(str, "%d.%d", temp/10, temp%10);
    tft.setTextColor(TFT_RED, TFT_RED); // over-write old 
    tft.drawString(str, 15, 25);
    temp = newTemp;
    sprintf(str, "%d.%d", temp/10, temp%10);
    tft.setTextColor(TFT_WHITE, TFT_RED); // over-write old 
    tft.drawString(str, 15, 25);
  }
}

void updateHumidity(int newHumidity) {
  if (newHumidity != humidity) {
    sprintf(str, "%d.%d", humidity/10, humidity%10);
    tft.setTextColor(TFT_BLUE, TFT_BLUE); // over-write old 
    tft.drawString(str, 135, 25);
    humidity = newHumidity;
    sprintf(str, "%d.%d", humidity/10, humidity%10);
    tft.setTextColor(TFT_WHITE, TFT_BLUE); // over-write old 
    tft.drawString(str, 135, 25);
  }
}

void updateG(int newG) {
  if (newG < g) {
    newG = g-1;
  }
  if (newG != g) {
    sprintf(str, "%d.%d", g/10, g%10);
    tft.setTextColor(TFT_GREEN, TFT_GREEN); // over-write old 
    tft.drawString(str, 245, 25);
    g = newG;
    sprintf(str, "%d.%d", g/10, g%10);
    tft.setTextColor(TFT_BLACK, TFT_GREEN); // over-write old 
    tft.drawString(str, 245, 25);
  }
}

void updateHeading(int newHeading) {
  newHeading -= 90; // Calibration for orientation
  if (abs(newHeading-heading)>2) {
    tft.fillRect(0,60,320,30, TFT_ORANGE);
    tft.setTextColor(TFT_BLACK, TFT_ORANGE);
    heading = newHeading;//(newHeading-heading)/abs((newHeading-heading)); // increment by one degree at a time
    for (int i=0; i<8; i++) {
      tft.drawString(dirs[i], 150+800*heading/360+i*100, 63);
      tft.drawString(dirs[i], 150-800+800*heading/360+i*100, 63);
      tft.drawString(dirs[i], 150-1600+800*heading/360+i*100, 63);
    }
  }
}

// MAIN LOOP //////////////////////////
void loop() {
  mpu6050.update();
  
  if(millis() - timer1 > 10) {
       
    gX = mpu6050.getAccX();
    gY = mpu6050.getAccY();
    gZ = mpu6050.getAccZ();
    
    for(int i=0; i<SAMPLES-2; i++) {
      tft.drawLine(10+i*3, 120-histX[i]*20, 10+(i+1)*3, 120-histX[i+1]*20, TFT_BLACK);
      tft.drawLine(10+i*3, 170-histY[i]*20, 10+(i+1)*3, 170-histY[i+1]*20, TFT_BLACK);
      tft.drawLine(10+i*3, 220-histZ[i]*20, 10+(i+1)*3, 220-histZ[i+1]*20, TFT_BLACK);
      histX[i] = histX[i+1];
      histY[i] = histY[i+1];
      histZ[i] = histZ[i+1];
      tft.drawLine(10+i*3, 120-histX[i]*20, 10+(i+1)*3, 120-histX[i+2]*20, TFT_WHITE);
      tft.drawLine(10+i*3, 170-histY[i]*20, 10+(i+1)*3, 170-histY[i+2]*20, TFT_GREEN);
      tft.drawLine(10+i*3, 220-histZ[i]*20, 10+(i+1)*3, 220-histZ[i+2]*20, TFT_ORANGE);
    }

    histX[SAMPLES-2] = histX[SAMPLES-1];
    histX[SAMPLES-1] = gX;
    histY[SAMPLES-2] = histY[SAMPLES-1];
    histY[SAMPLES-1] = gY;
    histZ[SAMPLES-2] = histZ[SAMPLES-1];
    histZ[SAMPLES-1] = gZ;
    timer1 = millis();
  }

  if(millis() - timer2 > 100) {
    updateG(abs(sqrt(gX*gX+gY*gY+gZ*gZ)*10 - 10));
    compass.read();
    updateHeading(compass.heading((LSM303::vector<int>){1, 0, 0}));
    Serial.println(heading);
    timer2 = millis()-50;
  }

  if(millis() - timer3 > 1050) {
    updateTemp(aht10.readTemperature()*10);                 // get new temperature
    updateHumidity(aht10.readHumidity()*10);                 // get new humidity
    // snprintf(report, sizeof(report), "A: %6d %6d %6d    M: %6d %6d %6d", compass.a.x/160, compass.a.y/160, compass.a.z/160, compass.m.x, compass.m.y, compass.m.z);
    // Serial.println(report);
    timer3 = millis()-90;
  }
}