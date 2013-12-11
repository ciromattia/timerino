/*
 Darkroom timer with programmable functions
 Supported models: DL002A CMG001A

 This sketch implements a darkroom timer with various modes.
 It's developed after Daniele Lucarelli's version on analogica.it
 (http://www.analogica.it/upgrade-timer-con-keypad-t6797.html)
 
 To build this sketch you need, besides core libraries, the Keypad.h
 library: http://playground.arduino.cc/code/Keypad

 The circuit:
 * Arduino UNO/Duemilanove
 * 4x4 keypad
 * buzzer
 * pushbutton
 * female 1/4" jack and pedal (optional)
 * 220V relay
 * 2 toggle switch
 * a 10K resistor
 * a 330 resistor
 
 Model DL002A specific components:
 * 7-Segment 4 number serial LCD display
 * Grove LED Bar
 
 Model CMG001A specific components
 * 16x2 matrix LCD display
 * 1 or 2 potentiometers
 
 The wiring schemes should come with this sketch, if not drop me a line.
 
 ### Pin Map Recap - DL002A ###
 D0: 7-Segment RX
 D1: 7-Segment TX
 D2: DI led bar
 D3: DCKI led bar
 D8: buzzer
 D9: progression switch (linear or f/stop) - maybe superfluous?
 D10: Relay signal
 D11, D12, A0-A6: 4x4 keypad
 D13: main button (pedal/pushbutton)
 
 ### Pin Map Recap - CMG001A ###
 D0: 
 D1: 
 D2-D7: LCD
 D8: buzzer
 D9: progression switch (linear or f/stop) - maybe superfluous?
 D10: Relay signal
 D11, D12, A0-A6: 4x4 keypad
 D13: main button (pedal/pushbutton)

 created  11 Nov 2013
 by Daniele Lucarelli
 adapted with LCD 16x2 display 5 Dec 2013
 by Ciro Mattia Gonano <ciromattia@gmail.com>

 TODO: allow setting a buzzer interval for linear countdown (for test strips)
 TODO: implement eventListener for keyboard to manage key HOLDing
 */

/* set the model accordingly:
  - 0: DL-series (7segment LCD)
  - 1: CMG-series (16x2 matrix LCD)
*/
#define MYMODEL 0

#include <Keypad.h>
#include <avr/eeprom.h>

#if 0 == MYMODEL
  // defines for LED Bar. They're set on pins 2-3, change accordingly
  // if you wired the LED Bar to another location
  #define LEDBar_DDRData  DDRD
  #define LEDBar_DDRClk   DDRD
  #define LEDBar_PORTData PORTD
  #define LEDBar_PORTClk  PORTD
  #define LEDBar_BITData  0x04
  #define LEDBar_BITClk   0x08
  #define CmdMode         0x0000  //Work on 8-bit mode
  #define ON              0x00ff  //8-bit 1 data
  #define SHUT            0x0000  //8-bit 0 data
  // end defines for LCD Bar
  #include <SoftwareSerial.h>
  SoftwareSerial Serial7Segment(0, 5);
  const byte lcdrxpin = 5;
  const byte brightness = 16;  // change this to control 7-Segment brightness
#elif 1 == MYMODEL
  #include <LiquidCrystal.h>
  LiquidCrystal lcd(2, 3, 4, 5, 6, 7);
#endif

// define wired pins
const byte buzzer = 8; // buzzer pin
const byte mainbtn = 13; // main button (pushbutton/pedal) pin
const byte relay = 10; // relay pin
const byte selector = 9; // progression switch pin
// define tone and delays
const int tone_up = 600;
const int tone_down = 300;
const int scrollTime = 250;
const int waitTime = 800;


/************* YOU SHOULD NOT TOUCH ANYTHING BELOW THIS LINE *************/
// keypad
const byte ROWS = 4; // keypad: 4 rows
const byte COLS = 4; // keypad: 4 cols
// modes
const byte MODLINFREE = 0b00000;
const byte MODLINUP = 0b00010;
const byte MODLINDOWN = 0b00100;
const byte MODLINDDS = 0b00110;
const byte MODFSTFREE = 0b00001;
const byte MODFSTPREC = 0b00011;
const byte MODFSTTEST = 0b00101;
const byte MODFSTDOWN = 0b00111;

