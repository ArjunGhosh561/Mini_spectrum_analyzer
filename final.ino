
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>
#include <arduinoFFT.h>
/* Uncomment the initialize the I2C address , uncomment only one, If you get a totally blank screen try the other*/
#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's
//#define i2c_Address 0x3d //initialize with the I2C addr 0x3D Typically Adafruit OLED's

#define SAMPLES 64// power of 2
#define SAMPLING_FREQ 24000 // 12 kHz Fmax = sampleF /2 
#define AMPLITUDE 100 // sensitivity
#define FREQUENCY_BANDS 14

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

#define BARWIDTH 11
#define BARS 11

#define ANALOG_PIN A0

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

double vImag[SAMPLES];
double vReal[SAMPLES];
unsigned long sampling_period_us;
arduinoFFT fft = arduinoFFT(vReal, vImag, SAMPLES, SAMPLING_FREQ);

// adjust reference to get remove background noise noise
float reference = log10(50.0);

double coutoffFrequencies[FREQUENCY_BANDS];


void setup(){
 // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if (!display.begin()) { // Address 0x3C for 128x32
    for (;;); // Don't proceed, loop forever
  }

  // Setup display
  display.clearDisplay();
  display.display();
  display.setRotation(0);
  display.invertDisplay(false);

  // Serial.begin(9600);

  sampling_period_us = (1.0 / SAMPLING_FREQ ) * pow(10.0, 6);
 // Calculate cuttoff frequencies,meake a logarithmic scale base basePOt
  double basePot = pow(SAMPLING_FREQ / 2.0, 1.0 / FREQUENCY_BANDS);
  coutoffFrequencies[0] = basePot;
  for (int i = 1 ; i < FREQUENCY_BANDS; i++ ) {
    coutoffFrequencies[i] = basePot * coutoffFrequencies[i - 1];
  }

  // draw dashed lines to sperate frequency bands
  for (int i = 0; i < BARS - 1 ; i++) {
    for (int j = 0; j < SCREEN_HEIGHT ; j += 4) {
      display.writePixel((i + 1)*BARWIDTH + 2 , j, SH110X_WHITE );
    }
  }
  display.drawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SH110X_WHITE);
}

int oldHeight[20];
int oldMax[20];
double maxInFreq;







void loop(){

// take samples
  for (int i = 0; i < SAMPLES; i++) {
    unsigned long newTime = micros();
    int value = analogRead(ANALOG_PIN);
    vReal[i] = value;
    vImag[i] = 0;
    while (micros() < (newTime + sampling_period_us)) {
      yield();
    }
  }

  // compute FFT
  fft.DCRemoval();
  fft.Windowing(FFT_WIN_TYP_HAMMING, FFT_FORWARD);
  fft.Compute(FFT_FORWARD);
  fft.ComplexToMagnitude();

  double median[20];
  double max[20];
  int index = 0;
  double hzPerSample = (1.0 * SAMPLING_FREQ) / SAMPLES; //
  double hz = 0;
  double maxinband = 0;
  double sum = 0;
  int count = 0;
  for (int i = 2; i < (SAMPLES / 2) ; i++) {
    count++;
    sum += vReal[i];
    if (vReal[i] >  max[index] ) {
      max[index] = vReal[i];
    }
    if (hz > coutoffFrequencies[index]) {
      median[index] = sum / count;
      sum = 0.0;
      count = 0;
      index++;
      max[index] = 0;
      median[index]  = 0;
    }
    hz += hzPerSample;
  }
  // calculate median and maximum per frequency band
  if ( sum > 0.0) {
    median[index] =  sum / count;
    if (median[index] > maxinband) {
      maxinband = median[index];
    }
  }

  int bar = 0;

  for (int i = FREQUENCY_BANDS - 1; i >= 3; i--) {
    int newHeight = 0;
    int newMax = 0;
    // calculate actual decibels
    if (median[i] > 0 && max[i] > 0 ) {
      newHeight = 20.0 * (log10(median[i] ) - reference);
      newMax = 20.0 * (log10(max[i] ) - reference);
    }

    // adjust minimum and maximum levels
    if (newHeight < 0 ||  newMax < 0) {
      newHeight = 1;
      newMax = 1;
    }
    if (newHeight >= SCREEN_HEIGHT - 2) {
      newHeight = SCREEN_HEIGHT - 3;
    }
    if (newMax >= SCREEN_HEIGHT - 2) {
      newMax = SCREEN_HEIGHT - 3;
    }

    int barX = bar * BARWIDTH + 5;
    // remove old level median
    if (oldHeight[i] > newHeight) {
      display.fillRect(barX, newHeight + 1, 7, oldHeight[i], SH110X_BLACK);
    }
    // remove old max level
    if ( oldMax[i] > newHeight) {
      for (int j = oldMax[i]; j > newHeight; j -= 2) {
        display.drawFastHLine(barX , j, 7, SH110X_BLACK);
      }
    }
    // paint new max level
    for (int j = newMax; j > newHeight; j -= 2) {
      display.drawFastHLine(barX , j, 7,  SH110X_WHITE);
    }
    // paint new level median
    display.fillRect(barX , 1, 7, newHeight, SH110X_WHITE);

    oldMax[i] = newMax;
    oldHeight[i] = newHeight;

    bar++;
  }
  display.drawFastHLine(0 , SCREEN_HEIGHT - 1, SCREEN_WIDTH, SH110X_WHITE);

  
  
  display.display();


  
}