#include "BluetoothSerial.h"
#include "String.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "hal/adc_types.h"
#include <ESP32Servo.h>

BluetoothSerial SerialBT;
#define EMG
#ifdef EMG
  #define TWO_SENSOR
#endif
#define CV

enum Commands {
  NONE = '0',
  LARGE = '1',
  SMALL,
  PALMAR,
  BALL,
  LATERAL,
  THUMB1,
  THUMB2,
  THUMB3,
  RESETGRIP,
  RESETLARGE = 'a',
  RESETSMALL,
  RESETPALMAR,
  RESETBALL,
  RESETLATERAL,
  RESETTHUMB1,
  RESETTHUMB2,
  RESETTHUMB3,
  HANDWAVE
};
void recvWithEndMarker();
void showNewData();
Commands command = NONE;

const byte numChars = 32;
char receivedChars[numChars];   // an array to store the received data

boolean newData = false;

#define SENSOR1 ADC1_CHANNEL_5
#define SENSOR2 ADC1_CHANNEL_4
#define SPERIOD 1000 //number of cyles per sample
#define WAIT    (3 * (1000000 / SPERIOD)) //in number of samples. Currently at 2 kHz this is 3 seconds
#define WINDOW  50
#define PER     10 //Percentage of the way between high and low that the thresholds will be placed
#define GSTART  LARGE
#define GEND    RESETTHUMB3
#define RSTART  HANDWAVE
#define REND    HANDWAVE
#define CLOSE   300

Servo myservo;
Servo myservo2;
Servo myservo3;
Servo myservo4;

static const int servopin  = 25;
static const int servopin2 = 26;
static const int servopin3 = 27;
static const int servopin4 = 14;
static const int ledPin  = 4;
static const int ledPin2 = 5;
static const int fsr  = 19;
static const int fsr2 = 21;
static const int fsr3 = 22;
static const int fsr4 = 23;
volatile int btnstate = 0;
volatile int btnstate2 = 0;
volatile int btnstate3 = 0;
volatile int btnstate4 = 0;
int angle = 0;
int angle2 = 0;
int angle3 = 0;
int angle4 = 0;

#ifdef EMG
int16_t adcRawVal[2][WAIT];
float alpha = 0.5;
float hpFilteredTrain[2][WAIT]; //High pass
float bpFilteredTrain[2][WAIT]; //band pass
float emgLow[2];
float emgHigh[2];
float dist[2];
int16_t offset[2] = {0, 0};

int16_t ADC_Buff[2][WINDOW];
float hpFiltered[2][WINDOW];
float bpFiltered[2][WINDOW];
int ADC_Buff_Index = 0;
float sum[2] = {0, 0};
float ADC_avg[2] = {0, 0};

bool flag = false;
bool newAVG = false;
bool done = false;
#endif

#ifdef CV
int pixDistance = 0;
bool pixReady = false;
#endif

hw_timer_t * timer = NULL;

//synchronization between the main code and interrupt.
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

void IRAM_ATTR stops() {
  btnstate = digitalRead(fsr);
  if (btnstate == LOW) {
    myservo.detach();
  }
}

void IRAM_ATTR stops2() {
  btnstate2 = digitalRead(fsr2);
  if (btnstate2 == LOW) {
    myservo2.detach();
  }
}

void IRAM_ATTR stops3() {
  btnstate3 = digitalRead(fsr3);
  if (btnstate3 == LOW) {
    myservo3.detach();
  }
}

void IRAM_ATTR stops4() {
  btnstate4 = digitalRead(fsr4);
  if (btnstate4 == LOW) {
    myservo4.detach();
  }
}

void callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param) {
  if (event == ESP_SPP_SRV_OPEN_EVT) {
    Serial.println("Client Connected");
  }

  if (event == ESP_SPP_CLOSE_EVT) {
    Serial.println("Client Disconnected");
  }
}