// Multiplier formula for f/stop progression (thanks to Gergio)
// mult = 2^(1/precision)

// Variables
int i = 0;
int time = 0;
int time_succ;
int appo_time;
int time_countdown;
int time_dds;
int time_fsttest;
int time_fstdown;
byte timer_mode = MODLINFREE; // timer mode (0 = lin.A, 1 = lin.B, 2 = lin.C, 3 = lin.D, 4 = fst.A, 5 = fst.B, 6 = fst.C, 7 = fst.D)
long last_time = 0;
long errlet = 0;
int btnstatus = 0;         // main button status
int lastbtnstatus = LOW;     // last main button status
int selstatus = LOW;
int lastselstatus = LOW;
boolean running = false;
boolean btnhigh = false;
boolean firstpress = true;
boolean mute = false;
float mult = 1.0;
int precis = 1;
char _buffer[17];

/* init matrix keypad */
char keys[ROWS][COLS] = {
  {1,2,3,'A' },
  {4,5,6,'B' },
  {7,8,9,'C' },
  {'*','0','#','D' }
};
byte rowPins[ROWS] = {A4, A5, 11, 12}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {A0, A1, A2, A3}; //connect to the column pinouts of the keypad
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );


/***************** DISPLAY functions *****************/
void beep(int this_tone, int duration) {
  if (!mute)
    tone(buzzer, this_tone, duration);
}
void metronome() {
  if (running && time%10 == 0) {
    beep(tone_down, 50);
  }
}

#if 0 == MYMODEL  // add LED Bar functions
  void LEDBar_set_LED_Index(unsigned int index) {
    unsigned char i;
    LEDBar_send16bitData(CmdMode); 
    for (i=0;i<12;i++) {
      LEDBar_send16bitData(index&0x0001 ? ON : SHUT);
      index= index>>1;
    }
    LEDBar_latchData();
  }
  void LEDBar_send16bitData(unsigned int data) {
     for (unsigned char i=0;i<16;i++) {
       data&0x8000 ? LEDBar_PORTData |= LEDBar_BITData : LEDBar_PORTData &=~ LEDBar_BITData;
       LEDBar_PORTClk ^= LEDBar_BITClk;
       data <<= 1;
     }
  }
  void LEDBar_latchData(void) {
      LEDBar_PORTData &=~ LEDBar_BITData;
      delayMicroseconds(10);
      for(unsigned char i=0;i<8;i++)
        LEDBar_PORTData ^= LEDBar_BITData;
  }
#endif

void setup_display() {
  #if 0 == MYMODEL
    LEDBar_set_LED_Index(0b000001111111111);
    // Setup 7SegmentDisplay
    Serial7Segment.begin(9600); // Connect to 7-Segment display in serial mode
    Serial7Segment.write('v'); // Display reset - brings cursors to the first char
    Serial7Segment.write(0x7A);  // Brightness control command
    Serial7Segment.write((byte) brightness);
    Serial7Segment.write('A');
    LEDBar_set_LED_Index(0b000000111111111);
    delay(scrollTime);
    Serial7Segment.write('N');
    LEDBar_set_LED_Index(0b000000011111111);
    delay(scrollTime);
    Serial7Segment.write('A');
    LEDBar_set_LED_Index(0b000000001111111);
    delay(scrollTime);
    Serial7Segment.write('L');
    LEDBar_set_LED_Index(0b000000000111111);
    delay(scrollTime);
    Serial7Segment.write("NALO");
    LEDBar_set_LED_Index(0b000000000011111);
    delay(scrollTime);
    Serial7Segment.write("ALOG");
    LEDBar_set_LED_Index(0b000000000001111);
    delay(scrollTime);
    Serial7Segment.write("LOGI");
    LEDBar_set_LED_Index(0b000000000000111);
    delay(scrollTime);
    Serial7Segment.write("OGIC");
    LEDBar_set_LED_Index(0b000000000000011);
    delay(scrollTime);
    Serial7Segment.write("GICA");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b00001000);
    LEDBar_set_LED_Index(0b000000000000001);
    delay(scrollTime);
    Serial7Segment.write("ICAI");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b00000100);
    LEDBar_set_LED_Index(0b000000000000000);
    delay(scrollTime);
    Serial7Segment.write("CAIT");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b00000010);
    LEDBar_set_LED_Index(0b000001000000000);
    delay(scrollTime);
    LEDBar_set_LED_Index(0b000000000000000);
    delay(scrollTime);
    LEDBar_set_LED_Index(0b000001000000000);
    delay(scrollTime);
    Serial7Segment.write("AIT ");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b00000001);
    LEDBar_set_LED_Index(0b000000000000000);
    delay(scrollTime);
    Serial7Segment.write("IT  ");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b01000000);
    LEDBar_set_LED_Index(0b000001000000000);
    delay(scrollTime);
    Serial7Segment.write("T  0");      
    LEDBar_set_LED_Index(0b000000000000000);
    delay(scrollTime);
    Serial7Segment.write("  00");
    LEDBar_set_LED_Index(0b000001000000000);
    delay(scrollTime);
    Serial7Segment.write(" 000");
    LEDBar_set_LED_Index(0b000000000000000);  
    delay(scrollTime);
    reset();
    delay(scrollTime);
    delay(scrollTime);
    LEDBar_set_LED_Index(0b000001000000000);
  #elif 1 == MYMODEL
    lcd.begin(16, 2);
    lcd.clear();
    sprintf(_buffer,"%s","ANALOGICA.IT    ");
    lcd.setCursor(16,1);
    lcd.autoscroll();
    for (int thisChar=0; thisChar < 16; thisChar++) {
      lcd.print(_buffer[thisChar]);
      delay(scrollTime);
    }
    lcd.noAutoscroll();
  #endif
}

