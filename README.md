# powerfeather_test_build

ESP32-based low-power image/audio recording device.

## Setup

Set-up [ESP-IDF](https://github.com/espressif/esp-idf) then run:

```idf.py flash``` 

or ```idf.py flash monitor``` to monitor output.

## Hardware

We use the ESP32-based [PowerFeather](https://github.com/espressif/esp-idf).

Camera
- GPIO pin layout in ```cam.h```.
- 3v3 -> 3v3, GND -> GND1
- Leave RET and PWDN unconnected

Mic
- GPIO pin layout in ```mic.h```
- VDD -> EN, GND -> GND2

PIR
- VDD -> VBAT*, GND -> GND2, SD->D13

GPS
- TX -> RX, RX -> TX, GND -> GND2, VDD -> QON

## Notes
- PowerFeather only has 2 GND.
- Make the following modifications to ```powerfeather-sdk``` and ```esp32-camera``` under ```managed-components```:
  - ```esp32-camera/driver/sccb.h```: Set default I2C port to 1
  - ```powerfeather-sdk/src/Utils/MasterI2C.cpp```: remove line 54
  - ```powerfeather-sdk/src/Mainboard/Mainboard.h```: on line 606, set ```_i2cPort``` to 0.
