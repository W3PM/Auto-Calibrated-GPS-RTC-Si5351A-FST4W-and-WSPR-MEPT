/*
  Si5351 bandswitched WSPR and FST4W source designed for 2200 - 10 meters utilizing
  a 1pps from an RTC module for timing and frequency adjustment.

  This sketch uses a frequency resolution down to 0.01 Hz to provide optimum
  reolution for the FST4W/WSPR tone separation specification. This limits
  the upper frequency to 40 MHz.

  For additional time and frequency accuracy, the DS3231 RTC crystal oscillator
  aging offset may adjusted. Refer to calibration instructions found elsewhere in
  this sketch.

  This sketch is hardware compatible with the W3PM Multifunction Project found in
  the May/June QEX magazine.

  Permission is granted to use, copy, modify, and distribute this software
  and documentation for non-commercial purposes.

  Copyright (C) 2020,  Gene Marcus W3PM GM4YRE

  12 October, 2020

  30 December 2021  v1a  Newer baatches of Si5351's default to spread spectrum if
                         not set to disable. This version includes code to disable
                         the spread spectrum mode.

  19 April 2022     v2    Improved ulti-mode multi-band scheduling                       

  The code was sucessfully compiled using Arduino 1.8.13

   NOTE! Uses library SSD1306Ascii by Bill Greiman
        Load from: Sketch > Include Library > Manage Libraries

   EEPROM address (unused addresses maintain compatibility for hardware re-use with past projects)
         10    Band
         62    TX interval
         66    TX offset
         78    TX hop on/off
         82    TX mode


  ---------------------------------------------------------------------------------------------------------------
  Five pushbuttons are used to control the functions.
  Pushbutton pin allocations follow:
  ---------------------------------------------------------------------------------------------------------------
         WSPR/FST4W     SET TIME                  SET DATE
  PB1   N/A            Time sync / Set Hour*     Set Day*
  PB2   ON / OFF       Set Minute*               Set Month*
  PB3   Band Select    Save and exit             Set Year* / Save data and exit
  PB4   N/A            *Hold to change time      *Hold to change date

          TX INTERVAL                         SELECT MODE
  PB1   Increase TX interval                Depress to select mode
  PB2   N/A                                 N/A
  PB3   Save data and exit                  Save data and exit
  PB4   Decrease TX interval                Depress to select mode

            SET OFFSET                         TX FREQ HOP
  PB1   increase offset                     "YES"
  PB2   N/A                                 N/A
  PB3   Save data and exit                  Save data and exit
  PB4   Decrease offset                     "NO"

  MENU - exit function and return to menu when not transmitting


  ------------------------------------------------------------------------
  Nano Digital Pin Allocations follow:
  ------------------------------------------------------------------------
  D0/RX
  D1
  D2  RTC 1pps input
  D3
  D4
  D5  2.5 MHz input from Si5351 CLK0 pin
  D6  pushButton 1
  D7  pushButton 2
  D8  pushButton 3
  D9  pushButton 4
  D10 MENU
  D11
  D12
  D13

  A0/D14 XmitLED
  A1/D15 MinLED
  A2/D16 SecLED
  A3/D17
  A4/D18 Si5351 & OLED SDA & RTC
  A5/D19 Si5351 & OLED SCL & RTC

*/

/*******************************************************************************************

                                 OPTIONAL RTC CALIBRATION

 *******************************************************************************************


   While the default RTC is already very accurate its accuracy can be pushed even higher
   by adjusting its aging offset register.

   For normal operation calibration is not required. The default 2 parts per million accuracy
   of the RTC will result in an uncertainty of less than +/- 30 Hz on 20 meters.

   The time will require synchronization every 7 – 10 days without calibration.
   The re-synchronization timeframe can be stretched to a month or more with calibration.

   There are three options to perfom DS3231 RTC aging offset calibration:
   1) Measure and adjust the 32 kHz output
   2) Set VFO frequency to 10 MHz and use the frequency delta as the aging offset (not
      practical in this WSPR only version)
   3) Track the time over a period of days.

   A calibration function is provided view the current aging offset and enter a new offset.
   To enter the calibration funtion, hold down pushbutton 4 and invoke a reset. Invoke a
   reset to exit the calibration function.

   IMPORTANT NOTE: When using methods 2 and 3 above, any change will not take place until
                   the auto-calibration algorithm calculates the correction factor for the
                   Si5351A’s 25 MHz clock and the DS3231's temperature compensation algorithm
                   performs its calculation. This may take a minute or two before any
                   change appears. Additionally, the auto-calibration routine uses an
                   averaging alogithm over a few minutes to reduce the effects of 1pps gate
                   time jitter. Any adjustment (other than the 32 KHz method) will be an
                   iterative process and will take some time.


   32 kHz & FREQUENCY COUNTER METHOD:
   Attach a 10 kohm pull-up resistor from the VCC to the 32K pins of the RTC. Attach a high
   resolution accurate counter from the 32K pin to ground.
   Enter the calibration function (see above) and use pushbuttons 1 and 2 to adjust the
   frequency as closely to 32768 Hz as possible.


   VFO & FREQUENCY COUNTER METHOD:
   Set the VFO to 10 Mhz and measure the frequency from CLK1 with an accurate frequency
   counter. Measure to the nearest Hz and subract this number from 10 MHz. This will be
   the aging offset. Enter the calibration function (see above). If this is the first
   time you performed a calibration you should see "CF = 0" on the display. If not, add
   the measured aging factor to the displayed number.  Use pushbuttons 1 and 2 to set the
   device to the aging factor. Invoke a reset to exit.

   Note: Per the DS3231 datasheet at  +25°C,  one LSB (in the offset register) typically
         provides about 0.1ppm change in frequency. At 10MHx 0.1ppm equates to 1 Hz.
         Theoretically, the process described above should produce get you right on
         target. Practically, I found that I had to go back and forth to obtain the
         greatest accuracy. Allow the system a few minutes to stabilize before making
         any adjustments (refer to the note above).


   TIME TRACKING METHOD:
   Sychronize the device's displayed time with an accuate time source such as WWV.
   After a few days compare the displayed time the reference time source. The
   following formula should bring you close to nominal:

     aging offset = (change in seconds / (86700 x number of days)) x 10^7

   Use a positive integer if the clock is running fast and a negative integer if it is
   running slow.

   Enter the calibration function (see above). If this is the first time you
   performed a calibration you should see "CF = 0" on the display. If not, add the
   measured aging offset to the displayed number.  Use pushbuttons 1 and 2 to set the
   device to the aging factor. Invoke a reset to exit.

 ********************************************************************************************
*/


// Do not change AutoCalibrate - for development use only
//_________________________Auto calibrate using DS3231? (false = NO, true = YES)______________________
bool AutoCalibrate = true;

// Do not change basicOperation - for development use only
//_________________________Basic operation without LCD/OLED? (false = NO, true = YES)______________________
bool basicOperation = false;

// Do not change basicOperation - for development use only
//___________________________Enter Si5351 calibration factor:______________________________________________
int CalFactor = 0;




