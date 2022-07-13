// Jin Rhim
// Butterworth Filter 2 
// https://github.com/tttapa/Arduino-Filters

//https://github.com/MartinBloedorn/libFilter


#include <filters.h>
    
    const float cutoff_freq   = 20.0;  //Cutoff frequency in Hz
    const float sampling_time = 0.001; //Sampling time in seconds.
    IIR::ORDER  order  = IIR::ORDER::OD3; // Order (OD1 to OD4)
    
    // Low-pass filter
    Filter f(cutoff_freq, sampling_time, order);
    
    void setup() {
      Serial.begin(115200);
      pinMode(A0, INPUT);
      // Enable pull-ups if necessary
      digitalWrite(A0, HIGH);
    }
    
    void loop() {
      int value = analogRead(0);
      float filteredval = f.filterIn(value);
      //View with Serial Plotter
      Serial.print(value, DEC);
      Serial.print(",");
      Serial.println(filteredval, 4);
      delay(5); // Loop time will approx. match the sampling time.
    }