#ifdef EMG
void IRAM_ATTR onTimer() {     //compiler pace IRAM_ATTR to compile in Internal RAM.
  //enter internal service routine for timer interrupt
  portENTER_CRITICAL_ISR(&timerMux);
  TriggerADC();

  //exit internal service routine
  portEXIT_CRITICAL_ISR(&timerMux);

}
#endif

void setup() {
  // Serial for Android
  Serial.begin(115200);
  Serial2.begin(115200);
  SerialBT.register_callback(callback);

  if (!SerialBT.begin("ESP32")) {
    Serial.println("An error occured initializing Bluetooth");
  } else {
    Serial.println("Bluetooth initialized");
  }

  Serial.println("The device started, now you can pair it with bluetooth!");

  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT);

#ifdef EMG
  adc_power_acquire();
  adc1_config_channel_atten(SENSOR1, ADC_ATTEN_DB_11);

#ifdef TWO_SENSOR
  adc1_config_channel_atten(SENSOR2, ADC_ATTEN_DB_11);
#endif

  timer = timerBegin(0, 80, true);  // every 1 seconds timer is triggered

  //timer = global varaiable for time
  // &onTimer = pointer to the initialized timer
  // edge trigger = true. level trigger = false.
  timerAttachInterrupt(timer, &onTimer, true);

  // 80 mHz / 80  = 1 million.
  // 1 million microseconds = 1 seconds
  // 1000000 / 1000 = 1000 us = 1 ms.
  // ADC is triggered every 1ms to keep 1000 Hz sampling rate.
  timerAlarmWrite(timer, SPERIOD / 2, true);  //divide by sampling period/2 to sample the 2 sensors.
  timerAlarmEnable(timer);
#endif

  myservo.attach(servopin);
  myservo2.attach(servopin2);
  myservo3.attach(servopin3);
  myservo4.attach(servopin4);
  attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
  attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
  attachInterrupt(digitalPinToInterrupt(fsr3), stops3, HIGH);
  attachInterrupt(digitalPinToInterrupt(fsr4), stops4, HIGH);
}

#ifdef EMG
unsigned int numdat = 0;
void TriggerADC() {
  static bool flip = false; //Sample each sensor at 1 kHz
  //Interrupts is at 2 kHz
  //flip acts as ping pong to switch between sensors

  //After 3 second setup period for 3 seconds or after 3 second transition period for 3 seconds
  if (((numdat >= WAIT) && (numdat < 2 * WAIT)) || ((numdat >= 3 * WAIT) && (numdat < 4 * WAIT))) {
    flag = 1;
    if (!flip)
      adcRawVal[0][numdat % WAIT] = adc1_get_raw(SENSOR1) - offset[0]; //Subtract offset to bias around 0 for MAV

#ifdef TWO_SENSOR
    if (flip)
      adcRawVal[1][numdat % WAIT] = adc1_get_raw(SENSOR2) - offset[1]; //Subtract offset to bias around 0 for MAV
#endif

    digitalWrite(ledPin, HIGH);
    digitalWrite(ledPin2, HIGH);
  }

  if (numdat == 5 * WAIT) {// After calibration is done.
    done = true;
    numdat++;
  }

  if (done) {// After calibration is done.
    if (!flip)
      //      adcRawVal[0][0] = adc1_get_raw(SENSOR1);
      //save the adc value to WINDOW array
      ADC_Buff[0][ADC_Buff_Index] = adc1_get_raw(SENSOR1) - offset[0]; //Subtract offset

#ifdef TWO_SENSOR
    if (flip)
      //      adcRawVal[1][0] = adc1_get_raw(SENSOR2);
      //save the adc value to WINDOW array
      ADC_Buff[1][ADC_Buff_Index] = adc1_get_raw(SENSOR2) - offset[1]; //Subtract offset
#endif

    //once the data is 100th index, it will overwrite itself
    if (flip) {
      ADC_Buff_Index = (ADC_Buff_Index + 1) % WINDOW;
      if (ADC_Buff_Index == 0) {
        newAVG = true;
      }
    }
  }

  if (flip && (numdat < 5 * WAIT)) {
    numdat++;
  }
  flip ^= 1;
}
#endif