/*
  ----------------------------------------------------------------------------------------------------
                       Scheduling configuration follows:
  ----------------------------------------------------------------------------------------------------                       
  Format: (TIME, MODE, BAND),

  TIME: WSPR & FST4w are in 2 minute increments past the top of the hour
       FST4W300 is in 5 minute increments past the top of the hour

  MODE: 0-FST4W120 1-FSR4W300 2-FST4W900 3-FST4W1800 4-WSPR

  BAND: is 'TXdialFreq' band number beginng with 0  i.e. 2 = 1836600 kHz



  Note: Ensure times do not overlap when using the 5 minute FST4W300 mode

        FST4W modes are normally used only on LF and MF

        Ensure format is correct
        i.e. brackets TIME comma MODE comma BAND comma brackets comma {24,1,11},

        Maximum 'schedule' length is 30 time slots. 
        Ensure the end of the 'schedule' list ends in (-1,-1,-1), 

  example follows:

  const int schedule [][3] = {
  {0, 4, 1},   // WSPR at top of the hour on 474.200 kHz
  {2, 0, 1},   // FST4W120 at 2 minutes past the hour on 474.200 kHz
  {4, 4, 9},   // WSPR at 4 minutes past the hour on 10138.700 kHz
  {20, 1, 0},  // FST4W300 at 20 minutes past the hour on 136.000 kHz
  {58, 4, 14}, // WSPR at 58 minutes past the hour on 28124600 kHz
  (-1,-1,-1),  // End of schedule flag
  };
  ----------------------------------------------------------------------------------------------------
*/

  const int schedule [][3] = {
  {0, 3, 1},   // FST4W 1800 at top of the hour on 474.200 kHz
  {30,2, 1},   // FST4W900 at 30 minutes past the hour on 474.200 kHz
  {45, 1, 1},  // FST4W300 at 45 minutes past the hour on 474.200 kHz
  {50, 0, 1},  // FST4W120 at 50 minutes past the hour on 474.200 kHz
  {52, 4, 1},  // WSPR at 52 minutes past the hour on 474.200 kHz
  {54, 0, 1},  // FST4W120 at 54 minutes past the hour on 474.200 kHz
  {56, 4, 1},  // WSPR at 58 minutes past the hour on 474.200 kHz
  {58, 0, 1},  // FST4W120 at 58 minutes past the hour on 474.200 kHz
  (-1,-1,-1),  // End of schedule flag
  };


const unsigned long TXdialFreq [] =
{
  136000  , // Band 0
  474200  , // Band 1
  1836600 , // Band 2
  1836800 , // Band 3
  3568600 , // Band 4
  3592600 , // Band 5
  5287200 , // Band 6
  5364700 , // Band 7
  7038600 , // Band 8
  10138700, // Band 9
  14095600, // Band 10
  18104600, // Band 11
  21094600, // Band 12
  24924600, // Band 13
  28124600, // Band 14
  0
};

// include the library code:
#include <Wire.h>
#include "SSD1306Ascii.h"
#include "SSD1306AsciiAvrI2c.h"
#include <EEPROM.h>

// Set up MCU pins
#define InterruptPin             2
#define InterruptPin3            3
#define CFup                     7
#define CFdown                   6
#define calibrate                9
#define endCalibrate             8
#define NanoLED                 13
#define XmitLED                 14
#define MinLED                  15
#define SecLED                  16
#define pushButton1              6
#define pushButton2              7
#define pushButton3              8
#define pushButton4              9
#define menu                    10

// Set DS3231 I2C address
#define DS3231_addr           0x68

// Set sI5351A I2C address
#define Si5351A_addr          0x60

// Define OLED address
#define I2C_ADDRESS           0x3C

// initialize oled display
SSD1306AsciiAvrI2c oled;

// Define Si5351A register addresses
#define CLK_ENABLE_CONTROL       3
#define CLK0_CONTROL            16
#define CLK1_CONTROL            17
#define CLK2_CONTROL            18
#define SYNTH_PLL_A             26
#define SYNTH_PLL_B             34
#define SYNTH_MS_0              42
#define SYNTH_MS_1              50
#define SYNTH_MS_2              58
#define SSC_EN                 149
#define PLL_RESET              177
#define XTAL_LOAD_CAP          183

typedef struct {  // Used in conjunction with PROGMEM to reduce RAM usage
  char description [4];
} descriptionType;

// configure variables
bool clockTimeChange = false, TransmitFlag = false, suspendUpdateFlag = false, FreqHopTX = false;
bool toggle = false, transmit_toggle = false, timeSet_toggle = false, StartCalc = true, startFlag = false;
int AutoCalFactor[50], CFcount, tcount2, TXcount, TXband, T_R_period, tempSecond, TXinterval = 2;
int hours, minutes, seconds, xday, xdate, xmonth, xyear, startCount = 0;
unsigned long XtalFreq = 25000000, tcount = 2, time_now = 0, LEDtimer, mult = 0, TXoffset = 1500;
const descriptionType daysOfTheWeek [7] PROGMEM = { {"SUN"}, {"MON"}, {"TUE"}, {"WED"}, {"THU"}, {"FRI"}, {"SAT"},};
const descriptionType monthOfTheYear [12] PROGMEM = { {"JAN"}, {"FEB"}, {"MAR"}, {"APR"}, {"MAY"}, {"JUN"}, {"JUL"}, {"AUG"}, {"SEP"}, {"OCT"}, {"NOV"}, {"DEC"},};
const byte daysInMonth [] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
byte TransmitMode, tcount3, mode;

// Load WSPR/FST4W symbol frequency offsets
int OffsetFreq[4] [4] = {
  { -219, -73, 73 , 219}, // 120
  { -84, -28, 28, 84},    // 300
  { -27, -9, 9, 27},      // 900
  { -13, -4, 4, 13}       // 1800
};

// Load WSPR/FST4W symbol length
unsigned long SymbolLength[4] = {
  683,    // 120
  1792,   // 300
  5547,   // 900
  11200   // 1800
};

int T_R_time[4] = {
  120,
  300,
  900,
  1800
};




/*----------------------------------------------------------------------------------------------------
  The application to generate the data for “WSPRsymbols” is called WSPRMSG.exe and is found at
  <https://github.com/W3PM>.  Download this file into a convenient directory.
  Open the file and enter your callsign, grid locator, and power (dBm) as prompted.
  "WSPRsymbols" will create a file named "WSPRMSG.txt" in the same directory that "WSPRsymbols"
  resides. Cut and paste the symbol message data to replace the variable’s data.
  ----------------------------------------------------------------------------------------------------
*/
// Load WSPR channel symbols for WZ9ZZZ FN21 30dBm
const byte  WSPRsymbols[162] = {
  3, 3, 0, 0, 0, 2, 2, 0, 3, 2, 2, 2, 3, 1, 1, 0, 2, 0, 1, 0, 0, 1, 2, 3, 1, 3, 1, 0, 2, 2, 2, 0,
  0, 0, 1, 2, 0, 1, 2, 1, 2, 2, 0, 0, 0, 2, 1, 2, 1, 3, 0, 2, 3, 3, 2, 3, 0, 0, 0, 3, 3, 0, 1, 0,
  2, 0, 0, 1, 3, 0, 1, 0, 1, 2, 1, 2, 3, 2, 0, 3, 0, 0, 3, 2, 3, 3, 0, 2, 0, 1, 1, 2, 3, 0, 3, 2,
  0, 2, 1, 2, 2, 2, 0, 0, 1, 0, 2, 1, 2, 2, 3, 1, 1, 0, 3, 3, 0, 0, 1, 1, 2, 3, 0, 0, 0, 1, 3, 1,
  2, 2, 2, 2, 2, 3, 0, 1, 0, 2, 1, 1, 0, 0, 0, 2, 0, 0, 2, 1, 1, 0, 3, 2, 1, 3, 2, 2, 0, 1, 1, 2,
  0, 0,
};


