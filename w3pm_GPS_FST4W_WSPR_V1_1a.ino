/*
  Si5351 bandswitched WSPR and FST4W source designed for 2200 - 10 meters utilizing
  a GPS receiver module for timing and frequency adjustment.

  This sketch uses a frequency resolution down to 0.01 Hz to provide optimum
  reolution for the FST4W/WSPR tone separation specification. This limits
  the upper frequency to 40 MHz.

  This sketch is hardware compatible with the W3PM Multifunction Project found in
  the May/June QEX magazine with an additional connection for the GPS serial input.

  Permission is granted to use, copy, modify, and distribute this software
  and documentation for non-commercial purposes.

  Copyright (C) 2020,  Gene Marcus W3PM GM4YRE

  17 October, 2020

  11 March, 2021          The variable "validGPSflag" can now equal 1 or 2 to proceed with normal operation
  30 December 2021  v1_1a Newer baatches of Si5351's default to spread spectrum if
                          not set to disable. This version includes code to disable
                          the spread spectrum mode.                    


  The code was sucessfully compiled using Arduino 1.8.13

   NOTE! Uses library SSD1306Ascii by Bill Greiman
        Load from: Sketch > Include Library > Manage Libraries

   EEPROM address (unused addresses maintain compatibility for hardware re-use with past projects)
         10    band
         62    TX interval
         66    TX offset
         78    TX hop on/off
         82    TX mode

  ---------------------------------------------------------------------------------------------------------------
  Five pushbuttons are used to control the functions.
  Pushbutton pin allocations follow:
  ---------------------------------------------------------------------------------------------------------------
        Home Screen
  PB1   N/A
  PB2   Transmitter ON / OFF
  PB3   N/A
  PB4   N/A

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
  D0/RX to GPS TX (this pin may differ on the Nano board)
  D1
  D2  GPS 1pps input
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
  A4/D18 Si5351 & OLED SDA
  A5/D19 Si5351 & OLED SCL

*/


// Do not change AutoCalibrate - for development use only
//_________________________Auto calibrate using GPS? (false = NO, true = YES)_____________________________
bool AutoCalibrate = true;

// Do not change basicOperation - for development use only
//_________________________Basic operation without LCD/OLED? (false = NO, true = YES)______________________
bool basicOperation = false;

// Do not change basicOperation - for development use only
//___________________________Enter Si5351 calibration factor:______________________________________________
int CalFactor = 0;

