# Auto-Calibrated-GPS-RTC-Si5351A-FST4W-and-WSPR-MEPT

(V3 now uses Gaussian pulse width shaping for FST4W.)

Note: The FST4W modes require accurate system timing and frequency stability, especially when using the slower modes. The GPS 1 pps method of frequency calibration used in this sketch does not frequency stabilize the Si5351A during transmit periods. 

This Manned Experimental Propagation Transmitter (MEPT) uses the very popular Arduino Nano and Si5351A clock generator board to generate LF – 10 meter FST4W or WSPR signals. Frequency and time accuracy are maintained by a highly accurate temperature compensated DS3231 real time clock (RTC) board or by using a GPS receiver.


                       Guide to updated scheduling configuration follows:

  FORMAT:    (TIME,BAND,MODE),

  TIME:    WSPR & FST4w are in 2 minute increments past the top of the hour
           FST4W300 is in 5 minute increments past the top of the hour

  MODE:    0-FST4W120; 1-FSR4W300; 2-FST4W900; 3-FST4W1800; 4-WSPR

  BAND:    is 'TXdialFreq' band number  i.e. 2 = 1836600 kHz



  NOTE: 
  
  Ensure timeslots do not overlap when using the 5, 15, and 30 minute FST4W modes

  FST4W modes are normally used only on LF and MF

  Ensure format is correct
  i.e. brackets TIME comma MODE comma BAND comma brackets comma {24,1,11},

  Maximum 'schedule' length is 30 time slots.
  Ensure the end of the 'schedule' list ends in (-1,-1,-1),



  EXAMPLE:

  const int schedule [][3] = {
  
  {0, 4, 1},   // WSPR at top of the hour on 474.200 kHz
  
  {2, 0, 1},   // FST4W120 at 2 minutes past the hour on 474.200 kHz
  
  {4, 4, 9},   // WSPR at 4 minutes past the hour on 10138.700 kHz
  
  {20, 1, 0},  // FST4W300 at 20 minutes past the hour on 136.000 kHz
  
  {58, 4, 14}, // WSPR at 58 minutes past the hour on 28124600 kHz
  
  (-1,-1,-1),  // End of schedule flag
  };
 
