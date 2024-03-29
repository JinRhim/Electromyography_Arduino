# Prosthetic Arm with EMG Sensor and ESP32
<img width="1224" alt="image" src="https://user-images.githubusercontent.com/93160540/178064255-7b6dec91-0a37-433b-b4ca-ff3477a1df8b.png">

<img width="638" alt="image" src="https://user-images.githubusercontent.com/93160540/175752176-7b88f66e-afa7-4303-b2ed-c7faa6b5244f.png">

## what is electromyography? 

Because our muscles repolarize and depolarize, just like brain neurons, our muscles generate action potention. Whenever the signal comes in, the muscle cell membrain rapidly depolarize and then slowly repolarize. Its cell membrane action potential changes from -70mv to 20mv, then slowly repolarizes back to -70mv, using ATP. 
![image](https://user-images.githubusercontent.com/93160540/175752437-80cee87f-d4d2-44db-bd75-96c39e4b6d36.png)

Electromyography detects such small electrical changes in muscle and amplifies those signals. Those amplified analog signals then could be read through Analog to Ditigal Comparator of ESP32 board. 

The EMG sensor should be placed at the flexor muscle 
<img width="1241" alt="image" src="https://user-images.githubusercontent.com/93160540/178055154-1e55b69f-17b5-45a6-9ed2-25283ac5a8fb.png">


## What does Nyquiest Theorm States? Why is the sampling rate at 1000 Hz? 

Nyquiest Theorm states that sampling rate must be at least twice the highest frequency component of signal. If the sampling rate is lower than the minimum sampling rate, then it will cause aliasing effect. Too high sampling rate will cause computational complexity. Most of other research consisted of 500 Hz to 2000 Hz sampling rate, so our group decided to have sampling rate of 1000 Hz.

## Two EMG sensor: Myoware EMG sensor vs AK 8232

Although Myoware EMG sensor have high-pass filter included and have hardware circuit that makes rectified signal, its high price and lack of availability made us to test other types of sensors. I found AK8232 EMG sensor as useful replacement for Myoware EMG sensor, as it is comparatively cheaper and widely available during the circuit shortage. However, the lack of high-pass filter in AK 8232 forced us to implement the physical high-pass filter, due to its severe baseline drifting. 

## What is low-pass filter and high-pass filter? 

Filters remove the useless signals that comes from external source, other than EMG signal. There are multiple sources of noises in sEMG signal, such as 

 - Electronics of amplification system (thermal noise)
 - Skin-electrode interface (electro-chemical noise)
 - movement artifact noise when underlying skin causing a movement at electrode-skin interface 
 - power line noise  
 - cable motion artifiact 
 - can be eliminated by appropriate circuit design 

| Low-Pass Filter| High-Pass Filter |
| ------------- | ------------- |
| Removes baseline drift associated with movement or perspiration  | Prevents aliasing |
| Removes DC offset  | Content Cell  |
|Typical Cutoff: 5 ~ 20 Hz | Typical Cutoff: 200 ~ 1000 Hz|


<img width="1208" alt="image" src="https://user-images.githubusercontent.com/93160540/178063855-a34c9010-a174-48c8-8733-1e7b021a149a.png">

The picture on the left is from the myoware sensor, and the picture on the right is from AD8232 EKG sensor. Because AD8232 sensor lacks high-pass filter, it lacks the constant baseline.

## Circuit Design

<img width="1360" alt="image" src="https://user-images.githubusercontent.com/93160540/178064052-9e1ccac6-54c5-437b-944c-91c4f12b94a4.png">
<img width="1230" alt="image" src="https://user-images.githubusercontent.com/93160540/178064888-4b2cfe70-a363-4fb9-b61f-3c3a7cfb408d.png">



## Basic Digital Low-Pass Filter 

Although the physical filter implementation helped with the baseline drifting, the physical filter had a possibility of causing another noise. The extensive wire movements kept touching the parts of circuits, and it could be the source of signal noise. 
To remove such factor, we implemented what we learned in DSP class: digital filter. The predictability and zero-noise tolerance of digital filter could remove extra physical filter which could be a burden for small embedded system. 
<img width="662" alt="image" src="https://user-images.githubusercontent.com/93160540/186299855-adf9c36a-0e29-48dd-8eb5-bfff9f5c5876.png">
