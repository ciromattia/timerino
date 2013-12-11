# TIMERINO

**Timerino** is a programmable darkroom enlarger timer built upon an Arduino microcontroller.
Currently it has _linear_ and _geometric_ (f/stop) exposure functions.

Currently supports 2 models which differs only in the display:
 * *DL-series* has a 7-Segment 4-number LCD display and a 10-led bar
 * *CMG-series* has a 16x2 matrix LCD display


## CREDITS
**Timerino** is made by Daniele Lucarelli and [Ciro Mattia Gonano](http://github.com/ciromattia).  
It's developed after Daniele Lucarelli's version on
[analogica.it](http://www.analogica.it/upgrade-timer-con-keypad-t6797.html)


## ISSUES
If you have some problems using **Timerino** please [file an issue here](https://github.com/ciromattia/timerino/issues/new).  
If you can fix an open issue, fork & make a pull request.  


## HARDWARE

Your grocery list to build Timerino is:  
- Arduino UNO/Duemilanove
- a power supply transformer 12V 1A
- a case
- 4x4 keypad
- buzzer
- pushbutton
- female 1/4" jack and pedal (optional)
- 220V relay
- 2 toggle switch
- a 10K resistor
- a 330 resistor
 
*Model DL002A specific components:*
- 7-Segment 4 number serial LCD display
- Grove LED Bar
 
*Model CMG001A specific components*
- 16x2 matrix LCD display
- 1 or 2 potentiometers

Follow the appropriate schema for your chosen model (Fritzing and JPEGs included) to
wire up the circuit.


## SOFTWARE

To build this sketch you need, besides core libraries, the Keypad.h
 library: http://playground.arduino.cc/code/Keypad


## COPYRIGHT

Copyright (c) 2013 Daniele Lucarelli and Ciro Mattia Gonano.  
**Timerino** is released under ISC LICENSE; see LICENSE.txt for further details.