const unsigned long TXdialFreq [] =
{
  136000      ,
  474200      ,
  1836600      ,
  1836800      ,
  3592600      ,
  3568600      ,
  5287200      ,
  7038600      ,
  10138700      ,
  14095600      ,
  18104600      ,
  21094600      ,
  24924600      ,
  28124600      ,
  50293000      ,
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


// configure variables
bool TransmitFlag = false, suspendUpdateFlag = false, FreqHopTX = false;
bool toggle = false, transmit_toggle = false, toggle_LED;
int tcount2, TXcount, TXband;
int hours, minutes, seconds, startCount = 0;
int character, T_R_period, byteGPS = -1, TXinterval = 3;
unsigned long XtalFreq = 25000000, tcount = 2, time_now = 0, LEDtimer, mult = 0, TXoffset = 1500;
char StartCommand[7] = "$GPGGA", StartCommand2[7] = "$GPRMC", buffer[300] = "", EW, NS;
byte Buffer[10], bufCounter, tempBuf, TransmitMode, tcount3, mode;
int IndiceCount = 0, StartCount = 0, counter = 0, indices[13], validGPSflag;
static bool s_GpsOneSecTick = false;
int Lat10, Lat1, Lon100, Lon10, Lon1;
int dLon1, dLon2, dLat1, dLat2, dlon, sat1, sat10;



// Load WSPR/FST4W symbol frequency offsets
int OffsetFreq[4] [4] = {
  { -219, -73, 73 , 219}, // 120
  { -84, -28, 28, 84},  // 300
  { -27, -9, 9, 27},       // 900
  { -13, -4, 4, 13}     // 1800
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
  <http://www.knology.net/~gmarcus/>.  Download this file into a convenient directory.
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
// Called every second by from the GPS 1PPS to Nano pin D2
//***********************************************************************
void Interrupt()
{
  s_GpsOneSecTick = true;                         // New second by GPS.
  tcount++;
  tcount2++;
  if (tcount == 4)                                // Start counting the 2.5 MHz signal from Si5351A CLK0
  {
    TCCR1B = 7;                                   //Clock on falling edge of pin 5
  }
  else if (tcount == 44)                         //Total count is = XtalFreq x 4 - stop counting
  {
    TCCR1B = 0;                                   //Turn off counter
    XtalFreq = (mult * 0x10000 + TCNT1) / 4ULL;   //Calculate corrected XtalFreq
    TCNT1 = 0;                                    //Reset count to zero
    mult = 0;
    tcount = 0;                                   //Reset the seconds counter
  }

  digitalWrite(SecLED, HIGH);
  digitalWrite(NanoLED, HIGH);
  if (toggle_LED == false)
  {
    digitalWrite(SecLED, LOW);
    digitalWrite(NanoLED, LOW);
  }
  toggle_LED = !toggle_LED;
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

  Serial.begin(9600);

  // Initialize the Si5351
  Si5351_write(XTAL_LOAD_CAP, 0b11000000);      // Set crystal load to 10pF
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000000); // Enable CLK0, CLK1 and CLK2
  Si5351_write(CLK0_CONTROL, 0b01001111);       // Set PLLA to CLK0, 8 mA output, INT mode
  Si5351_write(CLK1_CONTROL, 0b01101111);       // Set PLLB to CLK1, 8 mA output, INT mode
  Si5351_write(CLK2_CONTROL, 0b01101111);       // Set PLLB to CLK2, 8 mA output, INT mode
  Si5351_write(PLL_RESET, 0b10100000);          // Reset PLLA and PLLB
  Si5351_write(SSC_EN,0b00000000);              // Disable spread spectrum
  

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
  if (mode < 0 | mode > 5) mode = 4;                    // Set to 4 (WSPR) if out of bounds
  if (mode < 4) T_R_period = mode;                      // Set T_R_period = mode
  if (mode == 4)  T_R_period = 0;                       // Set T_R_period to 120 seconds for WSPR


  // Set oled font size and type
  oled.begin(&Adafruit128x64, I2C_ADDRESS);
  oled.setFont(fixed_bold10x15);

  // Setup OLED initial display
  oled.clear();
  oled.setCursor(0, 0);
  oled.println(F("WAITING FOR"));
  oled.setCursor(12, 2);
  oled.println(F("VALID GPS"));
  oled.setCursor(36, 4);
  oled.println(F("DATA"));


  // Store time reference for sketch timing i.e. delays, EEPROM writes, display toggles
  // The Arduino "delay" function is not used because of critical timing conflicts
  time_now = millis();

}



//******************************************************************
// Loop starts here:
// Loops consecutively to check MCU pins for activity
//******************************************************************
void loop()
{
  GPSprocess();

  if (s_GpsOneSecTick )
  {
    s_GpsOneSecTick = false;
    if ((validGPSflag == 1 | validGPSflag == 2)  & suspendUpdateFlag == false)
    {
/*
 ----------------------------------------------------------------
   Multi mode transmissions set by a schedule begins here
 ----------------------------------------------------------------
        Multimode transmitting on an hourly schedule begins here
          0 = FST4W 120
          1 = FST4W 300
          2 = FST4W 900
          3 = FST4W 1800
          4 = WSPR
          5 = Multi Mode

         Schedule start time limitations:
         FST4W 1800: 0 or 30 minutes past top of hour
         FST4W 900 : 0, 15, 30, 45 minutes past top of hour
         FST4W 300 : 0, 5, 10.………45, 50, 55 minutes past top of hour
         WSPR      : any even numbered minute

        -----------------------------------------------------------------
      */
      if (seconds == 0 & TransmitFlag == true)
      {
        if (mode == 5) // Start cyclic hour beaconing for multi mode operation
        {
          switch (minutes)
          {
            case 0: // Transmit FST4W 1800 at top of the hour
              if (TransmitFlag == true)
              {
                TransmitMode = 3; // FST4W 1800
                T_R_period = 3;
                transmit();
                break;
              }
            case 30: // Transmit FST4W 900 at 30 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 2; // FST4W 900
                T_R_period = 2;
                transmit();
                break;
              }
            case 45: // Transmit FST4W 300 at 45 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 1; // FST4W 300
                T_R_period = 1;
                transmit();
                break;
              }
            case 50: // Transmit FST4W 120 at 50 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 0; // FST4W 120
                T_R_period = 0;
                transmit();
                break;
              }
            case 52: // Transmit WSPR at 52 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 4; // WSPR
                T_R_period = 0;
                transmit();
                break;
              }
            case 54: // Transmit FST4W 120 at 54 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 0; // FST4W 120
                T_R_period = 0;
                transmit();
                break;
              }
            case 56: // Transmit WSPR at 52 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 4; // WSPR
                T_R_period = 0;
                transmit();
                break;
              }
            case 58: // Transmit FST4W 120 at 58 minutes past top of hour
              if (TransmitFlag == true)
              {
                TransmitMode = 0; // FST4W 120
                T_R_period = 0;
                transmit();
                break;
              }
          }
        }