#ifdef CV
void convertCharsToInt() {
    char val = Serial2.read();
    //Serial2.read() gives one character at a time
    //In order to make up for this, we reconstruct the number it is sending
    if (val == '\n') { //newline character acts as the end marker
      pixReady = true;
    } else if ((val >= '0') && (val <= '9')) {
      //convert from char to digit and multiply old number by 10
      pixDistance = (pixDistance * 10) + (val - '0');
    }
    //    Serial.printf("character %c", val);
    //    if (pixReady) {
    //      Serial.printf("integer %d\n", pixDistance);
    //    }
}
#endif

#ifdef EMG
void calcWindowMAV() {
    for (int i = 1; i < WINDOW; i++) { //high pass
      //data_filtered[i] = alpha * (data_filtered[i-1] + data[i] - data[i-1]);
      hpFiltered[0][i] = alpha * (hpFiltered[0][i - 1] + ADC_Buff[0][i] - ADC_Buff[0][i - 1]);

#ifdef TWO_SENSOR
      hpFiltered[1][i] = alpha * (hpFiltered[1][i - 1] + ADC_Buff[1][i] - ADC_Buff[1][i - 1]);
#endif

    }
    for (int i = 1; i < WINDOW; i++) { //low pass
      //data_filtered[i] = alpha * data[i] + (1 - alpha) * data_filtered[i-1];
      bpFiltered[0][i] = alpha * hpFiltered[0][i] + (1 - alpha) * bpFiltered[0][i - 1];

#ifdef TWO_SENSOR
      bpFiltered[1][i] = alpha * hpFiltered[1][i] + (1 - alpha) * bpFiltered[1][i - 1];
#endif

    }

    sum[0] = 0;
    sum[1] = 0;
    for (int i = 0; i < WINDOW; i++) {
      if (bpFiltered[0][i] < 0) { //if value is negative
        sum[0] -= bpFiltered[0][i]; //add absolute value
      } else {
        sum[0] += bpFiltered[0][i];
      }

#ifdef TWO_SENSOR
      if (bpFiltered[1][i] < 0) { //if value is negative
        sum[1] -= bpFiltered[1][i]; //add absolute value
      } else {
        sum[1] += bpFiltered[1][i];
      }
#endif

    }
    ADC_avg[0] = sum[0] / WINDOW; //calculate 100 datapoint average
    ADC_avg[1] = sum[1] / WINDOW; //calculate 100 datapoint average
    newAVG = false;
    //    Serial.printf("%d\t%d\n",ADC_avg[0], ADC_avg[1]);
}

void calcClenchMAV() {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);

    sum[0] = 0;
    sum[1] = 0;
    for (int i = 0; i < WAIT; i++) { //calibrate offset
      sum[0] += adcRawVal[0][i];

#ifdef TWO_SENSOR
      sum[1] += adcRawVal[1][i];
#endif

    }
    offset[0] = sum[0] / WAIT;

#ifdef TWO_SENSOR
    offset[1] = sum[1] / WAIT;
