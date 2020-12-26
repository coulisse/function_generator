/*******************************************************************************
Title: Function generator
Author: Corrado Gerbaldo - IU1BOW

-------------------------------------------------------------------------------

*******************************************************************************/

/*------------------------------------------------------------------------------
Includes
------------------------------------------------------------------------------*/
#include <LiquidCrystal.h>

/*------------------------------------------------------------------------------
Constants
------------------------------------------------------------------------------*/
//AD9850
const int W_CLK =  3;    // Giallo
const int FQ_UD = 11;    // Verde   *
const int DATA  = 12;    // Viola   *
const int RESET = 13;    // Bianco


//frequency
const double FREQ_CORRECTION = 400; //correction in hz
#define btnSELECT   0
#define btnLEFT     1
#define btnUP       2
#define btnDOWN     3
#define btnRIGHT    4
#define btnNONE     6
//const int rs = 12, en = 11, d4 = 5, d5 = 4, d6 = 3, d7 = 2;
const int rs = 8, en = 9, d4 = 4, d5 = 5, d6 = 6, d7 = 7;    
const byte   C_START_STEP = 3;
const double C_MAX_FREQ = 50000000;
const double C_START_FREQ = 14000000;


/*------------------------------------------------------------------------------
Macros
------------------------------------------------------------------------------*/
#define pulseHigh(pin) {digitalWrite(pin, HIGH); digitalWrite(pin, LOW); }


/*------------------------------------------------------------------------------
Global variables
------------------------------------------------------------------------------*/
// initialize the library by associating any needed LCD interface pin
// with the arduino pin number it is connected to
int lcd_key     = 0;
int adc_key_in  = 0;

LiquidCrystal lcd(rs, en, d4, d5, d6, d7);

/*==============================================================================
Functions
==============================================================================*/

/*------------------------------------------------------------------------------
AD9850
transfers a byte, a bit at a time, LSB first to the 9850 via serial DATA line
------------------------------------------------------------------------------*/
void tfr_byte(byte data)
{
 for (int i=0; i<8; i++, data>>=1) {
   digitalWrite(DATA, data & 0x01);
   pulseHigh(W_CLK);   //after each bit sent, CLK is pulsed high
 }
}

// read the buttons
byte read_LCD_buttons() {
   adc_key_in = analogRead(0);  
   if (adc_key_in > 1500) return btnNONE; 
   if (adc_key_in < 50)   return btnRIGHT;  
   if (adc_key_in < 195)  return btnUP; 
   if (adc_key_in < 380)  return btnDOWN; 
   if (adc_key_in < 600)  return btnLEFT; 
   if (adc_key_in < 900)  return btnSELECT;    
   return btnNONE;  
}


/*
 * Format an unsigned long (32 bits) into a string in the format
 * "23,854,972".
 *
 * The provided buffer must be at least 14 bytes long. The number will
 * be right-adjusted in the buffer. Returns a pointer to the first
 * digit.
 * 
 * 
 * from: https://arduino.stackexchange.com/questions/28603/the-most-effective-way-to-format-numbers-on-arduino
 */
void lcd_print_freq(unsigned long val) {

    char s[14]; 
    byte cnt=0;
    char *p = s + 13;
    *p = '\0';
    do {
        cnt++;
        if ((p - s) % 4 == 2) {
            *--p = '.';
            cnt++;
        }
        *--p = '0' + val % 10;
        val /= 10;
    } while (val);

    for (byte i=6; i<16-cnt; i++) {
      lcd.setCursor(i,0);
      lcd.print(" ");
    }
    
    lcd.setCursor(16-cnt,0);
    lcd.print(p);
    
    return p;    
}

/*------------------------------------------------------------------------------
AD9850
Frequency send to AD9850.

  
Calculate the frequency using the selected frequency and, sum the "correction"
Then  <sys clock> * <frequency tuning word>/2^32

------------------------------------------------------------------------------*/
void sendFrequency(double f) {

    Serial.print("requested frequency: ");
    Serial.println(f);

    int32_t freq = FREQ_CORRECTION + f * 4294967295/125000000;  // note 125 MHz clock on 9850
    Serial.print("send to AD9850.....: ");
    Serial.println(freq); 

    for (int b=0; b<4; b++, freq>>=8) {
     tfr_byte(freq & 0xFF);
    }
    tfr_byte(0x000);   // Final control byte, all 0 for 9850 chip
    pulseHigh(FQ_UD);  // Done!  Should see output
    
    lcd_print_freq(f);   
  
};



/******************************************************************************
SETUP
*******************************************************************************/
void setup() {

  //set serial
  Serial.begin(9600);

  //configure lcd
  lcd.begin(16, 2);
  lcd.setCursor(5, 0);
  lcd.print("IU1BOW");
  delay(700);
  lcd.setCursor(0, 1);
  lcd.print("Func. generator");
  delay(2000);
  lcd.clear();

  // configure arduino data pins for output to AD9850 DDS
  pinMode(FQ_UD, OUTPUT);
  pinMode(W_CLK, OUTPUT);
  pinMode(DATA, OUTPUT);
  pinMode(RESET, OUTPUT);

  //enable AD9850 DDS
  pulseHigh(RESET);
  pulseHigh(W_CLK);
  pulseHigh(FQ_UD);  // this pulse enables serial mode - Datasheet page 12 figure 10

  Serial.println("READY");
  lcd.print("Freq:");
  sendFrequency(C_START_FREQ);
  lcd.setCursor(0,1);
  lcd.print("Step:");
  change_step(C_START_STEP,0);

};

//convert step to pow
long step2pow(byte step)  {
  return round(pow(10,step));;
};

//increase or decrease the step and print to lcd
byte change_step(byte step, short var) {
    step=step+var;
    Serial.print("Step: ");
    Serial.println(step2pow(step));      
    lcd.setCursor(6,1);
    lcd.print("   ");
    lcd.setCursor(6,1);
    if (step < 3) {
      lcd.print(step2pow(step));
      lcd.setCursor(10,1);
      lcd.print("Hz ");
    } else if (step > 5 ) {
      lcd.print(step2pow(step)/1000000);
      lcd.setCursor(10,1);
      lcd.print("MHz");
    } else {
      lcd.print(step2pow(step)/1000);
      lcd.setCursor(10,1);
      lcd.print("kHz");     
    }
    return step;
};

/******************************************************************************
MAIN LOOP
*******************************************************************************/
void loop() {

  static double freq = C_START_FREQ;
  static byte   step = C_START_STEP;
  
  byte button;
  button=read_LCD_buttons();

  if (button==btnUP){
    delay(250); 
    freq=freq+(step2pow(step));
    if (freq > C_MAX_FREQ) {
      freq = C_MAX_FREQ;
    };
    sendFrequency(freq);
    
  }else if (button==btnDOWN) {
    delay(250); 
    freq=freq-(step2pow(step));
    if (freq < 1) {
      freq = 1;
    };
    sendFrequency(freq);

  } else if (button==btnLEFT) { 
    delay(300); 
    if (step > 0) {
      step=change_step(step,-1);
    }
   
  } else if (button==btnRIGHT) { 
    delay(300); 
    if (step < 6) {
      step=change_step(step,+1);   
    }
  }

};