void say_reset() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("0000");
    Serial7Segment.write(0x77);
    Serial7Segment.write(0b00000100);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.clear();
  #endif
}
void say_clearprecis() {
  #if 0 == MYMODEL
    say_reset();
  #elif 1 == MYMODEL
    lcd.setCursor(12,1);
    lcd.print("     ");
  #endif
}
void say_cleartime() {
  #if 0 == MYMODEL
    say_reset();
  #elif 1 == MYMODEL
    lcd.setCursor(0,1);
    lcd.print("     ");
  #endif
}
void say_time() {
  say_cleartime();
  #if 0 == MYMODEL
    Serial7Segment.write(0x77); //Codice per invio funzioni specifiche
    Serial7Segment.write(0b00000100); //Accendo la virgola per 1 cifra decimale
    sprintf(_buffer, "%04i", time); //Converte il tempo in una striga con gli zeri in testa
    Serial7Segment.write(_buffer); // Stampo il tempo  
  #elif 1 == MYMODEL
    float ftime = time > 0 ? (float)time / 10 : 0.0;
    dtostrf(ftime, 3, 1, _buffer);
    lcd.setCursor(0,1);
    lcd.print(_buffer);
  #endif
}
void say_prec() {
  say_clearprecis();
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    switch (precis) {
      case 1:
        LEDBar_set_LED_Index(0b000001000000000);
        Serial7Segment.write("1-1 ");
        break;
      case 2:
        LEDBar_set_LED_Index(0b000000100000000);
        Serial7Segment.write("1-2 ");
        break;
      case 3:
        LEDBar_set_LED_Index(0b000000010000000);
        Serial7Segment.write("1-3 ");
        break;
      case 4:
        LEDBar_set_LED_Index(0b000000001000000);
        Serial7Segment.write("1-4 ");
        break;
      case 6:
        LEDBar_set_LED_Index(0b000000000100000);
        Serial7Segment.write("1-6 ");
        break;
      case 8:
        LEDBar_set_LED_Index(0b000000000010000);
        Serial7Segment.write("1-8 ");
        break;
      case 12:
        LEDBar_set_LED_Index(0b000000000001000);
        Serial7Segment.write("1-12");
        break;
      case 24:
        LEDBar_set_LED_Index(0b000000000000100);
        Serial7Segment.write("1-24");
        break;
      case 32:
        LEDBar_set_LED_Index(0b000000000000010);
        Serial7Segment.write("1-32");
        break;
      case 48:
        LEDBar_set_LED_Index(0b000000000000001);
        Serial7Segment.write("1-48");
        break;
    }
  #elif 1 == MYMODEL
    switch(precis) {
      case 1: lcd.setCursor(15,1); lcd.print("1");   break;
      case 2: lcd.setCursor(13,1); lcd.print("1/2"); break;
      case 3: lcd.setCursor(13,1); lcd.print("1/3"); break;
      case 4: lcd.setCursor(13,1); lcd.print("1/4"); break;  
      case 6: lcd.setCursor(13,1); lcd.print("1/6"); break;
      case 8: lcd.setCursor(13,1); lcd.print("1/8"); break;
      case 12: lcd.setCursor(12,1); lcd.print("1/12"); break;
      case 24: lcd.setCursor(12,1); lcd.print("1/24"); break;
      case 32: lcd.setCursor(12,1); lcd.print("1/32"); break;  
      case 48: lcd.setCursor(12,1); lcd.print("1/48"); break;
    }
  #endif
}

