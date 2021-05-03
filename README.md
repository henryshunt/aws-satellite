# AWS Satellite
A simple Arduino device that, as part of the [AWS](https://github.com/henryshunt/aws) project, allows for certain low-voltage sensors to be placed far away from (i.e. a satellite of) the main system. Communication with the satellite device is done via RS-485 serial.

# Usage
- Install the ArduinoJson library in the Arduino IDE.
- Upload the program to an Arduino Nano Every.
- Over serial, use the `CONFIG` command to enable and configure the connected sensors.
- Use the `SAMPLE` command to sample the sensors and send back the data.