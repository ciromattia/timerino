/*
 Timerino.h
 This file contains a number of defines to build Timerino software as you like it!
 Every define should come with an explicative comment, if not so clear please refer
 to the official documentation.
*/

// set the model accordingly:
//  - 0: DL-series (7segment LCD)
//  - 1: CMG-series (16x2 matrix LCD)
#define MYMODEL 0

// change to LOW the following if you want the relay be (de)activated on
// pushbutton *release* instead of *press*
#define ACTIONSIGNAL LOW

// Set to 1 if you want last mode and times used be preserved when you
// power off the timer
#define EPR 0