// ---------------------------------------------------------------------
// Transmit time determined by mode and transmit interval begins here
// ---------------------------------------------------------------------
        else 
        {
          TransmitMode = mode;
          switch (T_R_period)
          {
            case 0: // T/R period  = 120 seconds
              if (bitRead(minutes, 0) == 0)
              {
                T_R_period = 0;
                transmit();
              }
              break;
            case 1: // T/R period  = 300 seconds
              if ((minutes % 5) == 0)
              {
                T_R_period = 1;
                transmit();
              }
              break;
            case 2: // T/R period  = 900 seconds
              if (minutes == 0 | minutes == 15 | minutes == 30 | minutes == 45)
              {
                T_R_period = 2;
                transmit();
              }
              break;
            case 3: // T/R period  = 1800 seconds
              if (minutes == 0 | minutes == 30)
              {
                T_R_period = 3;
                transmit();
              }
              break;
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
  // if (seconds == 0  & basicOperation == false)digitalWrite(MinLED, HIGH);
  // if (seconds != 0  & basicOperation == false)digitalWrite(MinLED, LOW);
  // if ((tcount2 == 0 | tcount2 == 60) & basicOperation == true)digitalWrite(MinLED, HIGH);
  // if ((tcount2 == 1 | tcount2 == 61) & basicOperation == true)digitalWrite(MinLED, LOW);
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
      if (startCount > 5) startCount = 0;
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
    case 5: // Mode selection begins here:
      BandSelect();
      break;
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
  oled.print(F("SAVED   "));
  EEPROM.write(10, TXband);
  altDelay(2000);
}




//******************************************************************
// Mode selection function follows;
//
//  Determine WSPR FST4W or Multi of operation follows:
//
//  0 = FST4W 120
//  1 = FST4W 300
//  2 = FST4W 900
//  3 = FST4W 1800
//  4 = WSPR
//  5 = multi mode
//
//  Multi mode = WSPR and/or FST4W transmissions on an hourly cyclic basis
//
//  Called by Menu_selection()
//******************************************************************
void ModeSelect()
{
  SelectionHeader();
  EEPROM.get(82, mode);
  int i = mode;
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
        oled.print(F("MULTI MODE"));
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
      oled.print(F("MLT MODE "));
      break;
  }
}


