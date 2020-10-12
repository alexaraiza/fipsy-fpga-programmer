# Fipsy FPGA programmer

A tool to program the [Fipsy FPGA](https://www.mocomakers.com/fipsy-fpga/) via Wi-Fi.

![Screenshot](screenshot.png)

## General info

This project aims to facilitate FPGA development by providing an over-the-air programming tool, allowing FPGA configurations to be tested remotely.

This repository contains source code to enable an ESP8266 microcontroller to function as a web server, listening to incoming commands from clients in the same LAN and communicating with the Fipsy FPGA via SPI.

## Features

- Program FPGA
- Erase FPGA
- Check FPGA ID
- Store files on the microcontroller
- Support for multiple devices and browsers (not Internet Explorer)

**To do**

- Support for other FPGAs

## Technologies

**Front end**
- [HTML5](https://html.spec.whatwg.org/)
- [CSS3](https://www.w3.org/TR/CSS/)
- [JavaScript ES6](https://www.w3schools.com/Js/js_es6.asp)
- [AJAX](https://www.w3schools.com/xml/ajax_intro.asp)

**Back end**
- [Arduino programming language (C++)](https://www.arduino.cc/reference/en/)

## Setup

1. [Download ESP8266LittleFS](https://arduino-esp8266.readthedocs.io/en/latest/filesystem.html#uploading-files-to-file-system)
2. Upload the front end code to the flash file system of the ESP8266
3. Set the SSID and password of the networks
4. Upload the Arduino sketch to the microcontroller
5. Connect to the server\'s IP address

## Sources

This project was based on functionality implemented in the [Arduino-Fipsy-Programmer](https://github.com/MocoMakers/Arduino-Fipsy-Programmer) repository by MoCo Makers.