#endif

    //    Serial.printf("Offset %d\t%d\n", offset[0], offset[1]);
    for (int i = 0; i < WAIT; i++) {
      adcRawVal[0][i] -= offset[0]; //subtract offset

#ifdef TWO_SENSOR
      adcRawVal[1][i] -= offset[1];
#endif

    }

    //perform high and low pass filters
    hpFilteredTrain[0][0] = 0;
    hpFilteredTrain[1][0] = 0;
    for (int i = 1; i < WAIT; i++) { //High
      //%data_filtered[i] = alpha * (data_filtered[i-1] + data[i] - data[i-1]);
      hpFilteredTrain[0][i] = alpha * (hpFilteredTrain[0][i - 1] + (float)adcRawVal[0][i] - (float)adcRawVal[0][i - 1]);

#ifdef TWO_SENSOR
      hpFilteredTrain[1][i] = alpha * (hpFilteredTrain[1][i - 1] + (float)adcRawVal[1][i] - (float)adcRawVal[1][i - 1]);
#endif

    }

    for (int i = 1; i < WAIT; i++) { //Low
      //%data_filtered[i] = alpha * data[i] + (1 - alpha) * data_filtered[i-1];
      bpFilteredTrain[0][i] = alpha * hpFilteredTrain[0][i] + (1 - alpha) * bpFilteredTrain[0][i - 1];

#ifdef TWO_SENSOR
      bpFilteredTrain[1][i] = alpha * hpFilteredTrain[1][i] + (1 - alpha) * bpFilteredTrain[1][i - 1];
#endif

    }

    sum[0] = 0;
    sum[1] = 0;
    for (int i = 0; i < WAIT; i++) {
      // Calculate average of clenched calibration phase
      if (bpFilteredTrain[0][i] < 0) { //if value is negative
        sum[0] -= bpFilteredTrain[0][i]; //add absolute value
      } else {
        sum[0] += bpFilteredTrain[0][i];
      }

#ifdef TWO_SENSOR
      if (bpFilteredTrain[1][i] < 0) { //if value is negative
        sum[1] -= bpFilteredTrain[1][i]; //add absolute value
      } else {
        sum[1] += bpFilteredTrain[1][i];
      }
#endif

      //      Serial.printf("%.2f\t%.2f\n", bpFilteredTrain[0][i], bpFilteredTrain[1][i]); //print values from calibration phase
    }
    //    Serial.println("-----");
    emgHigh[0] = sum[0] / WAIT; //calculate high average
    emgHigh[1] = sum[1] / WAIT; //calculate high average
    flag = 0;
    //    Serial.printf("average = %f\t%f\n", emgHigh[0], emgHigh[1]);
}

void calcRelaxMAV() {
    digitalWrite(ledPin, LOW);
    digitalWrite(ledPin2, LOW);

    //perform high and low pass filters
    hpFilteredTrain[0][0] = 0;
    hpFilteredTrain[1][0] = 0;
    for (int i = 1; i < WAIT; i++) { //High
      //%data_filtered[i] = alpha * (data_filtered[i-1] + data[i] - data[i-1]);
      hpFilteredTrain[0][i] = alpha * (hpFilteredTrain[0][i - 1] + (float)adcRawVal[0][i] - (float)adcRawVal[0][i - 1]);

#ifdef TWO_SENSOR
      hpFilteredTrain[1][i] = alpha * (hpFilteredTrain[1][i - 1] + (float)adcRawVal[1][i] - (float)adcRawVal[1][i - 1]);
#endif

    }

    for (int i = 1; i < WAIT; i++) { //Low
      //%data_filtered[i] = alpha * data[i] + (1 - alpha) * data_filtered[i-1];
      bpFilteredTrain[0][i] = alpha * hpFilteredTrain[0][i] + (1 - alpha) * bpFilteredTrain[0][i - 1];

#ifdef TWO_SENSOR
      bpFilteredTrain[1][i] = alpha * hpFilteredTrain[1][i] + (1 - alpha) * bpFilteredTrain[1][i - 1];
#endif

    }

    sum[0] = 0;
    sum[1] = 0;
    for (int i = 0; i < WAIT; i++) {
      // Calculate average of relaxed calibration phase
      if (bpFilteredTrain[0][i] < 0) { //if value is negative
        sum[0] -= bpFilteredTrain[0][i]; //add absolute value
      } else {
        sum[0] += bpFilteredTrain[0][i];
      }

#ifdef TWO_SENSOR
      if (bpFilteredTrain[1][i] < 0) { //if value is negative
        sum[1] -= bpFilteredTrain[1][i]; //add absolute value
      } else {
        sum[1] += bpFilteredTrain[1][i];
      }
#endif

      //      Serial.printf("%.2f\t%.2f\n", bpFilteredTrain[0][i], bpFilteredTrain[1][i]); //print values from calibration phase
    }

    emgLow[0] = sum[0] / WAIT; //calculate low average
    emgLow[1] = sum[1] / WAIT; //calculate low average
    flag = 0;
    //    Serial.printf("average = %f\t%f\n", emgLow[0], emgLow[1]);

    dist[0] = emgHigh[0] - emgLow[0]; //calculate distance between high and low average
    dist[1] = emgHigh[1] - emgLow[1]; //calculate distance between high and low average
}
#endif

