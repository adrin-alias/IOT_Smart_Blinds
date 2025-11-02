# IOT Smart Blinds
A small electromechanical device to automate opening & closing blinds at a lower cost than commercial alternatives.

![Exploded View](docs\images\IoT_Smart_Blinds_Exploded_View.png)

Repo Contains CAD and firmware for the first version on the device.

## Hardware
- ESP 32 Dev Board
- 28BYJ-48 Stepper Motor
- ULN2003 Stepper Driver
- [3D printed parts](cad\exports)
- Adhesive Velcro Strips (Hyper Tough)

## Firmware
- [Arduino/C code](firmware) written using Platform.io

## Software
- Communicates to a Google Sheet 'database' for saving the current status of the blinds.
- [IFTTT](https://ifttt.com) to connect Google Sheets and Samsung SmartThings.
- Samsung SmartThings to read the alarm notification and trigger blinds.

Find more details in [my portfolio](https://adrinalias.vercel.app).

MIT License