/*
   ----------------------------------------------------------------------------------------------------
    Open Windows Command Prompt. Navigate to directory C:\WSJTX\bin>. Enter the folling command
    using your own callsign, grid squate , and power level(dBM) enclosed in quotes as follows:

    fst4sim "WZ9ZZZ FN21 30" 120 1500 0.0 1 0.1 1.0 0 99 F > FST4W_message.txt"

    This will create a file named "FST4W_message.txt" in C:\WSJTX\bin. Open the "FST4W_message.txt"
    file into a text editor. Separate the "Channel symbols:" data with commas. Replace the following
    data with the comma separated data:
  ----------------------------------------------------------------------------------------------------
*/
// Load FST4W channel symbols for WZ9ZZZ FN21 30dBm
//const byte  FST4Wsymbols[161] PROGMEM = {
const byte  FST4Wsymbols[160] = {
  0, 1, 3, 2, 1, 0, 2, 3, 2, 3,
  3, 2, 3, 2, 3, 3, 0, 3, 1, 1,
  1, 2, 1, 1, 0, 0, 3, 3, 0, 3,
  3, 1, 0, 1, 3, 3, 0, 1, 2, 3,
  1, 0, 3, 2, 0, 1, 2, 0, 1, 0,
  3, 3, 1, 3, 3, 2, 3, 3, 0, 0,
  1, 2, 1, 1, 3, 0, 3, 2, 0, 3,
  2, 0, 3, 3, 0, 0, 0, 1, 3, 2,
  1, 0, 2, 3, 2, 2, 3, 1, 0, 2,
  2, 0, 2, 2, 2, 1, 0, 1, 0, 0,
  3, 1, 2, 0, 0, 3, 1, 0, 2, 2,
  1, 2, 2, 2, 2, 3, 1, 0, 3, 2,
  0, 1, 2, 1, 3, 2, 3, 3, 3, 2,
  1, 0, 1, 0, 2, 2, 3, 0, 2, 3,
  3, 0, 2, 0, 0, 1, 1, 3, 2, 1,
  1, 0, 0, 1, 3, 2, 1, 0, 2, 3
};


//***********************************************************************
// This interrupt is used for Si5351 25MHz crystal frequency calibration
// Called every second by from the DS3231 RTC 1PPS to Nano pin D2
//***********************************************************************
void Interrupt()
{
  tcount++;
  tcount2++;
  if (tcount == 4)                                // Start counting the 2.5 MHz signal from Si5351A CLK0
  {
    TCCR1B = 7;                                   //Clock on falling edge of pin 5
  }
  else if (tcount == 44)                         //Total count is = XtalFreq x 4 - stop counting
  {
    TCCR1B = 0;                                   //Turn off counter
    unsigned long TempFreq = (mult * 0x10000 + TCNT1);     //Calculate corrected XtalFreq
    TCNT1 = 0;                                    //Reset count to zero
    mult = 0;
    tcount = 0;                                   //Reset the seconds counter

    // The following is an averageing alorithm used to smooth out DS3231 1pps jitter
    // Note: The upper and lower frequecny constraint prevents autocalibration data corruption
    //       which may occur if the COUNTER switch is in the wrong position.
    if (TempFreq > 99988000L & TempFreq < 100012000L) // Check bounds
    {
      int N = 12;
      if (CFcount == N)
      {
        CFcount = 0;
        StartCalc = true;
      }
      if (StartCalc == false)                       // This is the initial warm-up period
      {
        AutoCalFactor[CFcount] = 100000000UL - TempFreq;
        if (suspendUpdateFlag == true) CFcount++;   // Don't update corrected crystal frequency while transmitting

        else
        {
          XtalFreq = TempFreq / 4UL;                // Update corrected crystal frequency when not transmitting
          CFcount++;
        }
      }
      else                                          // Warm-up period completed, go into averaging mode
      {
        long temp = 0;
        AutoCalFactor[CFcount] = 100000000UL - TempFreq;
        for (int i = 0; i < N ; i++) temp = temp + AutoCalFactor[i];
        if (suspendUpdateFlag == false) XtalFreq = (100000000UL - (round(temp / N))) / 4UL; //Average the correction factors and update crystal frequency
        CFcount++;
      }
    }
  }
  digitalWrite(SecLED, LOW);
  digitalWrite(NanoLED, LOW);
  LEDtimer = millis() + 500;
  if (basicOperation == true & tcount2 == 120)
  {
    tcount2 = 0;
  }
}


//******************************************************************
// Basic operation - interrupt routine used to set timing to zero
// at the top of a two miniute time slot.  Input on MCU pin 3
//******************************************************************
void Interrupt2()
{
  tcount2 = 0;
}


//******************************************************************
// Timer 1 overflow intrrupt vector.
// Called upon counter overflow from timer 1
//******************************************************************
ISR(TIMER1_OVF_vect)
{
  mult++;                                          //Increment multiplier
  TIFR1 = (1 << TOV1);                             //Clear overlow flag
}