void loop() {
#ifdef CV
  if (pixReady) {//if we already had a full value from the Jetson Nano
    pixDistance = 0; //Reset it
    pixReady = false;
  }
#endif
  
  if (Serial.available()) {
    SerialBT.write(Serial.read());
  }

#ifdef CV
  if (Serial2.available()) {
    convertCharsToInt();
  }
#endif

#ifdef EMG
  //After the clenched calibration phase has completed
  if ((numdat >= 2 * WAIT) && (numdat < 3 * WAIT) && flag) {
    calcClenchMAV();
  }

  //After the unclenched calibration phase has completed
  if ((numdat >= 4 * WAIT) && flag) {
    calcRelaxMAV();
  }
  
  if (newAVG) {
    calcWindowMAV();
  }

  if (ADC_avg[0] > (emgHigh[0] - ((PER * dist[0]) / 100))) {
    digitalWrite(ledPin, HIGH);
  } else if (ADC_avg[0] < (emgLow[0] + ((PER * dist[0]) / 100))) {
    digitalWrite(ledPin, LOW);
  }

#ifdef TWO_SENSOR
  if (ADC_avg[1] > (emgHigh[1] - ((PER * dist[1]) / 100))) {
    digitalWrite(ledPin2, HIGH);
  } else if (ADC_avg[1] < (emgLow[1] + ((PER * dist[1]) / 100))) {
    digitalWrite(ledPin2, LOW);
  }
#endif
#endif
  recvWithEndMarker();
  showNewData();
}

void recvWithEndMarker() {
  static byte ndx = 0;
  char endMarker = '\n';
  char rc;

  while (SerialBT.available() > 0 && newData == false) {
    rc = SerialBT.read();

    if (rc != endMarker) {
      receivedChars[ndx] = rc;
      ndx++;

      if (ndx >= numChars) {
        ndx = numChars - 1;
      }
    } else {
      receivedChars[ndx] = '\0';
      ndx = 0;
      newData = true;
    }
  }
}

