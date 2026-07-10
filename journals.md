# RFID Combination Lock with Environmental Tamper Detection Build Journal

## Jul 9, 2026, 5:47 AM

I added some of the main components like the lcd screen, arduino, breadboard, rfid chip and some more. I connected the lcd screen to i2c adapter thing and connected that to the arduino. I made sure to triple check the voltages for all the components as I got it wrong before. I connected the rfid chip, temperature sensor, dht22 module and 74hc595 module to the arduino and connected all the power to the arduino via a breadboard.

---

## Jul 9, 2026, 8:13 AM

I continued with the project by adding all the components and wiring all of them together before adding the resistors for the led matrix and temperature sensor, so the lights dont explode and the temperature signal comes out cleaner. I ran into an issue where I didn't have enough ports in the led matrix driver, so I had to tie half of the led matrix ports to 5v, meaning I can only turn on/off one row at a time.

---

## Jul 10, 2026, 5:05 AM

Today I started on the code and made some pretty big changes. I removed the led matrix and changed the wiring a bit. I also started on the code. After finishing the first draft of my code, I realised some of the components didn't work in the simulator. To work around this, I just used the codes and I spent a while making sure that the code worked. I also forgot to record writing the code... but oh well.