//----------------------------------------------------------------------------------------------------
//                         Initial sketch setup follows:
//----------------------------------------------------------------------------------------------------
void setup()
{
  Wire.begin();                                 // join I2C bus
  Serial.begin(115200);

  // Initialize the Si5351
  Si5351_write(XTAL_LOAD_CAP, 0b11000000);      // Set crystal load to 10pF
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000000); // Enable CLK0, CLK1 and CLK2
  Si5351_write(CLK0_CONTROL, 0b01001111);       // Set PLLA to CLK0, 8 mA output, INT mode
  Si5351_write(CLK1_CONTROL, 0b01101111);       // Set PLLB to CLK1, 8 mA output, INT mode
  Si5351_write(CLK2_CONTROL, 0b01101111);       // Set PLLB to CLK2, 8 mA output, INT mode
  Si5351_write(PLL_RESET, 0b10100000);          // Reset PLLA and PLLB
  Si5351_write(SSC_EN, 0b00000000);             // Disable spread spectrum


  // Set up DS3231 for 1 Hz squarewave output
  // Needs only be written one time provided DS3231 battery is not removed
  Wire.beginTransmission(DS3231_addr);
  Wire.write(0x0E);
  Wire.write(0);
  Wire.endTransmission();

  //Set up Timer1 as a frequency counter - input at pin 5
  TCCR1B = 0;                                    //Disable Timer5 during setup
  TCCR1A = 0;                                    //Reset
  TCNT1  = 0;                                    //Reset counter to zero
  TIFR1  = 1;                                    //Reset overflow
  TIMSK1 = 1;                                    //Turn on overflow flag

  // Inititalize 1 Hz input pin
  pinMode(InterruptPin, INPUT);
  digitalWrite(InterruptPin, HIGH);              // internal pull-up enabled

  // Set pin 2 for external 1 Hz interrupt input
  attachInterrupt(digitalPinToInterrupt(InterruptPin), Interrupt, FALLING);

  if (basicOperation == true)
  {
    pinMode(InterruptPin3, INPUT);
    digitalWrite(InterruptPin3, HIGH);           // internal pull-up enabled
    attachInterrupt(digitalPinToInterrupt(InterruptPin3), Interrupt2, FALLING);
    TransmitFlag = true;
  }

  // Add CalFactor to the Si5351 crystal frequency for non-autocal frequency updates
  if (AutoCalibrate == false)
  {
    XtalFreq += CalFactor;  // Use corection factor if 1pps not used.
    detachInterrupt(digitalPinToInterrupt(InterruptPin)); // Disable the 1pps interrupt
  }

  // Allow time for autocalibration and Si5351 warm up for upon initial start
  // The first WSPR transmission will be delayed by one transmit interval
  TXcount = TXinterval + 1;

  // Set up push buttons
  pinMode(pushButton1, INPUT);
  digitalWrite(pushButton1, HIGH);      // internal pull-up enabled
  pinMode(pushButton2, INPUT);
  digitalWrite(pushButton2, HIGH);      // internal pull-up enabled
  pinMode(pushButton3, INPUT);
  digitalWrite(pushButton3, HIGH);      // internal pull-up enabled
  pinMode(pushButton4, INPUT);
  digitalWrite(pushButton4, HIGH);      // internal pull-up enabled
  pinMode(menu, INPUT);
  digitalWrite(menu, HIGH);             // internal pull-up enabled

  // Set up LEDs
  pinMode(NanoLED, INPUT);
  digitalWrite(NanoLED, HIGH);           // internal pull-up enabled
  pinMode(XmitLED, OUTPUT);              // Use with dropping resistor on pin D14
  digitalWrite(XmitLED, LOW);
  pinMode(SecLED, OUTPUT);               // Use with dropping resistor on pin D16
  digitalWrite(SecLED, LOW);
  pinMode(MinLED, OUTPUT);               // Use with dropping resistor on pin D15
  digitalWrite(MinLED, LOW);

  TCCR1B = 0;                             //Disable Timer1

  // Enable the Si5351
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000010); // Enable CLK0 and CLK2 - disable CLK1

  // Set CLK0 to 2.5 MHz for autocalibration
  si5351aSetFreq(SYNTH_MS_0, 2500000UL, 0);

  // Set CLK2 to 2.5 MHz for frequency stabilization
  si5351aSetFreq(SYNTH_MS_2, 2500000UL, 0);

  // Get stored transmit band
  TXband = EEPROM.read(10);
  if (TXband > 20 | TXband < 0) TXband = 1;             //Ensures valid EEPROM data - if not valid will default to 630m

  // Get stored transmit interval
  EEPROM.get(62, TXinterval);
  if (TXinterval > 30 | TXinterval < 0) TXinterval = 2; //Ensures valid EEPROM data - if not valid will default to 2

  // Get stored TX hopping variable
  EEPROM.get(78, FreqHopTX);
  if (FreqHopTX < 0 | FreqHopTX > 1) FreqHopTX = false; // Set to false if out of bounds

  // Get stored TX offset variable
  EEPROM.get(66, TXoffset);
  if (TXoffset < 0 | TXoffset > 1600) TXoffset = 1500;  // Set to 1500 if out of bounds

  // Get stored mode
  EEPROM.get(82, mode);
  if (mode < 0 | mode > 5) mode = 4;                   // Set to 4 (WSPR) if out of bounds
  if (mode < 4) T_R_period = mode;                   // Set T_R_period = mode
  if (mode == 4)  T_R_period = 0;                    // Set T_R_period to 120 seconds for WSPR

  // Set oled font size and type
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(fixed_bold10x15);

  // Setup OLED initial display
  oled.clear();
  oled.setCursor(0, 4);
  oled.println(F("PB3: scroll"));
  oled.print(F("PB2: select"));

  // Store time reference for sketch timing i.e. delays, EEPROM writes, display toggles
  // The Arduino "delay" function is not used because of critical timing conflicts
  time_now = millis();

  // Check for calibration function
  if (digitalRead(calibrate) == LOW) Calibrate(); //Used to enter calibration function upon reset

}



//******************************************************************
// Loop starts here:
// Loops consecutively to check MCU pins for activity
//******************************************************************
void loop()
{
  getTime();
  if (seconds != tempSecond)
  {
    tempSecond = seconds;
    if (suspendUpdateFlag == false)
    {
      /*
        ----------------------------------------------------------------
        Schedule transmissions begins here
        ----------------------------------------------------------------
             Multimode transmitting begins here
               0 = FST4W 120
               1 = FST4W 300
               2 = FST4W 900
               3 = FST4W 1800
               4 = WSPR
               5 = Schedule

              Schedule start time limitations:
              FST4W 1800: 0 or 30 minutes past top of hour
              FST4W 900 : 0, 15, 30, 45 minutes past top of hour
              FST4W 300 : 0, 5, 10.………45, 50, 55 minutes past top of hour
              WSPR      : any even numbered minute

             -----------------------------------------------------------------
      */
      if (seconds == 0 & TransmitFlag == true)
      {
        Serial.print(mode);
        Serial.print(" ");
        if (mode == 5) // Check 'schedule' first
        {
          // ---------------------------------------------------------------------
          // Transmit determined by schedule begins here
          // ---------------------------------------------------------------------
          for (int timeslot = 0; timeslot <= 29; timeslot++)
          {    
            if ((schedule [timeslot] [0]) == minutes)
            {
              TXinterval = 1;
              TXband = schedule [timeslot] [2];
              TransmitMode = schedule [timeslot][1];
              if (TransmitMode < 0 | TransmitMode > 4) return;
              if (TransmitMode < 4) T_R_period = TransmitMode;   // Set T_R_period = mode
              if (TransmitMode == 4)  T_R_period = 0;            // Set T_R_period to 120 seconds for WSPR
              setfreq();
              transmit();
            }
            if(schedule [timeslot] [0] == -1) timeslot = 29;
          }
        }

        // ---------------------------------------------------------------------
        // Transmit determined by transmit interval begins here
        // ---------------------------------------------------------------------
        else
        {
          if (mode == 0 | mode == 4) // must be either FST4W120 or WSPR 120 second mode
          {
            if (bitRead(minutes, 0) == 0)
            {
              T_R_period = 0;
              TransmitMode = mode;
              transmit();
            }
          }
          if (mode == 1) // FST4W300 300 second mode
          {
            if (minutes % 5 == 0)
            {
              T_R_period = 1;
              TransmitMode = mode;
              transmit();
            }
          }
          if (mode == 2) // FST4W900 15 minute mode
          {
            if (minutes % 15 == 0)
            {
              T_R_period = 2;
              TransmitMode = mode;
              transmit();
            }
          }
          if (mode == 3) // FST4W1800 30 minute mode
          {
            if (minutes % 30 == 0)
            {
              T_R_period = 3;
              TransmitMode = mode;
              transmit();
            }
          }
        }
      }

      // ---------------------------------------------------------------------
      // Do the following when not transmitting
      // ---------------------------------------------------------------------
      displayMode();
      TransmitStatus();
      setfreq();
      displayClock();
      displayStatus();
    }
  }

  //Turn tranmit inhibit on and off during non-transmit period
  if (digitalRead(pushButton2) == LOW & suspendUpdateFlag == false)
  {
    altDelay(300);
    transmit_toggle = !transmit_toggle;
    if (transmit_toggle == false)
    {
      TransmitFlag = false;
      TransmitStatus();
    }
    else
    {
      TransmitFlag = true;
      TransmitStatus();
    }
  }

  if (digitalRead (menu) == LOW)
  {
    oled.clear();
    oled.setCursor(0, 4);
    oled.println(F("PB3: scroll"));
    oled.print(F("PB2: select"));
    Menu_selection();
  }

  // LED operation
  if (LEDtimer < millis())
  {
    digitalWrite(SecLED, HIGH);
    digitalWrite(NanoLED, HIGH);
  }
  if (seconds == 0  & basicOperation == false)digitalWrite(MinLED, HIGH);
  if (seconds != 0  & basicOperation == false)digitalWrite(MinLED, LOW);
  if ((tcount2 == 0 | tcount2 == 60) & basicOperation == true)digitalWrite(MinLED, HIGH);
  if ((tcount2 == 1 | tcount2 == 61) & basicOperation == true)digitalWrite(MinLED, LOW);
}


