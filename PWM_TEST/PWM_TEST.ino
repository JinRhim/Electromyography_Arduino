

//D15 = PWM Thumb: GPIO 15
//D2 = PWM Index: GPIO 2 
//D4 = PWM Pinky: GPIO4


// Parallex Servo (Index) = 4 ~ 6 V 15 ~ 200 mAh

//J- DEAL Micro Servo 9g SG90 = 4.8 ~ 6 V 100mA ~ 250mA 


#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include <ESP32Servo.h>

#define flexing_thumb 180 
#define flexing_index 180
#define flexing_pinky 180

#define clenching_thumb 60
#define clenching_index 0
#define clenching_pinky 0

#define half_clenching_thumb 120
#define half_clenching_index 90
#define half_clenching_pinky 90



Servo myservo_thumb;  // create servo object to control a servo
Servo myservo_index; 
Servo myservo_pinky; 

// twelve servo objects can be created on most boards

char pos_thumb = 0;    // variable to store the servo position
char pos_index = 0; 
char pos_pinky = 0; 


void setup() {
  myservo_thumb.attach(15);  // attaches the servo on pin GPIO 15 to the servo object
  myservo_index.attach(2); 
  myservo_pinky.attach(4); 
}

void grab() {
  myservo_thumb.write(clenching_thumb); 
  myservo_index.write(clenching_index);
  myservo_pinky.write(clenching_pinky);
}

void rest() {
  myservo_thumb.write(flexing_thumb); 
  myservo_index.write(flexing_index);
  myservo_pinky.write(flexing_pinky);
}

void thumb_up() {
  myservo_thumb.write(flexing_thumb); 
  myservo_index.write(clenching_index); 
  myservo_pinky.write(clenching_pinky);
}


void loop() {
  grab();
  delay(1000); 
  rest();
  delay(1000);
  thumb_up(); 
  delay(1000); 
}