//******************************************************************
//  Selection Header:
//  Used to display the common header used in some menu selections
//
//  Called by all edit funtions
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
// displayClock follows:
// Displays UTC time
//
// Called by loop()
//******************************************************************
void displayClock()
{
  oled.setCursor(0, 4);
  oled.print(F(" "));

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
      if (tcount3 > 3) tcount3 = 0;
      oled.setCursor(0, 6);
      oled.print(F("            "));
    }
    toggle = !toggle;
  }

  if (toggle == false)
  {
    oled.setCursor(0, 6);
    switch (tcount3)
    {
      case 0: // Display transmit TX offset
        oled.print (F("Offset:"));
        oled.print(TXoffset);
        break;

      case 1: // Display transmit interval
        oled.print(F("TXint: "));
        if (mode == 5) oled.print (F("Mult"));
        else oled.print (TXinterval);
        break;

      case 2: // Display frequency hopping YES/NO
        oled.print(F("TXhop: "));
        if (FreqHopTX == false) oled.print(F("NO"));
        else oled.print(F("YES"));
        break;

      case 3: // Display period
        oled.print(F("Period: "));
        if (mode == 5) oled.print (F("Mult"));
        else oled.print(T_R_time[T_R_period]);
        break;
    }
  }
}



//******************************************************************
// Set transmit frequency function follows:
// Calculates the frequency data to be sent to the Si5351 clock generator
// and displays the frequency on the olded display
//
// Called by bandSelect() and transmit()
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
// Transmit algorithm follows:
// Configures data and timing and then sends data to the Si5351 
//
// Called by loop()
//******************************************************************
void transmit()
{
  int SymbolCount;
  if (mode != 5) // if in multi mode there is no reason to check transmit interval
  {
    TXcount++;
    if (TXcount > TXinterval) TXcount = 1;
    if (TXcount != TXinterval) return;
  }
  time_now = millis();
  suspendUpdateFlag = true;
  if (FreqHopTX == true) // Enables in-band TX frequency hopping in incremental 10Hz steps
  {
    TXoffset = TXoffset + 10UL;
    if (TXoffset > 1550UL) TXoffset = 1440UL;
  }
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000100); // Enable CLK0 CLK1 - disable CLK2
  setfreq();
  oled.setCursor(0, 4);
  oled.print(F(" XMITTING "));
  oled.setCursor(0, 6);
  oled.print(F("            "));
  oled.setCursor(0, 6);
  if (TransmitMode < 4) oled.print(F("FST4W "));
  if (TransmitMode == 4) oled.print(F("WSPR  "));
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
  suspendUpdateFlag = false;
  Si5351_write(CLK_ENABLE_CONTROL, 0b00000010); // Enable CLK0 CLK2 - disable CLK1 (drift compensation)
  si5351aSetFreq(SYNTH_MS_2, 2500000UL, 0);        // CLK2 is enabled to balance thermal drift between transmissions
  digitalWrite(XmitLED, LOW);
  seconds = 53;// Time not updated during transmit. This prevents loopback to transmit
}