//******************************************************************
// Menu selection function follows:
// Used to change interval, offset, mode, and turn
// in-band frequency hopping on and off
// Called called by loop
//******************************************************************
void Menu_selection()
{
  // Loop through the  menu rota until a selection is made
  while (digitalRead(pushButton2) == HIGH)
  {
    if (digitalRead(pushButton3) == LOW)
    {
      altDelay(500);
      startCount = startCount + 1;;
      if (startCount > 7) startCount = 0;
    }
    switch (startCount) {
      case 0:
        oled.setCursor(0, 0);
        oled.println(F("   EXIT    "));
        break;
      case 1:
        oled.setCursor(0, 0);
        oled.println(F("TX INTERVAL"));
        break;
      case 2:
        oled.setCursor(0, 0);
        oled.println(F(" SET OFFSET"));
        break;
      case 3:
        oled.setCursor(0, 0);
        oled.println(F("TX FREQ HOP"));
        break;
      case 4:
        oled.setCursor(0, 0);
        oled.println(F("SELECT MODE"));
        break;
      case 5:
        oled.setCursor(0, 0);
        oled.println(F("SELECT BAND"));
        break;
      case 6:
        oled.setCursor(0, 0);
        oled.println(F(" SET TIME  "));
        break;
      case 7:
        oled.setCursor(0, 0);
        oled.println(F(" SET DATE  "));
        break;
    }
  }

  switch (startCount) {
    case 0: // Exit
      altDelay(500);
      return;
      break;
    case 1: // Enter transmit interval begins here:
      EditTXinterval();
      break;
    case 2: // Transmit offset frequency adjustment begins here:
      EditTXoffset();
      break;
    case 3: // Transmit TX frequency hopping selection begins here:
      EditTXhop();
      break;
    case 4: // Mode selection begins here:
      ModeSelect();
      break;
    case 5: // Band selection begins here:
      BandSelect();
      break;
    case 6: // RTC clock adjustment begins here:
      adjClock();
      break;
    case 7: // RTC date adjustment begins here:
      setDATE();
      break;
  }
}


//******************************************************************
//  displayMode() follows:
//  Used to display the mode of operation on the home screen
//
//  Called loop()
//******************************************************************
void displayMode()
{
  oled.setCursor(0, 0);
  switch (mode)
  {
    case 0:
      oled.print(F("FST4W    "));
      break;

    case 1:
      oled.print(F("FST4W    "));
      break;

    case 2:
      oled.print(F("FST4W    "));
      break;

    case 3:
      oled.print(F("FST4W    "));
      break;

    case 4:
      oled.print(F("WSPR     "));
      break;

    case 5:
      oled.print(F("SCHEDULE "));
      break;
  }
}

//******************************************************************
// displayStatus follows:
// Displays the TX offset, TX interval, symbol period, and
// frequency hopping staus on the home screen
//
// Called by loop()
//******************************************************************
void displayStatus()
{
  if (seconds % 2 == 0)
  {
    if (toggle == true)
    {
      tcount3++;
      if (tcount3 > 4) tcount3 = 0;
      oled.setCursor(0, 6);
      oled.print(F("            "));
    }
    toggle = !toggle;
  }

  if (toggle == false)
  {
    getDate();
    oled.setCursor(0, 6);
    switch (tcount3)
    {
      case 0: // Display WSPR TX offset
        oled.print (F("Offset:"));
        oled.print(TXoffset);
        break;

      case 1: // Display transmit interval
        oled.print(F("TXint: "));
        if (mode == 5) oled.print (F("Sched"));
        else oled.print (TXinterval);
        break;

      case 2: // Display frequency hopping YES/NO
        oled.print(F("TXhop: "));
        if (FreqHopTX == false) oled.print(F("NO"));
        else oled.print(F("YES"));
        break;

      case 3: // Display period
        oled.print(F("Period: "));
        if (mode == 5) oled.print (F("Sked"));
        else oled.print(T_R_time[T_R_period]);
        break;

      case 4: // Display date
        descriptionType oneItem;
        memcpy_P (&oneItem, &daysOfTheWeek [xday], sizeof oneItem);
        oled.print (oneItem.description);
        oled.print(F(" "));

        if (xdate < 10) oled.print(F("0"));
        oled.print(xdate);
        oled.print(F(" "));
        memcpy_P (&oneItem, &monthOfTheYear [xmonth - 1], sizeof oneItem);
        oled.print (oneItem.description);
        oled.print(F("  "));
        break;
    }
  }
}


//******************************************************************
//  Band selection follows:
//  Used to determine the band of operation
//
//  Called by Menu_selection()
//******************************************************************
void BandSelect()
{
  SelectionHeader();

  while (digitalRead(pushButton3) == HIGH)
  {
    setfreq();
    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(200);
      TXband++;
      if (TXdialFreq [TXband] == 0)TXband = 0;
    }

    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(200);
      TXband--;
      if (TXband < 0) TXband = 0;
    }
  }
  oled.setCursor(0, 0);
  oled.print(F("SAVED  "));
  EEPROM.write(10, TXband);
  altDelay(2000);
}

//******************************************************************
// Transmit status function follows:
// Displays WSPR transmitter ON/OFF status
//
// Called by loop()
//******************************************************************
void TransmitStatus()
{
  if (TransmitFlag == false)
  {
    oled.setCursor(97, 0);
    oled.print(F("OFF"));
  }
  else
  {
    oled.setCursor(97, 0);
    oled.print(F(" ON"));
  }
}


//******************************************************************
// Clock adjust function follows:
// Used to adjust the system time
// Note: Quickly depressing pushbutton 1 will synchronize the clock
//       to the top of the minute "0"
//       Holding down pushbutton 4 while depressing pushbutton 1
//       will advance the hours.
//       Holding down pushbutton 3 while depressing pushbutton 2
//       will advance the minutes.
// Called called by Meun_selection()
//******************************************************************
void adjClock()
{
  while (digitalRead(pushButton3) == HIGH)
  {
    getTime();

    // Display current time
    int tempHour = hours;
    int tempMinute = minutes;
    int tempSecond = seconds;
    oled.setCursor(0, 0);
    oled.print(F("  "));
    int a = hours;
    int b = a % 10;
    a = a / 10;
    oled.print(a);
    oled.print(b);
    oled.print(F(":"));
    a = minutes;
    b = a % 10;
    a = a / 10;
    oled.print(a);
    oled.print(b);
    oled.print(F(":"));
    a = seconds;
    b = a % 10;
    a = a / 10;
    oled.print(a);
    oled.print(b);

    // Display legend
    oled.setCursor(0, 2);
    oled.print(F("Hold PB4"));
    oled.setCursor(0, 4);
    oled.print(F("PB1:H PB2:M"));
    oled.setCursor(0, 6);
    oled.print(F("PB3: Save   "));

    // Start one button time synchronization routine
    if (digitalRead(pushButton1) == LOW)
    {
      if (tempSecond > 30) tempMinute++;
      if (tempMinute > 59) tempMinute = 0;
      tempSecond = 0;
      updateTime(tempSecond, tempMinute, tempHour);
      altDelay(500);
    }
    timeSet_toggle = false;

    // Start set time routine
    while (digitalRead(pushButton4) == LOW)
    {
      if (digitalRead(pushButton1) == LOW)
      {
        altDelay(500);
        tempHour++;
        if (tempHour > 23) tempHour = 0;
        timeSet_toggle = true;
      }

      if (digitalRead(pushButton2) == LOW)
      {
        altDelay(500);
        tempMinute++;
        if (tempMinute > 59) tempMinute = 0;
        timeSet_toggle = true;
      }

      // Display set time
      oled.setCursor(0, 0);
      oled.print(F("  "));
      int a = tempHour;
      int b = a % 10;
      a = a / 10;
      oled.print(a);
      oled.print(b);
      oled.print(F(":"));
      a = tempMinute;
      b = a % 10;
      a = a / 10;
      oled.print(a);
      oled.print(b);
      oled.print(F(":00"));
    }

    // Update time if change is made
    if (timeSet_toggle == true)
    {
      int tempSecond = 0;
      updateTime(tempSecond, tempMinute, tempHour);
      timeSet_toggle = false;
    }
  }
}




