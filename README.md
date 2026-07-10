# RFID Combination Lock with Environmental Tamper Detection



![RFID Combination Lock with Environmental Tamper Detection](https://breadboard.hackclub.com/api/uploads/project-screenshots/WhKgYkbtBu6TToeYEtrImv7lv05UInqG/94/f7174e4b-5944-4b6e-aaae-3b83f5653a99.png)

A tabletop electronic safe. Scan an RFID card to start, enter a secret sequence on the button input, watch progress on an LCD, and a servo unlocks the vault door on success. A DHT11 acts as a tamper sensor (if someone opens the case, temp/humidity spikes trigger lockout). An RTC logs unlock timestamps as a vault log.


> Built in [Breadboard](https://breadboard.hackclub.com), a Hack Club program. This project took ~2.1 hours of work.



## How To Use It

As of writing this, some sensors don't work in the sim and can be bypassed by typing codes in the serial monitor for my project. Start by scanning the RFID card, if it doesn't work on the sim, type "scan" in the serial monitor. Next, use the 2 buttons to write the code, one button is change number and the other is confirm. The code is 1337. To close the safe, increase the temp by 5 celsius. This can be done IRL by placing your hand over the sensor, but if you can't do it in the sim, set the temp before unlocking the safe by typing "temp <celsius>" in the serial monitor and increasing it by 5 to lock the safe once opened.


## Demo

- **Try it:** [https://breadboard.hackclub.com/share/94](https://breadboard.hackclub.com/share/94)


## Schematic

The editor snapshot is in `breadboard-project.json`.


## Bill of Materials

| Part | Quantity |
| --- | --- |
| breadboard-full | 1 |
| buzzer-passive | 1 |
| dht11 | 1 |
| ds1302 | 1 |
| lcd1602 | 1 |
| lcd1602-i2c | 1 |
| pushbutton | 2 |
| rc522-rfid | 1 |
| resistor-10k | 1 |
| servo | 1 |


## Firmware

Firmware files are in the `firmware/` folder.


## Build Journal

Build journal entries are kept in [`journals.md`](journals.md).


---



*Made in [Breadboard](https://breadboard.hackclub.com) — 2.1h of work*



<p align="center"><img src="https://cdn.hackclub.com/019efae7-6857-75a2-8bc1-2618087b4eae/a%20bred%20tanuki%20(3).png" width="64" alt="Breadboard mascot" /></p>