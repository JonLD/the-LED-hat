# the_LED_hat project

## Introduction
Still under development. This project was undertaken to develop a visually interesting hat with embedded LEDs for music events such as festivals.

The project uses two Arduino Nano Every's, one in the hat and another in a controller, communicating via RF transcievers. The controller consists of a set of buttons, rotary encoders and OLED displays to adjust adjust lighting effect parameters.

## Hardware
Currently being prototyped using several solderable breadboards. Currently setup to be powered either via an external power supply unit or a battery pack.
### Parts list
I will attempt to give a detail list incase anyone wishes to replicate this project. Some parts are still only in use for prototyping and will not be in the final product.
- Arduino Nano Every x2 
- AZDelivery OLED Display SSD1306 128x64 x2
- AZDelivery NRF24L01 2.4GHz RF Transceiver x2
- HJHX WS2812b Led Strip 60leds/m
- Anker PowerCore Slim 10000
- ALLNICE Mini Toggle Switch
- Glarks 112Pcs 2.54mm Male and Female Pin Header Connector Assortment Kit
- ElectroCookie Prototype PCB Solderable Breadboard
- 5V 30A Power Supply AC 110/220V to DC 5V 150W
- RUNCCI-YUN Mini Momentary Push Button Switch
- WayinTop Rotary Encoder x2
- 2 Pin Universal Screw Terminal Block Connector
- ALMOCN Jumper Wire Kit
- Hiscate Jumper Wires Kit with header connectors

## Software

### Effect loop
The effect generally lasts 1 to 2 bars and is looped until another effect is selected. Effects are created comprised of sub-effects, lasting 1 beat or 1/2 beat. This enables custom sequences to be created from sub-effect building blocks. 

### BPM sync
In order for the effect to be in time with the music, it must be playing at the correct frequency (beats per minute of the music, BPM) and be in phase with the music. 

Syncing the BPM is achieved by with sequencial taps of a button. Each time the button is tapped a counter is incremented. The time from first tap to last tap can then be divided by number of taps to find the time spacing of beats.

Phase syncing is achieved by using a button to reset the current effect back to the start of its sequence. If pressed in time this will result in an effect playing in phase.