void say_free() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("LINE");
    beep(tone_up, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("Free Mode       ");
    beep(tone_up, 200);
    say_clearprecis();
  #endif
  say_cleartime();
  eeprom_write_byte(0, MODLINFREE);
}
void say_up(){
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("Up  ");
    beep(tone_up, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("Stopwatch       ");
    beep(tone_up, 200);
    say_clearprecis();
  #endif
  say_time();
  eeprom_write_byte(0, MODLINUP);
}
void say_down(){
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("DouN");
    beep(tone_down, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("Countdown       ");
    beep(tone_down, 200);
    say_clearprecis();
  #endif
  say_time();
  eeprom_write_byte(0, MODLINDOWN);
}
void say_dds() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("DDS ");
    beep(tone_down, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("DDS Mode        ");
    beep(tone_down, 200);
    say_clearprecis();
  #endif
  say_time();
  eeprom_write_byte(0, MODLINDDS);
}
void say_fstop(){
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("F-ST");
    beep(tone_up, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("F/stop Free Mode");
    beep(tone_up, 200);
    say_cleartime();
  #endif
  say_prec();
  eeprom_write_byte(0, MODFSTFREE);
}
void say_precis() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("pREC");
    beep(tone_down, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("F/Stop Precision");
    beep(tone_down, 200);
    say_cleartime();
  #endif
  say_prec();
  eeprom_write_byte(0, MODFSTPREC);
}
void say_test_strip() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("pROS");
    beep(tone_down, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("F/stop TestStrip");
    beep(tone_down, 200);
    say_prec();
  #endif
  say_time();
  eeprom_write_byte(0, MODFSTTEST);
}
void say_fstopdown() {
  #if 0 == MYMODEL
    Serial7Segment.write('v');
    Serial7Segment.write("CFST");
    beep(tone_down, 200);
    delay(500);
  #elif 1 == MYMODEL
    lcd.setCursor(0,0);
    lcd.print("F/stop Countdown");
    beep(tone_down, 200);
    say_prec();
  #endif
  say_time();
  eeprom_write_byte(0, MODFSTDOWN);
}
void say_timermode() {
  switch (timer_mode) {
    case MODLINFREE: say_free();       break;
    case MODLINUP:   say_up();         break;
    case MODLINDOWN: say_down();       break;
    case MODLINDDS:  say_dds();        break;
    case MODFSTFREE: say_fstop();      break;
    case MODFSTPREC: say_precis();     break;
    case MODFSTTEST: say_test_strip(); break;
    case MODFSTDOWN: say_fstopdown();  break;
    default: break;
  }
}


/************ MODE FUNCTIONS ********/
void off() {
  // no action taken
}
void countdown() {
  if (time > 0 && (millis() - last_time >= 100)) {
    last_time = millis();
    time--;
    metronome();
    say_time();
    if (time == 0) {
      // if time reached 0, shut down the relay and reset the function
      digitalWrite(relay, LOW);
      running = false;
      time = appo_time;
      beep(tone_down, 75);
      delay(150);
      beep(tone_down, 75);
      delay(150);
      beep(tone_down, 150);
      if (timer_mode == MODLINDDS) {
        firstpress = true;
      }
      say_time();
    }
  }
}
void stopwatch() {
  if (millis() - last_time >= 100) {
    last_time = millis();
    time++;
    metronome();
    say_time();
  }
}
void test_strip() {
  if (millis() - last_time >= 100) {
    last_time = millis();
    time++;
    say_time();
    if (time == time_succ) {
      beep(tone_up, 150);    
      mult = pow(2.0,(1.0/precis));  
      time_succ = int(float(time) * mult);
    }
    if (time_succ - time <= 3) {
      beep(tone_up, 50);
    }
  }
}
void reset() {
  say_reset();
  time = 0;
  appo_time = 0;
}

void load_eeprom() {
  timer_mode = eeprom_read_byte(0);
  // if mode is not valid set the first one and return
  if ((timer_mode & 0b11000) > 0) {
    timer_mode = MODLINFREE;
    return;
  }
  time_countdown = eeprom_read_word((uint16_t *)1);  // 2 byte
  time_dds = eeprom_read_word((uint16_t *)3);        // 2 byte
  time_fsttest = eeprom_read_word((uint16_t *)5);    // 2 byte
  time_fstdown = eeprom_read_word((uint16_t *)7);    // 2 byte
  precis = eeprom_read_word((uint16_t *)9);          // 2 byte
}

void init_timermode() {
  switch (timer_mode) {
    case MODLINDOWN:
      time = time_countdown;
      break;
    case MODLINDDS:
      time = time_dds;
      firstpress = true;
      break;
    case MODFSTTEST:
      time = time_fsttest;
      break;
    case MODFSTDOWN:
      time = time_fstdown;
      break;
    case MODLINFREE:
    case MODLINUP:
    case MODFSTFREE:
    case MODFSTPREC:
    default:
      break;
  }
  say_timermode();
}


/*** Keypad management functions ***/
//void keypadEvent(KeypadEvent key) {}
void read_key() {
  int key = keypad.getKey();
  if (key == NO_KEY) return;
  switch (key) {
    case 'A':  // Free mode
      reset();
      timer_mode = MODLINFREE | (timer_mode & 0b00001);
      init_timermode();
      break;
    case 'B':
      reset();
      timer_mode = MODLINUP | (timer_mode & 0b00001);
      init_timermode();
      break;
    case 'C':
      reset();
      timer_mode = MODLINDOWN | (timer_mode & 0b00001);
      init_timermode();
      break;
    case 'D':
      reset();
      timer_mode = MODLINDDS | (timer_mode & 0b00001);
      init_timermode();
      break;

    case '*': // reset for functions that need that
      if (timer_mode == MODFSTDOWN) {
        // Step down
        mult = pow(2.0, (1.0/precis));
        time = int(float(time) / mult);
        if (time <= 1)
          time = 1;
        appo_time = time;
        say_time();
      }
      break;

    case '#': // Abilita disabilita suono su alcune modeioni
      //mute = (timer_mode == MODLINUP || timer_mode == MODLINDOWN || timer_mode == MODLINDDS || timer_mode == MODFSTTEST);
      if (timer_mode == MODFSTDOWN) {
        // Step up
        mult = pow(2.0, (1.0/precis));
        time = int(float(time) * mult);
        if (time >= 9999)
          time = 9999;
        appo_time = time;
        say_time();
      }
      break;

    default: 
      if (timer_mode == MODLINUP) { // stopwatch does not accept numbers input
        say_up();
        appo_time = time;
        say_time();
      } else if (timer_mode == MODFSTPREC) { // Imposta precisione decodifico i numeri
        switch (key) {
          case 1:  precis = 1; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 2:  precis = 2; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 3:  precis = 3; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 4:  precis = 4; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 5:  precis = 6; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 6:  precis = 8; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 7:  precis = 12; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 8:  precis = 24; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case 9:  precis = 32; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          case '0':  precis = 48; say_prec(); eeprom_write_word((uint16_t *)9, precis); break;
          default: break;
        }
      } else {
        if (time < 1) {
          time = key == '0' ? 0 : key;
        } else {
          time *= 10;
          if (time > 9999)
            time %= 10000;
          if (key != '0')
            time += key;
        }
        appo_time = time;
        say_time();
    }
  }
}

void setup() {
  //Serial.begin(9600);
  //Serial.println(">>> Debug <<<");
  
#if 0 == MYMODEL
  // init LED Bar
  LEDBar_DDRData |= LEDBar_BITData;
  LEDBar_DDRClk |= LEDBar_BITClk;
  pinMode(lcdrxpin, OUTPUT);
#endif

  /* ensure analog pins are set to INPUT */
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A4, INPUT);
  pinMode(A5, INPUT);
  pinMode(buzzer, OUTPUT);      // Buzzer pin in Output
  pinMode(relay, OUTPUT);      // Relé pin in Output
  pinMode(mainbtn, INPUT);       // Pedale/pulsante pin in Input
  pinMode(selector, INPUT);       // Selettore modalità (lineare/fstop) in Input

  // read from EEPROM stored values
  load_eeprom();
  
  // Setup display
  setup_display();

  // init fstop/linear mode
  selstatus = digitalRead(selector);
  lastselstatus = selstatus;
  if (selstatus == HIGH) {
    bitSet(timer_mode,0); // f/stop mode
  } else {
    bitClear(timer_mode,0); // linear mode
  }

  // TODO: add event listener for keypad
  //keypad.addEventListener(keypadEvent);

  // cleanup and get ready to start
  delay(waitTime);
  say_reset();
  init_timermode();
  errlet = millis();
  digitalWrite(relay, LOW); // shut down the relay
}

void loop() {
  if (millis() - errlet <= 20) {
  } else {
    btnstatus = digitalRead(mainbtn);
    if (btnstatus != lastbtnstatus) {
      // main button has been toggled
      if (btnstatus == HIGH) {
        // main button has been pressed
        btnhigh = true; 
      }
      lastbtnstatus = btnstatus;
    }
    errlet = millis();
  }

  selstatus = digitalRead(selector);
  if (selstatus != lastselstatus) {
    // mode change
    lastselstatus = selstatus;
    timer_mode = (selstatus == HIGH) ? MODFSTFREE : MODLINFREE;
    say_timermode();
  }

  if (running && btnhigh) {
    // shut down the relay and pause the timer
    digitalWrite(relay, LOW);
    btnhigh = false;
    running = false;
    if (timer_mode == MODLINDDS && firstpress) {
      firstpress = false;
    }
  } else if (running && !btnhigh) {
    switch (timer_mode) {
      case MODLINFREE:
        off();
        break;
      case MODLINUP:
        stopwatch();
        break;
      case MODLINDOWN:
        countdown();
        break;
      case MODLINDDS:
        firstpress ? off() : countdown();
        break;
      case MODFSTFREE:
        off();
        break;
      case MODFSTPREC:
        off();
        break;
      case MODFSTTEST:
        test_strip();
        break;
      case MODFSTDOWN:
        countdown();
        break;
    }
  } else if (!running && btnhigh) { // not running
    btnhigh = false;
    switch (timer_mode) {
      case MODLINFREE:
      case MODFSTFREE:
        digitalWrite(relay, HIGH); // powerup the relay
        running = true;
        off();
        break;
      case MODLINUP:
        digitalWrite(relay, HIGH); // powerup the relay
        running = true;
        last_time = millis(); // reset timer counter
        stopwatch();
        break;
      case MODLINDOWN:
        time_countdown = appo_time;
        eeprom_write_word((uint16_t *)1, time_countdown);
        if (time > 0) {
          digitalWrite(relay, HIGH); // powerup the relay
          running = true;
          last_time = millis(); // reset timer counter
          countdown();
        }
        break;
      case MODLINDDS:
        time_dds = appo_time;
        eeprom_write_word((uint16_t *)3, time_dds);
        if (time > 0) {
          digitalWrite(relay, HIGH); // powerup the relay
          running = true;
          if (firstpress == true) {
            off();
          } else {
            last_time = millis(); // reset timer counter
            countdown();
          }
        }
        break;
      case MODFSTPREC:
        off();
        break;
      case MODFSTTEST:
        time_fsttest = appo_time;
        eeprom_write_word((uint16_t *)5, time_fsttest);
        digitalWrite(relay, HIGH); // powerup the relay
        running = true;
        last_time = millis(); // reset timer counter
        if (time < 10)
          time = 10;
        time_succ = time;
        time = 0;
        test_strip();
        break;
      case MODFSTDOWN:
        time_fstdown = appo_time;
        eeprom_write_word((uint16_t *)7, time_fstdown);
        if (time > 0) {
          digitalWrite(relay, HIGH); // powerup the relay
          running = true;
          last_time = millis(); // reset timer counter
          countdown();        
        }
        break;
    }
  } else { // not running nor button released, so read keypad input
    read_key();
  }
}
