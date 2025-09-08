# PICO-6502

This is a 6502 emulator for the [Raspberry Pi Pico](https://www.raspberrypi.org/products/raspberry-pi-pico/)

This is a fork of the follow repo:

https://github.com/jfoucher/pico-6502

This is a fork of the emulator that runs on a Raspberry Pi Pico, with additional hardware. The hardware is as follows:

* SD Card Reader - To store binary files, that can be run, and other support files.
* RS232 Interface - To connect a USB serial cable
* I/O Expansion board - To be used to emulate the 6522 chip.
* Reset button - Reset the unit without having to unplug the USB cable
* LCD display - For displaying information.

## Compiling for Linux and Mac (Didn't try it on Windows)

* You need the Pi Pico SDK and CMake installed before you start.

* Create a directory called build

    mkdir -p build

* Run CMake

    cmake ..

* Run Make

    make

* Copy the uf2 file to the Pi Pico



## Features

### Configuration File

The file contains the configuration for the device. You can set some things to Auto. If the file does not exist, the defaults
are used:

#### ROM-FILE=roms.txt (Default roms.txt)

#### SERIAL-FLOW=0

Serial Flow Control. Values are:

* 0 = Auto (Default)
* 1 = None
* 2 = RTS/CTS
* 3 = XON/XOFF

#### LCD-INSTALLED=1

LCD Installed

* 0 = No (Default)
* 1 = Yes

#### IO-EMULATION=0

I/O Emulation
* 0 = Auto
* 1 = None
* 2 = VIA Full (Pico GPIO)
* 3 = I/O Only (Pico GPIO)
* 4 = VIA Full (Expansion Board)
* 5 = I/O Only (Expansion Board)

#### I/O Pins if Pico GPIO for VIA

Specify whether the pin is a VIA pin or not for the Pi Pico. Values are:
* VIA - VIA I/O pin
* RESERVED - Pin is used by something else

The VIA pins start with PA0, going up to PA7. The next pin is PB0, going up to 
PB7. If there is more VIA pins specified than actual pins, they are ignored. If
you don't have 16 VIA pins, the rest are not assigned to a pin on the Pi Pico.

    GPIO-0=RESERVED  # Serial RX
    GPIO-1=RESERVED  # Serial TX
    GPIO-2=RESERVED  # Serial CTS
    GPIO-3=RESEVERD  # Serial RTS
    GPIO-4=VIA       # PA0
    GPIO-5=VIA       # PA1
    GPIO-6=VIA       # PA2
    GPIO-7=VIA       # PA3
    GPIO-8=RESERVED  # SD Card MISO
    GPIO-9=RESERVED  # SD Card CS
    GPIO-10=RESERVED # SD Card SCLK
    GPIO-11=RESERVED # SD Card MOSI
    GPIO-12=VIA      # PA4
    GPIO-13=VIA      # PA5
    GPIO-14=VIA      # PA6
    GPIO-15=VIA      # PA7

    GPIO-16=VIA      # PB0
    GPIO-17=VIA      # PB1
    GPIO-18=VIA      # PB2
    GPIO-19=VIA      # PB3
    GPIO-20=VIA      # PB4
    GPIO-21=VIA      # PB5
    GPIO-22=VIA      # PB6
    GPIO-26=VIA      # PB7
    GPIO-27=RESERVED # Unused
    GPIO-28=RESERVED # Unused


### Loading ROMs from SD Card

I added the SD Card to allow you to load the ROMs from the card, instead loading them using header files. Originally, the 
the emulator loaded the Taliforth ROM from a header file. I have converted it to a .bin file, and also included other 
binary files, that can be loaded. This allows you to develop 6502 code and run it, without recompiling the emulator.
The emulator looks for a roms.txt file, which includes the description, bin file, starting location, VIA location, and I/O location

Included are binary files for the following:

* TaliForth
* WOZMon (Modified for the Pico-6502)
* MSBasic (Modified and extended for the Pico-6502) https://github.com/sean-johnston/msbasic
* Hello World program
* Echo (Echos keystrokes from the terminal)

### Extended I/O

I extended the I/O to allow saving and loading to the SD Card, as well as displaying a directory. The following are the default I/O 
addresses. The start of theses addresses can be overridden in the roms.txt file. The protocol for the extra I/O will be explained
later in this document. The addresses are as follows:

* $F001 - Character Out - Send a character to the terminal
* $F002 - File Mode - Set the type of operation to perform
* $F003 - File Data - This is used to stream the data out, based on the File Mode
* $F004 - Character In - Get a character from the terminal
* $F005 - File Load Data - This is used to stream the data in, based on the File Mode 
* $F006 - Debug IO Enable - This enables debugging output for the I/O.
* $F007 - LCD status. If 1, the LCD is installed. If 0, it is not. This is read from the configuration file.

### RS232 interface

A serial RS232 interface can be connected UART0. You can connect one with or without flow
control. GPIO pin 0 connects to RX. GPIO pin 1 connects to TX. GPIO pin 2 connects to CTS
(optional RTS/CTS flow control). If you do not have CTS, you can use XON/XOFF, by
grounding GPIO pin 2. If the pin is grounded, to will detect XON/XOFF. If it is not
it will assume RTS/CTS flow control. 

### I/O Expansion

The original code used the Pico GPIO pins for the VIA emulation. Since the SD card was
added, we ran out of pins for the VIA emulation.

The I/O expansion connect through the I2C interface 0. It is address 0x27. It response
to the VIA addresses (Default $D000). The ports are as follows:

$D000 Port B data
$D001 Port A data
$D002 Port B data direction
$D003 Port A data direction

The rest of the other registers have not been implemented yet, but will be in the future.

You can add move I/O expansion modules to the I2C bus. You just need to make a different
address, and implement it in the code.

### LCD Display

The LCD display is connected I/O expansion, on Port B. This the same port that is used in
the Ben Eater Breadboard 6502. It uses the exact same code, except for the change of the
memory address.

## Extended I/O Protocol

### File Mode

Then interacting with the extended I/O You need to set the file mode. This determines what 
is sent or received from the data ports, and what the format is. The modes are as follows:

#### $00 - Idle

Resets all extended I/O and frees any memory associated with it. This can be used to put 
the system into a known state.

#### $FF - Send Filename

Send the data to $F003. This starts with a byte that is the length of the filename, followed
by the file name.

* byte 0 - Length of filename
* byte 1 - First byte
* ...
* byte n - nth byte

#### $FE - Send File Data

Send a block of data. 

Send the data to $F003. This starts with the high byte of length, and then the low byte of
the length. The number of bytes follows.

* byte 0 - High byte of length of data
* byte 1 - Low byte of length of data
* byte 2 - First byte
* ...
* byte n - nth byte

#### $FD - End Data

Save the data, and go back to idle state.

This take the data and sends it the data to the SD card, based on the filename specified. If
no filename is specified, or the file already exists, this will fail. An error code is
returned in $F003. If it is not 0, there is an error.

Error codes are as follows:

* 0 Success
* 1 Could not open file
* 4 No filename specified
* 5 Could not mount file system
* 6 File exists

#### $FC - Read File Data

Load the data from the specified filename or catalog.

Read the data from $F005. This starts with the high byte of length, and then 
the low byte of the length. The number of bytes follow.  If no filename is 
specified, or the file does not exist, this will fail. An error code is 
returned in $F003. If it is not 0, there is an error.

* byte 0 - High byte of length of data
* byte 1 - Low byte of length of data
* byte 2 - First byte
* ...
* byte n - nth byte

Error codes are as follows:

* 0 Success
* 1 Could not open file
* 4 No filename specified
* 5 Could not mount file system

#### $FB - Read Catalog From SD Card

Load a catalog of the SD card. 

Read the data from $F005. This starts with the high byte of length, 
and then the low byte of the length. The number of bytes follows.

* byte 0 - High byte of length of data
* byte 1 - Low byte of length of data
* byte 2 - First byte
* ...
* byte n - nth byte

Error codes are as follows:

* 5 Could not mount file system

#### $FA - Delete File From SD Card

Delete the specified file from the SD card

* 0 Success
* 4 No filename specified
* 5 Could not mount file system
* 6 File exists

### Module Used In The Project

The following modules are used in the project. I will list what they are, 
and give an Amazon link to the ports. You can substitute other modules, 
but you may need to modify the code or wiring.

#### Pi Pico

You can use any of the Pi Picos. (Pico, Pico W, Pico 2, Pico 2 W, Pico 2 H)

#### Pi Pico Breakout Board (Optional)

Freenove Breakout Board for Raspberry Pi Pico 1 2 W H WH, Terminal Block 
Shield HAT with Pin Header, GPIO Status LED

https://www.amazon.com/dp/B0BFB53Y2N?ref=ppx_yo2ov_dt_b_fed_asin_title

This a a breakout board that I used to allow connecting the Dupont jumper
wires to the different modules.

#### RS232 Interface

NOYITO TTL to RS232 Module TTL RS232 Male Female Mutual Conversion Module 
Serial Level Conversion to SP232 TTL Serial Port Communicates with RS232 
Level Device 15KV ESD Protect (Female)

http://amazon.com/dp/B07BJJC3R6?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1

<img src="images/ttl-rs232.jpg" width = "400px" >

This is an interface to connect a RS232 cable to the Pi Pico on UART0. The one that I am
using, has the flow control pins. You can connect the USB serial cable directly to the 
pins if you don't want to do the RS232. You can even just use the Pico USB serial.

#### SD Card Module

<img src="images/sd-card.jpg" width = "400px" >

HiLetgo 5pcs Micro SD TF Card Adater Reader Module 6Pin SPI Interface Driver Module with chip Level Conversion for Arduino UNO R3 MEGA 2560 Due

https://www.amazon.com/dp/B07BJ2P6X6?ref=ppx_yo2ov_dt_b_fed_asin_title

#### I/O Expansion Module

<img src="images/MCP23017.png" width = "400px" >


Waveshare MCP23017 IO Expansion Board I2C Interface Expands 16 I/O Pins Compatible with Both 3.3V and 5V Levels

https://www.amazon.com/dp/B07P2H1NZG?ref=ppx_yo2ov_dt_b_fed_asin_title


#### 1602A LCD Module

2 Pack HD44780 1602 LCD Display Module DC 5V 16x2 Character Blue Blacklight
Visit the LGDehome Store

https://www.amazon.com/dp/B079M3GWFG

<img src="images/LCD.jpg" width = "400px" >

#### 10K Trimmer Pot

Any potentiometer will do, as long as it is 10K. This is for the contrast
control on the LCD. If you are not using the LCD, you don't need to get
the potentiometer.

DGZZI 10k Trim Potentiometer 8PCS Breadboard Trim Potentiometer with Knob 
for Arduino Raspberry Pi 3386MP-103 3Pins 0-10K Ohm

https://www.amazon.com/dp/B07ZYVS2W6?ref=ppx_yo2ov_dt_b_fed_asin_title

#### Jumper Wires

Any kind of wires will do, depending on how you are hooking them up.

## Wiring Diagram

Below is the wire diagram for my unit. I also have one that only has the Pi
Pico, RS232 interface, and SD card. In this repo I also included the 3D 
models to attach the module with 1mm x 3 mm screws.

<img src="images/raspberry-pi-pico-wiring.png">

## Computer Unit

Here is a picture of the computer unit with the modules attached to the 
3D printed sub-straight.

<img src="images/pico-6502.jpg" width="50%">


You can interact with it via its USB serial connection.