//******************************************************************
// Date adjust function follows:
// Used to adjust the system date
// Note:
//       Holding down pushbutton 4 while depressing pushbutton 1
//       will advance the date.
//       Holding down pushbutton 4 while depressing pushbutton 2
//       will advance the month.
//       Holding down pushbutton 4 while depressing pushbutton 3
//       will advance the year.
// Called called by Meun_selection()
//******************************************************************
void setDATE()
{
  while (digitalRead(pushButton3) == HIGH)
  {
    getDate();

    int updateDate = xdate;
    int updateMonth = xmonth;
    int updateYear = xyear;

    // Display currently stored date
    oled.setCursor(0, 0);
    oled.print(updateDate);
    oled.print(F(" "));
    descriptionType oneItem;
    memcpy_P (&oneItem, &monthOfTheYear [updateMonth - 1], sizeof oneItem);
    oled.print (oneItem.description);
    oled.print(F(" "));
    oled.print(updateYear);
    oled.print(F("  "));

    // Display legend
    oled.setCursor(0, 2);
    oled.print(F("Hold PB4"));
    oled.setCursor(0, 4);
    oled.print(F("1:D 2:M 3:Y "));
    oled.setCursor(0, 6);
    oled.print(F("PB3: Save   "));

    // Start update
    while (digitalRead(pushButton4) == LOW)
    {
      if (digitalRead(pushButton1) == LOW)
      {
        altDelay(500);
        updateDate++;
        if (updateDate > 31) updateDate = 0;
        timeSet_toggle = true;
      }

      if (digitalRead(pushButton2) == LOW)
      {
        altDelay(500);
        updateMonth++;
        if (updateMonth > 12) updateMonth = 1;
        timeSet_toggle = true;
      }

      if (digitalRead(pushButton3) == LOW)
      {
        altDelay(500);
        updateYear++;
        if (updateYear > 30) updateYear = 19;
        timeSet_toggle = true;
      }

      // Display updates
      oled.setCursor(0, 0);
      //if (xdate < 10) oled.print(F("0"));
      oled.print(updateDate);
      oled.print(F(" "));
      descriptionType oneItem;
      memcpy_P (&oneItem, &monthOfTheYear [updateMonth - 1], sizeof oneItem);
      oled.print (oneItem.description);
      oled.print(F(" "));
      oled.print(updateYear);
    }

    // Save data if updated
    if (timeSet_toggle == true)
    {
      // Convert DEC to BCD
      updateDate = ((updateDate / 10) * 16) + (updateDate % 10);
      updateMonth = ((updateMonth  / 10) * 16) + (updateMonth % 10);
      updateYear = ((updateYear / 10) * 16) + (updateYear % 10);

      // Write the data
      Wire.beginTransmission(DS3231_addr);
      Wire.write((byte)0x04); // start at register 4
      Wire.write(updateDate);
      Wire.write(updateMonth);
      Wire.write(updateYear);
      Wire.endTransmission();
    }
  }
}


//******************************************************************
//  Edit transmit interval follows:
//  Used to determine time between transmissions
//
//  Called by Meun_selection()
//******************************************************************
void EditTXinterval()
{
  SelectionHeader();

  while ((digitalRead(pushButton3) == HIGH))
  {
    oled.setCursor(0, 0);
    oled.print(TXinterval);

    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(200);
      oled.setCursor(0, 0);
      oled.print(F("   "));
      oled.setCursor(0, 2);
      oled.print(F("          "));
      TXinterval++;
    }

    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(200);
      oled.setCursor(0, 0);
      oled.print(F("   "));
      oled.setCursor(0, 2);
      oled.print(F("          "));
      TXinterval--;
    }
  }
  oled.setCursor(0, 0);
  oled.print(TXinterval);
  oled.setCursor(0, 2);
  oled.print(F("SAVED   "));
  EEPROM.put(62, TXinterval);
  altDelay(2000);
}


//******************************************************************
//  Edit transmit frequency hop follows:
//  Used to engage/disengage frequency hopping within the WSPR window
//
//  Called by Meun_selection()
//******************************************************************
void EditTXhop()
{
  SelectionHeader();

  while ((digitalRead(pushButton3) == HIGH))
  {

    oled.setCursor(0, 0);
    if (FreqHopTX == true) oled.print(F("YES"));
    else oled.print(F("NO "));

    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(200);
      oled.setCursor(0, 0);
      oled.print(F("NO "));
      FreqHopTX = false;
    }


    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(200);
      oled.setCursor(0, 0);
      oled.print(F("YES"));
      FreqHopTX = true;
    }

    //if (digitalRead (menu) == LOW) Menu_selection();
  }

  oled.setCursor(0, 2);
  oled.print(F("SAVED   "));
  EEPROM.put(78, FreqHopTX);
  altDelay(2000);
}



//******************************************************************
//  Edit transmit offset follows:
//  Used to determine the WSPR offset frequency (1400 - 1600) Hz
//
//  Called by Menu_selection()
//******************************************************************
void EditTXoffset()
{
  SelectionHeader();

  while ((digitalRead(pushButton3) == HIGH))
  {
    oled.setCursor(0, 0);
    oled.print(TXoffset);

    if (digitalRead(pushButton4) == LOW)
    {
      altDelay(200);
      TXoffset++;
      if (TXoffset > 1600UL) TXoffset = 1600UL;
    }

    if (digitalRead(pushButton1) == LOW)
    {
      altDelay(200);
      TXoffset--;
      if (TXoffset < 1400UL) TXoffset = 1400UL;
    }

    // if (digitalRead (menu) == LOW) Menu_selection();
  }

  oled.setCursor(0, 2);
  oled.print(F("SAVED   "));
  EEPROM.put(66, TXoffset);
  altDelay(2000);
}


//******************************************************************
//  Determine WSPR/FST4W or Schedule mode type of operation follows:
//
//  0 = FST4W 120
//  1 = FST4W 300
//  2 = FST4W 900
//  3 = FST4W 1800
//  4 = WSPR
//  5 = Schedule
//
//  Schedule = WSPR and/or FST4W transmissions on an hourly cyclic basis
//
//  Called by Meun_selection()
//******************************************************************
void ModeSelect()
{
  SelectionHeader();
  EEPROM.get(82, mode);
  int i = mode;
  // while ((digitalRead(pushButton2) == HIGH) & (digitalRead(pushButton3) == HIGH))
  while (digitalRead(pushButton3) == HIGH)
  {
    if (digitalRead(pushButton4) == LOW)
    {
      i++;
      altDelay(200);
    }
    if (digitalRead(pushButton1) == LOW)
    {
      i--;
      altDelay(200);
    }
    oled.setCursor(0, 2);
    switch (i)
    {
      case 0:
        oled.print(F("FST4W 120 "));
        break;

      case 1:
        oled.print(F("FST4W 300 "));
        break;

      case 2:
        oled.print(F("FST4W 900 "));
        break;

      case 3:
        oled.print(F("FST4W 1800"));
        break;

      case 4:
        oled.print(F("   WSPR   "));
        break;

      case 5:
        oled.print(F(" SCHEDULE "));
        break;

    }
    if (i < 0 | i > 5)i = 0;
    if (i < 4) T_R_period = i;
    if (i == 4)  T_R_period = 0; // Set T_R_period to 120 seconds for WSPR
  }
  mode = i;
  oled.setCursor(0, 2);
  oled.print(F("SAVED     "));
  EEPROM.put(82, mode);
  altDelay(2000);
}