void showNewData() {
  if (newData == true) {
    command = NONE;
    // Grasps
    if (strncmp(receivedChars, "large", 5) == 0) {
      command = LARGE;
      //      Serial.write("1");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "small", 5) == 0) {
      command = SMALL;
      //      Serial.write("2");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "palmar", 6) == 0) {
      command = PALMAR;
      //      Serial.write("3");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "ball", 4) == 0) {
      command = BALL;
      //      Serial.write("4");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "lateral", 7) == 0) {
      command = LATERAL;
      //      Serial.write("5");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "thumb1", 6) == 0) {
      command = THUMB1;
      //      Serial.write("6");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "thumb2", 6) == 0) {
      command = THUMB2;
      //      Serial.write("7");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "thumb3", 6) == 0) {
      command = THUMB3;
      //      Serial.write("8");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetgrip", 9) == 0) {
      command = RESETGRIP;
      //      Serial.write("9");
      //      Serial.flush();

      // Resets
    } else if (strncmp(receivedChars, "resetlarge", 10) == 0) {
      // 97
      command = RESETLARGE;
      //      Serial.write("a");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetsmall", 10) == 0) {
      // 98
      command = RESETSMALL;
      //      Serial.write("b");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetpalmar", 11) == 0) {
      // 99
      command = RESETPALMAR;
      //      Serial.write("c");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetball", 9) == 0) {
      // 100
      command = RESETBALL;
      //      Serial.write("d");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetlateral", 12) == 0) {
      // 101
      command = RESETLATERAL;
      //      Serial.write("e");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetthumb1", 11) == 0) {
      // 102
      command = RESETTHUMB1;
      //      Serial.write("f");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetthumb2", 11) == 0) {
      // 103
      command = RESETTHUMB2;
      //      Serial.write("g");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "resetthumb3", 11) == 0) {
      // 104
      command = RESETTHUMB3;
      //      Serial.write("h");
      //      Serial.flush();
    } else if (strncmp(receivedChars, "handwave", 8) == 0) {
      // 105
      command = HANDWAVE;
      //      Serial.write("i");
      //      Serial.flush();
    }
    newData = false;
  }
#ifdef EMG
  if ((ADC_avg[0] > (emgHigh[0] - ((PER * dist[0]) / 100))) && (command >= GSTART) && (command <= GEND)) {
#ifdef TWO_SENSOR
    if (ADC_avg[1] > (emgHigh[1] - ((PER * dist[1]) / 100))) {
#endif
#endif
#ifdef CV
      if ((pixDistance < CLOSE) && pixReady) {
#endif
        //double high signal
        Serial.write(command);
        Serial.flush();

        switch (command) {
          case LARGE:

            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr3), stops3, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr4), stops4, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);
            myservo4.write(180);

            break;
          case SMALL:

            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);

            break;
          case BALL:
            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr3), stops3, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr4), stops4, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);
            myservo4.write(180);

            break;
          case RESETLARGE:

            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));
            detachInterrupt(digitalPinToInterrupt(fsr4));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);
            myservo4.attach(servopin4);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);
            myservo4.write(angle4);

            break;
          case RESETSMALL:
            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);

            break;
          case RESETBALL:
            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));
            detachInterrupt(digitalPinToInterrupt(fsr4));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);
            myservo4.attach(servopin4);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);
            myservo4.write(angle4);

            break;
          default:
            break;
        }

        command = NONE;
#ifdef CV
        pixDistance = 0;
        pixReady = false;
      }
#endif
#ifdef EMG
#ifdef TWO_SENSOR
    }
#endif
  } else if ((ADC_avg[0] < (emgLow[0] + ((PER * dist[0]) / 100))) && (command >= RSTART) && (command <= REND)) {
#ifdef TWO_SENSOR
    if (ADC_avg[1] < (emgLow[1] + ((PER * dist[1]) / 100))) {
#endif
#endif
#ifdef CV
      if ((pixDistance < CLOSE) && pixReady) {
#endif
        //double low signal
        Serial.write(command);
        Serial.flush();

        switch (command) {
          case LARGE:

            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr3), stops3, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr4), stops4, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);
            myservo4.write(180);

            break;
          case SMALL:

            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);

            break;
          case BALL:
            attachInterrupt(digitalPinToInterrupt(fsr), stops, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr2), stops2, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr3), stops3, HIGH);
            attachInterrupt(digitalPinToInterrupt(fsr4), stops4, HIGH);

            myservo.write(180);
            myservo2.write(180);
            myservo3.write(180);
            myservo4.write(180);

            break;
          case RESETLARGE:

            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));
            detachInterrupt(digitalPinToInterrupt(fsr4));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);
            myservo4.attach(servopin4);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);
            myservo4.write(angle4);

            break;
          case RESETSMALL:
            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);

            break;
          case RESETBALL:
            detachInterrupt(digitalPinToInterrupt(fsr));
            detachInterrupt(digitalPinToInterrupt(fsr2));
            detachInterrupt(digitalPinToInterrupt(fsr3));
            detachInterrupt(digitalPinToInterrupt(fsr4));

            myservo.attach(servopin);
            myservo2.attach(servopin2);
            myservo3.attach(servopin3);
            myservo4.attach(servopin4);

            myservo.write(angle);
            myservo2.write(angle2);
            myservo3.write(angle3);
            myservo4.write(angle4);

            break;
          default:
            break;
        }

        command = NONE;
#ifdef CV
        pixDistance = 0;
        pixReady = false;
      }
#endif
#ifdef EMG
#ifdef TWO_SENSOR
    }
#endif
  }
#endif
}