//******************************************************************
//  GPS NMEA processing starts here
//
//  called by loop()
//******************************************************************
void GPSprocess()
{
  byte temp, pinState = 0, i;

  if (Serial.available() > 0)
  {
    // NMEA $GPGGA data begins here
    byteGPS = Serial.read();     // Read a byte of the serial port
    buffer[counter] = byteGPS;   // If there is serial port data, it is put in the buffer
    counter++;
    if (byteGPS == 13) {         // If the received byte is = to 13, end of transmission
      IndiceCount = 0;
      StartCount = 0;
      for (int i = 1; i < 7; i++) { // Verifies if the received command starts with $GPGGA
        if (buffer[i] == StartCommand[i - 1]) {
          StartCount++;
        }
      }
      if (StartCount == 6) {     // If yes, continue and process the data
        for (int i = 0; i < 300; i++) {
          if (buffer[i] == ',') { // check for the position of the  "," separator
            indices[IndiceCount] = i;
            IndiceCount++;
          }
          if (buffer[i] == '*') { // ... and the "*"
            indices[12] = i;
            IndiceCount++;
          }
        }
        // Load the NMEA time data
        temp = indices[0];
        hours = (buffer[temp + 1] - 48) * 10 + buffer[temp + 2] - 48;
        minutes = (buffer[temp + 3] - 48) * 10 + buffer[temp + 4] - 48;
        seconds = (buffer[temp + 5] - 48) * 10 + buffer[temp + 6] - 48;
        // Load latitude and logitude data
        temp = indices[1];
        Lat10 = buffer[temp + 1] - 48;
        Lat1 = buffer[temp + 2] - 48;
        dLat1 = buffer[temp + 3] - 48;
        dLat2 = buffer[temp + 4] - 48;
        temp = indices[2];
        NS = buffer[temp + 1];
        temp = indices[3];
        Lon100 = buffer[temp + 1] - 48;
        Lon10 = buffer[temp + 2] - 48;
        Lon1 = buffer[temp + 3] - 48;
        dLon1 = buffer[temp + 4] - 48;
        dLon2 = buffer[temp + 5] - 48;
        temp = indices[4];
        EW = buffer[temp + 1];
        temp = indices[5];
        validGPSflag = buffer[temp + 1] - 48;
        temp = indices[6];
        // Load the NMEA satellite data
        sat10 = buffer[temp + 1] - 48;
        sat1 = buffer[temp + 2] - 48;
      }

      else
      { // NMEA $GPRMC data begins here
        // Note: &GPRMC does not include satellite in view data
        IndiceCount = 0;
        StartCount = 0;
        for (int i = 1; i < 7; i++) { // Verifies if the received command starts with $GPRMC
          if (buffer[i] == StartCommand2[i - 1]) {
            StartCount++;
          }
        }
        if (StartCount == 6) {     // If yes, continue and process the data
          for (int i = 0; i < 300; i++) {
            if (buffer[i] == ',') { // check for the position of the  "," separator
              indices[IndiceCount] = i;
              IndiceCount++;
            }
            if (buffer[i] == '*') { // ... and the "*"
              indices[12] = i;
              IndiceCount++;
            }
          }
          // Load time data
          temp = indices[0];
          hours = (buffer[temp + 1] - 48) * 10 + buffer[temp + 2] - 48;
          minutes = (buffer[temp + 3] - 48) * 10 + buffer[temp + 4] - 48;
          seconds = (buffer[temp + 5] - 48) * 10 + buffer[temp + 6] - 48;
          temp = indices[1];
          if (buffer[temp + 1] == 65) {
            validGPSflag = 1;
          }
          else
          {
            validGPSflag = 0;
          }
          // Load latitude and logitude data
          temp = indices[2];
          Lat10 = buffer[temp + 1] - 48;
          Lat1 = buffer[temp + 2] - 48;
          dLat1 = buffer[temp + 3] - 48;
          dLat2 = buffer[temp + 4] - 48;
          temp = indices[3];
          NS = buffer[temp + 1];
          temp = indices[4];
          Lon100 = buffer[temp + 1] - 48;
          Lon10 = buffer[temp + 2] - 48;
          Lon1 = buffer[temp + 3] - 48;
          dLon1 = buffer[temp + 4] - 48;
          dLon2 = buffer[temp + 5] - 48;
          temp = indices[5];
          EW = buffer[temp + 1];
        }
      }
      counter = 0;                // Reset the buffer
      for (int i = 0; i < 300; i++) buffer[i] = ' ';
    }
  }
}



//******************************************************************
//  Si5351 processing follows:
//  Generates the Si5351 clock generator frequency message
//
//  Called by sketch setup() and transmit()
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

// Called by sketch setup, transmit(), si5351aSetFreq, and
// si5351aStart functions.
//******************************************************************
uint8_t Si5351_write(uint8_t addr, uint8_t data)
{
  Wire.beginTransmission(Si5351A_addr);
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}