//******************************************************************
//  Selection Header:
//  Used to display the common header used in some menu selections
//
//  Called by all edit or select functions
//******************************************************************
void SelectionHeader()
{
  oled.clear();
  oled.setCursor(0, 4);
  oled.print("1&4 - edit");
  oled.setCursor(0, 6);
  oled.print("3 - save");
}


//******************************************************************
//  Time update function follows:
//  Used to retrieve the correct time from the DS3231 RTC
//
//  Called by displayClock(), adjClock(), and loop()
//******************************************************************
void  getTime()
{
  // Send request to receive data starting at register 0
  Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
  Wire.write((byte)0); // start at register 0
  Wire.endTransmission();
  Wire.requestFrom(DS3231_addr, 3); // request three bytes (seconds, minutes, hours)

  while (Wire.available())
  {
    seconds = Wire.read(); // get seconds
    minutes = Wire.read(); // get minutes
    hours = Wire.read();   // get hours

    seconds = (((seconds & 0b11110000) >> 4) * 10 + (seconds & 0b00001111)); // convert BCD to decimal
    minutes = (((minutes & 0b11110000) >> 4) * 10 + (minutes & 0b00001111)); // convert BCD to decimal
    hours = (((hours & 0b00100000) >> 5) * 20 + ((hours & 0b00010000) >> 4) * 10 + (hours & 0b00001111)); // convert BCD to decimal (assume 24 hour mode)
  }
}


//******************************************************************
//  Date update function follows:
//  Used to retrieve the correct date from the DS3231 RTC
//
//  The day of the week algorithm is a modified version
//  of the open source code found at:
//  Code by JeeLabs http://news.jeelabs.org/code/
//
//  Called by displayClock() and setDATE()
//******************************************************************
void  getDate()
{
  int nowDay;
  int nowDate;
  int tempdate;
  int nowMonth;
  int nowYear;

  // send request to receive data starting at register 3
  Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
  Wire.write((byte)0x03); // start at register 3
  Wire.endTransmission();
  Wire.requestFrom(DS3231_addr, 4); // request four bytes (day date month year)

  while (Wire.available())
  {
    nowDay = Wire.read();    // get day (serves as a placeholder)
    nowDate = Wire.read();   // get date
    nowMonth = Wire.read();  // get month
    nowYear = Wire.read();   // get year

    xdate = (((nowDate & 0b11110000) >> 4) * 10 + (nowDate & 0b00001111)); // convert BCD to decimal
    tempdate = xdate;
    xmonth = (((nowMonth & 0b00010000) >> 4) * 10 + (nowMonth & 0b00001111)); // convert BCD to decimal
    xyear = ((nowYear & 0b11110000) >> 4) * 10 + ((nowYear & 0b00001111)); // convert BCD to decimal

    if (xyear >= 2000) xyear -= 2000;
    for (byte i = 1; i < xmonth; ++i)
      tempdate += pgm_read_byte(daysInMonth + i - 1);
    if (xmonth > 2 && xyear % 4 == 0)
      ++tempdate;
    tempdate = tempdate + 365 * xyear + (xyear + 3) / 4 - 1;
    xday = (tempdate + 6) % 7; // Jan 1, 2000 is a Saturday, i.e. returns 6
  }
}


//******************************************************************
//  Time set function follows:
//  Used to set the DS3231 RTC time
//
//  Called by adjClock()
//******************************************************************
void updateTime(int updateSecond, int updateMinute, int updateHour)
{
  // Convert BIN to BCD
  updateSecond = updateSecond + 6 * (updateSecond / 10);
  updateMinute = updateMinute + 6 * (updateMinute / 10);
  updateHour = updateHour + 6 * (updateHour / 10);

  // Write the data
  Wire.beginTransmission(DS3231_addr);
  Wire.write((byte)0); // start at location 0
  Wire.write(updateSecond);
  Wire.write(updateMinute);
  Wire.write(updateHour);
  Wire.endTransmission();
}


//******************************************************************
// Alternate delay function follows:
// altDelay is used because the command "delay" causes critical
// timing errors.
//
// Called by all functions
//******************************************************************
unsigned long altDelay(unsigned long delayTime)
{
  time_now = millis();
  while (millis() < time_now + delayTime) //delay 1 second
  {
    __asm__ __volatile__ ("nop");
  }
}


//******************************************************************
// displayClock follows:
// Displays the time and date during WSPR function operation
//
// Called by loop()
//******************************************************************
void displayClock()
{
  oled.setCursor(0, 4);
  oled.print(F("  "));
  getTime();

  if (hours < 10) oled.print(F("0"));
  oled.print(hours);
  oled.print(F(":"));

  if (minutes < 10) oled.print(F("0"));
  oled.print(minutes);
  oled.print(F(":"));

  if (seconds < 10) oled.print(F("0"));
  oled.print(seconds);
  oled.print(F("   "));
}



//******************************************************************
// Set transmit frequency function follows:
// Calculates the frequency data to be sent to the Si5351 clock generator
// and displays the frequency on the oled display
//
// Called by loop() and transmit()
//******************************************************************
void setfreq()
{
  unsigned long  frequency = TXdialFreq [TXband] + TXoffset; // Temporarily store Freq_1

  oled.setCursor(0, 2);
  char buf[11];
  if (frequency >= 1000000UL)
  {
    int MHz = int(frequency / 1000000UL);
    int kHz = int ((frequency - (MHz * 1000000UL)) / 1000UL);
    int Hz = int (frequency % 1000UL);
    snprintf(buf, sizeof(buf), "%2u,%03u,%03u", MHz, kHz, Hz);
  }

  else if (frequency >= 1000UL & frequency < 1000000UL)
  {
    int kHz = int (frequency / 1000UL);
    int Hz = int (frequency % 1000UL);
    snprintf(buf, sizeof(buf), "%6u,%03u", kHz, Hz);
  }
  else if (frequency < 1000UL)
  {
    int Hz = int (frequency);
    snprintf(buf, sizeof(buf), "%10u", Hz);
  }
  oled.print(buf);
}




//******************************************************************
// WSPR transmit algorithm follows:
// Configures data and timing and then sends data to the Si5351
//
// Called by loop()
//******************************************************************
void transmit()
{
  int SymbolCount;
  if (mode != 5) // if in Schedule mode there is no reason to check transmit interval
  {
    TXcount++;
    if (TXcount > TXinterval) TXcount = 1;
    if (TXcount != TXinterval) return;
  }
  time_now = millis();
  altDelay(1000); // Delay 1 second
  suspendUpdateFlag = true;
  if (FreqHopTX == true) // Enables in-band TX frequency hopping in incremental 15Hz steps
  {
    TXoffset = TXoffset + 10UL;
    if (TXoffset > 1550UL) TXoffset = 1440UL;
  }
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000100); // Enable CLK0 CLK1 - disable CLK2 (to enhance Si5351 freq stability)
  setfreq();
  oled.setCursor(0, 4);
  oled.print(F(" XMITTING "));
  oled.setCursor(0, 6);
  oled.print(F("            "));
  oled.setCursor(0, 6);
  if (TransmitMode < 4) oled.print(F("FST4W "));
  if (TransmitMode == 4) oled.print(F("WSPR2 "));
  oled.print(T_R_time[T_R_period]);
  digitalWrite(XmitLED, HIGH);
  unsigned long currentTime = millis();
  if (TransmitMode == 4) SymbolCount = 162; // WSPR symbol count
  else
  {
    SymbolCount = 160; // FST4 symbol count
  }
  for (int count = 0; count < SymbolCount; count++)
  {
    unsigned long timer = millis();
    if (TransmitMode == 4) si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + TXoffset), OffsetFreq [0][WSPRsymbols[count]]); // WSPR mode
    else si5351aSetFreq(SYNTH_MS_1, (TXdialFreq [TXband] + TXoffset), OffsetFreq[T_R_period][FST4Wsymbols[count]]); // FST4W mode
    while ((millis() - timer) <= SymbolLength[T_R_period]) {
      __asm__("nop\n\t");
    };
  }
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000010); // Enable CLK0 CLK2 - disable CLK1 (drift compensation)
  si5351aSetFreq(SYNTH_MS_2, 2500000UL, 0);        // CLK2 is enabled to balance thermal drift between transmissions
  suspendUpdateFlag = false;
  digitalWrite(XmitLED, LOW);
}


//******************************************************************
//  Si5351 processing follows:
//  Generates the Si5351 clock generator frequency message
//
//  Called by sketch setup(), transmit(), and loop()
//******************************************************************
void si5351aSetFreq(int synth, unsigned long  frequency, int symbolOffset)
{
  unsigned long long  CalcTemp;
  unsigned long PLLfreq, divider, a, b, c, p1, p2, p3, PLL_P1, PLL_P2, PLL_P3;
  byte dividerBits = 0;
  int PLL, multiplier = 1;
  if (synth == SYNTH_MS_0) PLL = SYNTH_PLL_A;
  else PLL = SYNTH_PLL_B;

  frequency = frequency * 100UL + symbolOffset;
  c = 0xFFFFF;  // Denominator derived from max bits 2^20

  divider = 90000000000ULL / frequency;
  while (divider > 900UL) {           // If output divider out of range (>900) use additional Output divider
    dividerBits++;
    divider = divider / 2UL;
    multiplier = multiplier * 2;
    //multiplier = dividerBits << 4;
  }

  dividerBits = dividerBits << 4;
  if (divider % 2) divider--;
  //  PLLfreq = (divider * multiplier * frequency) / 10UL;

  unsigned long long PLLfreq2, divider2 = divider, multiplier2 = multiplier;
  PLLfreq2 = (divider2 * multiplier2 * frequency) / 100UL;
  PLLfreq = PLLfreq2;

  a = PLLfreq / XtalFreq;
  CalcTemp = PLLfreq2 - a * XtalFreq;
  CalcTemp *= c;
  CalcTemp /= XtalFreq ;
  b = CalcTemp;  // Calculated numerator


  p1 = 128UL * divider - 512UL;
  CalcTemp = 128UL * b / c;
  PLL_P1 = 128UL * a + CalcTemp - 512UL;
  PLL_P2 = 128UL * b - CalcTemp * c;
  PLL_P3 = c;


  // Write data to PLL registers
  Si5351_write(PLL, 0xFF);
  Si5351_write(PLL + 1, 0xFF);
  Si5351_write(PLL + 2, (PLL_P1 & 0x00030000) >> 16);
  Si5351_write(PLL + 3, (PLL_P1 & 0x0000FF00) >> 8);
  Si5351_write(PLL + 4, (PLL_P1 & 0x000000FF));
  Si5351_write(PLL + 5, 0xF0 | ((PLL_P2 & 0x000F0000) >> 16));
  Si5351_write(PLL + 6, (PLL_P2 & 0x0000FF00) >> 8);
  Si5351_write(PLL + 7, (PLL_P2 & 0x000000FF));


  // Write data to multisynth registers
  Si5351_write(synth, 0xFF);
  Si5351_write(synth + 1, 0xFF);
  Si5351_write(synth + 2, (p1 & 0x00030000) >> 16 | dividerBits);
  Si5351_write(synth + 3, (p1 & 0x0000FF00) >> 8);
  Si5351_write(synth + 4, (p1 & 0x000000FF));
  Si5351_write (synth + 5, 0);
  Si5351_write (synth + 6, 0);
  Si5351_write (synth + 7, 0);

}



//******************************************************************
// Write I2C data function for the Si5351A follows:
// Writes data over the I2C bus to the appropriate device defined by
// the address sent to it.

// Called by sketch setup, loop(), transmit(),
// si5351aSetFreq, and si5351aStart functions.
//******************************************************************
uint8_t Si5351_write(uint8_t addr, uint8_t data)
{
  Wire.beginTransmission(Si5351A_addr);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}



//******************************************************************
// Set the DS3231 RTC crystal oscillator aging offset function follows:
// This is effectively the system calibration routine. Small
// capacitors can be swithed in or out within the DS3231 RTC to control
// frequency accuracy. Positive aging values add capacitance to the
// array, slowing the oscillator frequency. Negative values remove
// capacitance from the array, increasing the oscillator frequency.
// One offset count provides about 0.1 ppm change in frequency.

// To enter the calibration function hold down pushbutton 4 and invoke
// a reset. Refer to the calibration notes.
//******************************************************************

void Calibrate()
{
  int CF, tempCF, busy;
  //oled.setFont(fixed_bold10x15);
  //oled.set1X();
  oled.clear();
  oled.print(F("Cal CF ="));
  oled.setCursor(0, 2);
  oled.println(F("PB1 = Down"));
  oled.println(F("PB2 = Up"));
  oled.print(F("reset = Exit"));

  while (digitalRead(endCalibrate) == HIGH)
  {
    // send request to receive data starting at register 3
    Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
    Wire.write((byte)0x10); // start at register 0x10
    Wire.endTransmission();
    Wire.requestFrom(DS3231_addr, 1); // request one byte (aging factor)
    while (Wire.available())
    {
      CF = Wire.read();
    }
    if (CF > 127) CF -= 256;
    tempCF = CF;
    oled.setCursor(96, 0);
    oled.print(F("    "));
    oled.setCursor(96, 0);
    oled.println(CF);
    if (digitalRead(CFdown) == LOW) CF -= 1;
    if (digitalRead(CFup) == LOW) CF += 1;

    Wire.beginTransmission(DS3231_addr); // DS3231_addr is DS3231 device address
    Wire.write((byte)0x0F); // start at register 0x0F
    Wire.endTransmission();
    Wire.requestFrom(DS3231_addr, 1); // request one byte to determine DS3231 update status
    while (Wire.available())
    {
      busy = Wire.read();
    }
    busy = bitRead(busy, 2);
    if (CF != tempCF & bitRead(busy, 2) == 0)
    {
      setAgingOffset(CF);
      forceConversion();
    }
    altDelay(500);
  }
}


void setAgingOffset(int offset)
{
  if (offset < 0) offset += 256;

  Wire.beginTransmission(DS3231_addr);
  Wire.write(0x10);
  Wire.write(offset);
  Wire.endTransmission();
}


void forceConversion()
{
  Wire.beginTransmission(DS3231_addr);
  Wire.write(0x0E);

  Wire.write(B00111100);
  Wire.endTransmission();
}
