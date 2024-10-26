#ifndef BEENHERE
#include "SDT.h"
#endif

#ifdef V12HWR // changes are so extensive for calibration in V12, this file supports only V12

// Updates to DoReceiveCalibration() and DoXmitCalibrate() functions by .  July 20, 2023
// Updated PlotCalSpectrum() function to clean up graphics.   August 3, 2023
// Major clean-up of calibration.   August 16, 2023

int val;
int corrChange;
float correctionIncrement;  //AFP 2-7-23
int userScale, userZoomIndex, userXmtMode;
int transmitPowerLevelTemp;
float corrFactorTemp = 0.8;
int timer1;
char strBuf[100];

/*****
  Purpose: Set up prior to IQ calibrations.  Revised function. AFP 07-13-24
  These things need to be saved here and restored in the prologue function:
  Vertical scale in dB  (set to 10 dB during calibration)
  Zoom, set to 1X in receive and 4X in transmit calibrations.
  Transmitter power, set to 5W during both calibrations.
   Parameter List:
      int setZoom   (This parameter should be 0 for receive (1X) and 2 (4X) for transmit)

   Return value:
      void
 *****/
static long userCurrentFreq;
static long userCenterFreq;
static long userTxRxFreq;
static long userNCOFreq;
void CalibratePreamble(int setZoom) {
  calOnFlag = 1;
  corrChange = 0;
  correctionIncrement = 0.01;  //AFP 2-7-23
  IQCalType = 0;
  radioState = CW_TRANSMIT_STRAIGHT_STATE;  // 
  userXmtMode = xmtMode;          // Store the user's mode setting.   July 22, 2023
  userZoomIndex = spectrum_zoom;  // Save the zoom index so it can be reset at the conclusion.   August 12, 2023
  zoomIndex = 1;
  spectrum_zoom = setZoom; // spectrum_zoom is used in Process.cpp. zoom_display has no effect
  userCurrentFreq = currentFreq;
  userTxRxFreq = TxRxFreq;
  userNCOFreq = NCOFreq;
  userCenterFreq = centerFreq;
  TxRxFreq = centerFreq;
  currentFreq = TxRxFreq;
  NCOFreq = 0L;
  tft.clearScreen(RA8875_BLACK);  //AFP07-13-24
  //ButtonZoom();
  ResetTuning(); 
  tft.writeTo(L1);  // Always exit function in L1.   August 15, 2023
  DrawBandWidthIndicatorBar();
  ShowSpectrumdBScale();
  DrawFrequencyBarValue();
  ShowFrequency();
  tft.writeTo(L2);  // Erase the bandwidth bar.   August 16, 2023
  tft.clearMemory();
  tft.writeTo(L1);
  tft.setFontScale((enum RA8875tsize)0);
  tft.setTextColor(RA8875_GREEN);
  tft.setCursor(550, 160);
  tft.print("Dir Freq - Gain/Phase");
  tft.setCursor(550, 175);
  tft.print("User1 - Incr");
  tft.setTextColor(RA8875_CYAN);
  tft.fillRect(350, 125, 100, tft.getFontHeight(), RA8875_BLACK);
  tft.fillRect(0, 272, 517, 399, RA8875_BLACK);  // Erase waterfall.   August 14, 2023
  tft.setCursor(400, 125);
  tft.print("dB");
  tft.setCursor(350, 110);
  tft.print("Incr= ");
  tft.setCursor(400, 110);
  tft.print(correctionIncrement, 3);
  userScale = currentScale;  //  Remember user preference so it can be reset when done.  
  currentScale = 1;          //  Set vertical scale to 10 dB during calibration.  
  updateDisplayFlag = 0;
  xrState = RECEIVE_STATE;
  T41State = CW_RECEIVE;
  modeSelectInR.gain(0, 1);
  modeSelectInL.gain(0, 1);
  modeSelectInExR.gain(0, 0);
  modeSelectInExL.gain(0, 0);
  modeSelectOutL.gain(0, 1);
  modeSelectOutR.gain(0, 1);
  modeSelectOutL.gain(1, 0);
  modeSelectOutR.gain(1, 0);
  modeSelectOutExL.gain(0, 1);
  modeSelectOutExR.gain(0, 1);
  
  xrState = TRANSMIT_STATE;
  digitalWrite(RXTX, HIGH);  // Turn on transmitter.
  ShowTransmitReceiveStatus();
  ShowSpectrumdBScale();
}

/*****
  Purpose: Shut down calibtrate and clean up after IQ calibrations.  Revised function.  AFP 07-13-24

   Parameter List:
      void

   Return value:
      void
 *****/
void CalibratePost() {
  digitalWrite(RXTX, LOW);  // Turn off the transmitter.
  digitalWrite(CAL, CAL_OFF);
  SetRF_InAtten(currentRF_InAtten);
  SetRF_OutAtten(currentRF_OutAtten);
  updateDisplayFlag = 0;
  xrState = RECEIVE_STATE;
  T41State = CW_RECEIVE;
  Q_in_L.clear();
  Q_in_R.clear();
  currentFreq = userCurrentFreq;
  TxRxFreq = userTxRxFreq;
  NCOFreq = userNCOFreq;
  centerFreq = userCenterFreq;
  xrState = RECEIVE_STATE;
  calibrateFlag = 0;  // this was set when the Calibration menu option was selected
  calFreqShift = 0;
  currentScale = userScale;  //  Restore vertical scale to user preference.  
  //ShowSpectrumdBScale();
  xmtMode = userXmtMode;                        // Restore the user's floor setting.   July 27, 2023
  transmitPowerLevel = transmitPowerLevelTemp;  // Restore the user's transmit power level setting.   August 15, 2023
  EEPROMWrite();                                // Save calibration numbers and configuration.   August 12, 2023
  zoomIndex = userZoomIndex - 1;
  spectrum_zoom = userZoomIndex;
  ButtonZoom();     // Restore the user's zoom setting.  Note that this function also modifies spectrum_zoom.
  EEPROMWrite();    // Save calibration numbers and configuration.   August 12, 2023
  tft.writeTo(L2);  // Clear layer 2.   July 31, 2023
  tft.clearMemory();
  tft.writeTo(L1);  // Exit function in layer 1.   August 3, 2023
  RedrawDisplayScreen();
  IQChoice = 5; // this is the secondary menu choice. equivalent to Cancel
  calOnFlag = 0;
  mainMenuWindowActive = 0;
  radioState = CW_RECEIVE_STATE;  // 
  SetFreq();                      // Return Si5351 to normal operation mode.  
  lastState = 1111;               // This is required due to the function deactivating the receiver.  This forces a pass through the receiver set-up code.   October 16, 2023
  return;
}

/*====
  Purpose: Combined input/ output for the purpose of calibrating the receive IQ

   Parameter List:
      void

   Return value:
      void
 *****/
void DoReceiveCalibrate() {
  //=======================
  int task = -1;
  int lastUsedTask = -2;
  tft.setTextColor(RA8875_CYAN);
  CalibratePreamble(0);
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_CYAN);
  tft.setCursor(550, 300);
  tft.print("Receive I/Q ");
  tft.setCursor(550, 350);
  tft.print("Calibrate");
  if (bands[currentBand].mode == DEMOD_LSB) calFreqShift = 20500;  //  LSB offset. AFP
  if (bands[currentBand].mode == DEMOD_USB) calFreqShift = 21500;  //  USB offset.
  Clk2SetFreq = ((centerFreq + calFreqShift) * SI5351_FREQ_MULT);
  SetFreq();
  si5351.output_enable(SI5351_CLK2, 1);
  si5351.set_freq(Clk2SetFreq, SI5351_CLK2);
  digitalWrite(XMIT_MODE, XMIT_CW);
  digitalWrite(KEY2, HIGH);
  digitalWrite(CW_ON_OFF, CW_ON);
  digitalWrite(CAL, CAL_ON);
  SetRF_InAtten(30);
  int out_atten = 30;
  int previous_atten = out_atten;
  SetRF_OutAtten(out_atten);
  zoomIndex = 0;
  calTypeFlag = 0;  // RX cal

  while (true) {
    val = ReadSelectedPushButton();
    if (val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
      if (val != lastUsedTask && task == -100) task = val;
      else task = BOGUS_PIN_READ;
    }
    ShowSpectrum2(); // this is where the IQ data processing is applied
    val = ReadSelectedPushButton();
    if (val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
      if (val != lastUsedTask && task == -100) task = val;
      else task = BOGUS_PIN_READ;
    }
    switch (task) {
        // Toggle gain and phase
      case CAL_CHANGE_TYPE:
        IQCalType = !IQCalType;
        break;
      case CAL_CHANGE_INC: 
        corrChange = !corrChange;
        if (corrChange == 1) {
          correctionIncrement = 0.001;  //AFP 2-7-23
        } else {                        //if (corrChange == 0)                   // corrChange is a toggle, so if not needed JJP 2/5/23
          correctionIncrement = 0.01;   //AFP 2-7-23
        }
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(400, 110, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setCursor(400, 110);
        tft.print(correctionIncrement, 3);
        break;

      case MENU_OPTION_SELECT:
        tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
        EEPROMData.IQAmpCorrectionFactor[currentBand] = IQAmpCorrectionFactor[currentBand];
        EEPROMData.IQPhaseCorrectionFactor[currentBand] = IQPhaseCorrectionFactor[currentBand];
        IQChoice = 6;
        break;
      default:
        break;
    }                                     // End switch
    if (task != -1) lastUsedTask = task;  //  Save the last used task.
    task = -100;                          // Reset task after it is used.
    if (IQCalType == 0) {                 // AFP 2-11-23
      IQAmpCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQAmpCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Gain");
    } else {
      IQPhaseCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQPhaseCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Phase");
    }
    // Adjust the value of the TX attenuator:
    out_atten = GetFineTuneValueLive(0,31,out_atten,1,(char *)"Out Atten");
    // Update via I2C if the attenuation value changed
    if (out_atten != previous_atten){
        SetRF_OutAtten(out_atten);
        previous_atten = out_atten;
    }
    if (val == MENU_OPTION_SELECT) {
      break;
    }
  }
  si5351.output_enable(SI5351_CLK2, 0);
  CalibratePost();
}


/*****
  Purpose: Combined input/ output for the purpose of calibrating the transmit IQ

   Parameter List:
      void

   Return value:
      void

   CAUTION: Assumes a spaces[] array is defined
 *****/
void DoXmitCalibrate() {
  int task = -1;
  int lastUsedTask = -2;
  CalibratePreamble(0);

  calibrateFlag = 0;
  //Serial.println("in DoXmitCalibrate");
  int setZoom = 1;
  int corrChange = 0;
  int val;
//  int userScale;
  tft.setFontScale((enum RA8875tsize)1);
  tft.setTextColor(RA8875_CYAN);
  tft.setCursor(550, 300);
  tft.print("Transmit I/Q ");
  tft.setCursor(550, 350);
  tft.print("Calibrate");
  IQChoice = 3;
  SetRF_InAtten(30);
  SetRF_OutAtten(20);
//  int userFloor = currentNoiseFloor[currentBand];  // Store the user's floor setting.
  //zoomIndex = 0;
  calTypeFlag = 1;
  calOnFlag = 1;  //10-20-24
  IQEXChoice = 0;
  corrChange = 0;                 //10-20-24
  IQCalType = 0;                  //10-20-24
  userXmtMode = xmtMode;          // Store the user's mode setting.   July 22, 2023
  userZoomIndex = spectrum_zoom;  // Save the zoom index so it can be reset at the conclusion.   August 12, 2023
  zoomIndex = setZoom - 1;
  //ButtonZoom();

  T41State = SSB_XMIT;
  digitalWrite(CAL, CAL_ON);  // Turn on transmitter.
  digitalWrite(KEY1, LOW);   // Turn on transmitter.
  digitalWrite(XMIT_MODE, XMIT_SSB);  // Turn on transmitter.
  ShowTransmitReceiveStatus();
  while (true) {
    ShowSpectrum2();
    val = ReadSelectedPushButton();
    if (val != BOGUS_PIN_READ) {
      val = ProcessButtonPress(val);
      if (val != lastUsedTask && task == -100) task = val;
      else task = BOGUS_PIN_READ;
    }
    switch (task) {
      // Toggle gain and phase
      case (UNUSED_1):
        IQEXChoice = !IQEXChoice;  //IQEXChoice=0, Gain  IQEXChoice=1, Phase
        break;
      // Toggle increment value
      case (UNUSED_2):  //
        corrChange = !corrChange;
        if (corrChange == 1) {          // Toggle increment value
          correctionIncrement = 0.001;  // AFP 2-11-23
        } else {
          correctionIncrement = 0.01;  // AFP 2-11-23
        }
        tft.setFontScale((enum RA8875tsize)0);
        tft.fillRect(400, 110, 50, tft.getFontHeight(), RA8875_BLACK);
        tft.setCursor(400, 110);
        tft.print(correctionIncrement, 3);
        break;
      case (MENU_OPTION_SELECT):  // Save values and exit calibration.
        tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
        EEPROMData.IQXAmpCorrectionFactor[currentBandA] = IQAmpCorrectionFactor[currentBandA];
        EEPROMData.IQXPhaseCorrectionFactor[currentBandA] = IQPhaseCorrectionFactor[currentBandA];
        tft.fillRect(SECONDARY_MENU_X, MENUS_Y, EACH_MENU_WIDTH + 35, CHAR_HEIGHT, RA8875_BLACK);
        IQChoice = 6;  // AFP 2-11-23
        break;
      default:
        break;
    }  // end switch
    //  Need to remember the last used task;
    if (task != -1) lastUsedTask = task;  //  Save the last used task.
    task = -100;                          // Reset task after it is used.
    //  Read encoder and update values.
    //Serial.println("353");
    if (IQEXChoice == 0) {
      //Serial.println("354");
      IQXAmpCorrectionFactor[currentBandA] = GetEncoderValueLive(-2.0, 2.0, IQXAmpCorrectionFactor[currentBandA], correctionIncrement, (char *)"IQ Gain X");
      //IQAmpCorrectionFactor[currentBand] = GetEncoderValueLive(-2.0, 2.0, IQAmpCorrectionFactor[currentBand], correctionIncrement, (char *)"IQ Gain");
      Serial.print("IQXAmpCorrectionFactor= ");
      Serial.println(IQXAmpCorrectionFactor[currentBandA]);
    } else {
      IQXPhaseCorrectionFactor[currentBandA] = GetEncoderValueLive(-2.0, 2.0, IQXPhaseCorrectionFactor[currentBandA], correctionIncrement, (char *)"IQ Phase X");
      Serial.print("IQXPhaseCorrectionFactor= ");
      Serial.println(IQXPhaseCorrectionFactor[currentBandA]);
    }

    if (val == MENU_OPTION_SELECT) {
      //CalibratePost();
      break;
    }
  }  // end while
  CalibratePost();
  // Clean up and exit to normal operation.
}


/*****
  Purpose: Signal processing for th purpose of calibrating the transmit IQ

   Parameter List:
      void

   Return value:
      void
 *****/
void ProcessIQData2() {
  //Serial.println("in ProcessIQData2");
  float bandCouplingFactor[NUMBER_OF_BANDS] = { 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5, 0.5 }; 
  float bandOutputFactor;                                               // AFP 2-11-23
  float rfGainValue;                                                    // AFP 2-11-23
  float recBandFactor[NUMBER_OF_BANDS] = { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 };
  /**********************************************************************************  AFP 12-31-20
        Get samples from queue buffers
        Teensy Audio Library stores ADC data in two buffers size=128, Q_in_L and Q_in_R as initiated from the audio lib.
        Then the buffers are read into two arrays sp_L and sp_R in blocks of 128 up to N_BLOCKS.  The arrarys are
        of size BUFFER_SIZE * N_BLOCKS.  BUFFER_SIZE is 128.
        N_BLOCKS = FFT_LENGTH / 2 / BUFFER_SIZE * (uint32_t)DF; // should be 16 with DF == 8 and FFT_LENGTH = 512
        BUFFER_SIZE*N_BLOCKS = 2048 samples
     **********************************************************************************/

  bandOutputFactor = bandCouplingFactor[currentBand] * CWPowerCalibrationFactor[currentBand] / CWPowerCalibrationFactor[1];  //AFP 2-7-23

  // Generate I and Q for the transmit or receive calibration.  
  if (IQChoice == 2 || IQChoice == 3) {                                   // 
                                                                          // Serial.println("IQChoice 2");
    arm_scale_f32(cosBuffer3, bandOutputFactor, float_buffer_L_EX, 256);  // AFP 2-11-23 Use pre-calculated sin & cos instead of Hilbert
    arm_scale_f32(sinBuffer3, bandOutputFactor, float_buffer_R_EX, 256);  // AFP 2-11-23 Sidetone = 3000
  }

  if (bands[currentBand].mode == DEMOD_LSB) {
    arm_scale_f32(float_buffer_L_EX, -IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);        //Adjust level of L buffer // AFP 2-11-23
    // KI3P merge notes: changed this from IQPhase... to IQXPhase...
    IQXPhaseCorrection(float_buffer_L_EX, float_buffer_R_EX, IQXPhaseCorrectionFactor[currentBand], 256);  // Adjust phase
  } else {
    if (bands[currentBand].mode == DEMOD_USB) {
      arm_scale_f32(float_buffer_L_EX, IQXAmpCorrectionFactor[currentBand], float_buffer_L_EX, 256);  // AFP 2-11-23
      IQXPhaseCorrection(float_buffer_L_EX, float_buffer_R_EX, IQXPhaseCorrectionFactor[currentBand], 256);
    }
  }
  //24KHz effective sample rate here
  arm_fir_interpolate_f32(&FIR_int1_EX_I, float_buffer_L_EX, float_buffer_LTemp, 256);
  arm_fir_interpolate_f32(&FIR_int1_EX_Q, float_buffer_R_EX, float_buffer_RTemp, 256);

  // interpolation-by-4,  48KHz effective sample rate here
  arm_fir_interpolate_f32(&FIR_int2_EX_I, float_buffer_LTemp, float_buffer_L_EX, 512);
  arm_fir_interpolate_f32(&FIR_int2_EX_Q, float_buffer_RTemp, float_buffer_R_EX, 512);

  // are there at least N_BLOCKS buffers in each channel available ?
  if ((uint32_t)Q_in_L.available() > N_BLOCKS + 0 && (uint32_t)Q_in_R.available() > N_BLOCKS + 0) {
    // Revised I and Q calibration signal generation using large buffers. 
    q15_t q15_buffer_LTemp[2048];  //
    q15_t q15_buffer_RTemp[2048];  //
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::NON_STALLING);
    arm_float_to_q15(float_buffer_L_EX, q15_buffer_LTemp, 2048);
    arm_float_to_q15(float_buffer_R_EX, q15_buffer_RTemp, 2048);
    Q_out_L_Ex.play(q15_buffer_LTemp, 2048);
    Q_out_R_Ex.play(q15_buffer_RTemp, 2048);
    Q_out_L_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);
    Q_out_R_Ex.setBehaviour(AudioPlayQueue::ORIGINAL);

    usec = 0;
    for (unsigned i = 0; i < N_BLOCKS; i++) {
      sp_L1 = Q_in_R.readBuffer(); // this is not a mistake. L and R must have got switched at 
      sp_R1 = Q_in_L.readBuffer(); // some point and the calibration process incorporates it

      /**********************************************************************************  AFP 12-31-20
          Using arm_Math library, convert to float one buffer_size.
          Float_buffer samples are now standardized from > -1.0 to < 1.0
      **********************************************************************************/
      arm_q15_to_float(sp_L1, &float_buffer_L[BUFFER_SIZE * i], BUFFER_SIZE);  // convert int_buffer to float 32bit
      arm_q15_to_float(sp_R1, &float_buffer_R[BUFFER_SIZE * i], BUFFER_SIZE);  // convert int_buffer to float 32bit
      Q_in_L.freeBuffer();
      Q_in_R.freeBuffer();
    }
    rfGainValue = pow(10, (float)rfGainAllBands / 20);                                   //AFP 2-11-23
    arm_scale_f32(float_buffer_L, rfGainValue, float_buffer_L, BUFFER_SIZE * N_BLOCKS);  //AFP 2-11-23
    arm_scale_f32(float_buffer_R, rfGainValue, float_buffer_R, BUFFER_SIZE * N_BLOCKS);  //AFP 2-11-23

    /**********************************************************************************  AFP 12-31-20
      Scale the data buffers by the RFgain value defined in bands[currentBand] structure
    **********************************************************************************/
    arm_scale_f32(float_buffer_L, recBandFactor[currentBand], float_buffer_L, BUFFER_SIZE * N_BLOCKS);  //AFP 2-11-23
    arm_scale_f32(float_buffer_R, recBandFactor[currentBand], float_buffer_R, BUFFER_SIZE * N_BLOCKS);  //AFP 2-11-23

    // Manual IQ amplitude correction
    if (bands[currentBand].mode == DEMOD_LSB) {
      //Serial.println("if  rec demod lsb");
      arm_scale_f32(float_buffer_L, -IQAmpCorrectionFactor[currentBand], float_buffer_L, BUFFER_SIZE * N_BLOCKS);  //AFP 04-14-22
      IQPhaseCorrection(float_buffer_L, float_buffer_R, IQPhaseCorrectionFactor[currentBand], BUFFER_SIZE * N_BLOCKS);
    } else {
      if (bands[currentBand].mode == DEMOD_USB) {
        arm_scale_f32(float_buffer_L, IQAmpCorrectionFactor[currentBand], float_buffer_L, BUFFER_SIZE * N_BLOCKS);  //AFP 04-14-22  changed sign
        IQPhaseCorrection(float_buffer_L, float_buffer_R, IQPhaseCorrectionFactor[currentBand], BUFFER_SIZE * N_BLOCKS);
      }
    }

    //spectrum_zoom = zoomIndex;
    spectrum_zoom = SPECTRUM_ZOOM_1;
    // Serial.print("BUFFER_SIZE * N_BLOCKS= ");Serial.println(BUFFER_SIZE * N_BLOCKS);
    if (spectrum_zoom == SPECTRUM_ZOOM_1) {  // && display_S_meter_or_spectrum_state == 1)
      zoom_display = 1;
      CalcZoom1Magn();  //AFP Moved to display function
    }
 
    if (auto_codec_gain == 1) {
      Codec_gain();
    }
  }
}

/*****
  Purpose: Show Spectrum display modified for IQ calibration.
           This is similar to the function used for normal reception, however, it has
           been simplified and streamlined for calibration.

  Parameter list:
    void

  Return value;
    void
*****/
void ShowSpectrum2()  //AFP 2-10-23
{
  int x1 = 0;
  float adjdB = 0.0;
  int capture_bins = 10;  // Sets the number of bins to scan for signal peak.
  //=========== // AFP 2-11-23

  pixelnew[0] = 0;
  pixelnew[1] = 0;
  pixelold[0] = 0;
  pixelold[1] = 0;

  //  This is the "spectra scanning" for loop.  During calibration, only small areas of the spectrum need to be examined.
  //  If the entire 512 wide spectrum is used, the calibration loop will be slow and unresponsive.
  //  The scanning areas are determined by receive versus transmit calibration, and LSB or USB.  Thus there are 4 different scanning zones.
  //  All calibrations use a 0 dB reference signal and an "undesired sideband" signal which is to be minimized relative to the reference.
  //  Thus there is a target "bin" for the reference signal and another "bin" for the undesired sideband.
  //  The target bin locations are used by the for-loop to sweep a small range in the FFT.  A maximum finding function finds the peak signal strength.

  /*************************************
  ProcessIQData2 performs an N-point (SPECTRUM_RES = 512) FFT on the data in float_buffer_L and 
  float_buffer_R when they fill up. The data in float_buffer_L and float_buffer_R are sampled at 
  192000 Hz. The length of the float_buffer_L and float_buffer_R buffers is BUFFER_SIZE * N_BLOCKS 
  = 128*16 = 2048, of which the FFT only uses the first 512 points.
    N_BLOCKS = FFT_LENGTH / 2 / BUFFER_SIZE * (uint32_t)DF
             = 512 / 2 / 128 * 8
             = 16
  Therefore the bin width of each FFT bin is SAMPLE_RATE / FFT_LEN = 192000 / 512 = 375 Hz.
  The frequency of the middle bin is centerFreq + IFFreq and our spectrum spans 
  (centerFreq + IFFreq - SAMPLE_RATE/2) to (centerFreq + IFFreq + SAMPLE_RATE/2).

  So the equation for bin number n given frequency Clk2SetFreq is:
    n = (Clk2SetFreq - Clk1SetFreq)/375 + 256
      = (Clk2SetFreq - (centerFreq + IFFreq))/375 + 256

  We set Clk2SetFreq to centerFreq + calFreqShift
  So we expect the desired tone to appear in bin
    n_tone = (calFreqShift - IFFreq)/375 + 256
  while the undesired image product will be at 
    n_image= (IFFreq - calFreqShift)/375 + 256

  For LSB, calFreqShift = 20500 and:
    n_tone = (20500-48000)/375 + 256 = 182
    n_image= (48000-20500)/375 + 256 = 329

  And for USB, calFreqShift = 21500 so:
    n_tone = (21500-48000)/375 + 256 = 185
    n_image= (48000-21500)/375 + 256 = 326
  *********************************************/

  int cal_bins[2] = { 0, 0 };
  if (calTypeFlag == 0 && bands[currentBand].mode == DEMOD_LSB) {
    cal_bins[0] = 182;
    cal_bins[1] = 329;
  }  // Receive calibration, LSB.  
  if (calTypeFlag == 0 && bands[currentBand].mode == DEMOD_USB) {
    cal_bins[0] = 185;
    cal_bins[1] = 326;
  }  // Receive calibration, USB.  
  if (calTypeFlag == 1 && bands[currentBand].mode == DEMOD_LSB) {
    cal_bins[0] = 247;
    cal_bins[1] = 265;
  }  // Transmit calibration, LSB.  
  if (calTypeFlag == 1 && bands[currentBand].mode == DEMOD_USB) {
    cal_bins[0] = 265;
    cal_bins[1] = 247;
  }  // Transmit calibration, USB.  

  // Draw vertical markers for the reference and undesired sideband locations.  For debugging only!
  //tft.drawFastVLine(cal_bins[0], SPECTRUM_TOP_Y, h, RA8875_GREEN);
  //tft.drawFastVLine(cal_bins[1], SPECTRUM_TOP_Y, h, RA8875_GREEN);

  //  There are 2 for-loops, one for the reference signal and another for the undesired sideband.  
  if (calTypeFlag == 0) {
    for (x1 = cal_bins[0] - capture_bins; x1 < cal_bins[0] + capture_bins; x1++) adjdB = PlotCalSpectrum(x1, cal_bins, capture_bins);
    for (x1 = cal_bins[1] - capture_bins; x1 < cal_bins[1] + capture_bins; x1++) adjdB = PlotCalSpectrum(x1, cal_bins, capture_bins);
  } else {
    // Plot carrier during transmit cal, do not return a dB value:
    if (calTypeFlag == 1) {
      for (x1 = cal_bins[0] - capture_bins; x1 < cal_bins[0] + capture_bins; x1++) adjdB = PlotCalSpectrumEx(x1, cal_bins, capture_bins);
      for (x1 = cal_bins[1] - capture_bins; x1 < cal_bins[1] + capture_bins; x1++) adjdB = PlotCalSpectrumEx(x1, cal_bins, capture_bins);
      //for (x1 = cal_bins[0] - capture_bins; x1 < cal_bins[0] + capture_bins; x1++) adjdB = PlotCalSpectrum(x1, cal_bins, capture_bins);
      //for (x1 = cal_bins[1] + 20; x1 < cal_bins[1] - 20; x1++) PlotCalSpectrum(x1, cal_bins, capture_bins);
    }
  }
  // Finish up:
  //= AFP 2-11-23
  tft.fillRect(350, 125, 50, tft.getFontHeight(), RA8875_BLACK);
  tft.setFontScale((enum RA8875tsize)0);
  tft.setCursor(350, 125);  // 350, 125
  tft.print(adjdB, 1);

  tft.BTE_move(WATERFALL_LEFT_X, FIRST_WATERFALL_LINE, MAX_WATERFALL_WIDTH, MAX_WATERFALL_ROWS - 2, WATERFALL_LEFT_X, FIRST_WATERFALL_LINE + 1, 1, 2);
  while (tft.readStatus())
    ;
  tft.BTE_move(WATERFALL_LEFT_X, FIRST_WATERFALL_LINE + 1, MAX_WATERFALL_WIDTH, MAX_WATERFALL_ROWS - 2, WATERFALL_LEFT_X, FIRST_WATERFALL_LINE + 1, 2);
  while (tft.readStatus())
    ;  // Make sure it's done.
}

/*****
  Purpose:  Plot Receive Calibration Spectrum   //   7/2/2023
            This function plots a partial spectrum during calibration only.
            This is intended to increase the efficiency and therefore the responsiveness of the calibration encoder.
            This function is called by ShowSpectrum2() in two for-loops.  One for-loop is for the refenence signal,
            and the other for-loop is for the undesired sideband.
  Parameter list:
    int x1, where x1 is the FFT bin.
    cal_bins[2] locations of the desired and undesired signals
    capture_bins width of the bins used to display the signals
  Return value;
    float returns the adjusted value in dB
*****/
float PlotCalSpectrum(int x1, int cal_bins[2], int capture_bins) {
  //Serial.println("in PlotCalSpectrum");
  float adjdB = 0.0;
  int16_t adjAmplitude = 0;  // Was float; cast to float in dB calculation.  
  int16_t refAmplitude = 0;  // Was float; cast to float in dB calculation.  
  uint32_t index_of_max;     // This variable is not currently used, but it is required by the ARM max function.  
  if (x1 == (cal_bins[0] - capture_bins)) {  // Set flag at revised beginning.  
    updateDisplayFlag = 1;                   //Set flag so the display data are saved only once during each display refresh cycle at the start of the cycle, not 512 times 
  } else updateDisplayFlag = 0;  //  Do not save the the display data for the remainder of the

  //-------------------------------------------------------
  // This block of code, which calculates the latest FFT and finds the maxima of the tone
  // and its image product, does not technically need to be run every time we plot a pixel 
  // on the screen. However, according to the comments below this is needed to eliminate
  // conflicts.
  ProcessIQData2();  // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays
  // Find the maximums of the desired and undesired signals.
  if (bands[currentBand].mode == DEMOD_LSB) {
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
    //arm_max_q15(&pixelnew[(265 - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
  } else {
    if (bands[currentBand].mode == DEMOD_USB) {
      arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
      arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    }
  }
  //-------------------------------------------------------
  
  y_new = pixelnew[x1];
  y1_new = pixelnew[x1 - 1];
  y_old = pixelold[x1];
  y_old2 = pixelold[x1 - 1];

  //=== // AFP 2-11-23
  if (y_new > base_y) y_new = base_y;
  if (y_old > base_y) y_old = base_y;
  if (y_old2 > base_y) y_old2 = base_y;
  if (y1_new > base_y) y1_new = base_y;

  if (y_new < 0) y_new = 0;
  if (y_old < 0) y_old = 0;
  if (y_old2 < 0) y_old2 = 0;
  if (y1_new < 0) y1_new = 0;

  // Erase the old spectrum and draw the new spectrum.
  tft.drawLine(x1, spectrumNoiseFloor - y_old2, x1, spectrumNoiseFloor - y_old, RA8875_BLACK);   // Erase old...
  tft.drawLine(x1, spectrumNoiseFloor - y1_new, x1, spectrumNoiseFloor - y_new, RA8875_YELLOW);  // Draw new
  pixelCurrent[x1] = pixelnew[x1];                                                               //  This is the actual "old" spectrum!  This is required due to CW interrupts.  Copied to pixelold by the FFT function.

  if (calTypeFlag == 0) {  // Receive Cal
    adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;
    tft.writeTo(L2);
    if (bands[currentBand].mode == DEMOD_LSB) {
      tft.fillRect(cal_bins[0] - capture_bins, SPECTRUM_TOP_Y + 20, 2*capture_bins, h - 6, RA8875_BLUE);     // SPECTRUM_TOP_Y = 100
      tft.fillRect(cal_bins[1] - capture_bins, SPECTRUM_TOP_Y + 20, 2*capture_bins, h - 6, DARK_RED);  // h = SPECTRUM_HEIGHT + 3
    } else { 
      if(bands[currentBand].mode == DEMOD_USB)                                                                                 // SPECTRUM_HEIGHT = 150 so h = 153
      tft.fillRect(cal_bins[0] - capture_bins, SPECTRUM_TOP_Y + 20, 2*capture_bins, h - 6, DARK_RED);
      tft.fillRect(cal_bins[1] - capture_bins, SPECTRUM_TOP_Y + 20, 2*capture_bins, h - 6, RA8875_BLUE);
    }
  } 

  tft.writeTo(L1);
  return adjdB;
}

/*****
  Purpose:  Plot Calibration Xmit Spectrum   //  AFP 10/17/2024
            This function plots a partial spectrum during calibration only for the Xmit Cal.
            This is intended to increase the efficiency and therefore the responsiveness of the calibration encoder.
            This function is called by ShowSpectrum2() in two for-loops.  One for-loop is for the refenence signal,
            and the other for-loop is for the undesired sideband.
  Parameter list:
    int x1, where x1 is the FFT bin.
    cal_bins[2] locations of the desired and undesired signals
    capture_bins width of the bins used to display the signals
  Return value;
    float returns the adjusted value in dB
*****/
float PlotCalSpectrumEx(int x1, int cal_bins[2], int capture_bins) {
  //Serial.println("in PlotCalSpectrum");
  float adjdB = 0.0;
  int16_t adjAmplitude = 0;  // Was float; cast to float in dB calculation.  
  int16_t refAmplitude = 0;  // Was float; cast to float in dB calculation.  
  uint32_t index_of_max;     // This variable is not currently used, but it is required by the ARM max function.  

  if (x1 == (cal_bins[0] - capture_bins)) {  // Set flag at revised beginning.  
    updateDisplayFlag = 1;                   //Set flag so the display data are saved only once during each display refresh cycle at the start of the cycle, not 512 times 
  } else updateDisplayFlag = 0;              //  Do not save the the display data for the remainder of the

  ProcessIQData2();  // Call the Audio process from within the display routine to eliminate conflicts with drawing the spectrum and waterfall displays

  y_new = pixelnew[x1];
  y1_new = pixelnew[x1 - 1];
  y_old = pixelold[x1];
  y_old2 = pixelold[x1 - 1];

  // Find the maximums of the desired and undesired signals.
  if (bands[currentBand].mode == DEMOD_LSB) {
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
  }
  if (bands[currentBand].mode == DEMOD_USB) {
    arm_max_q15(&pixelnew[(cal_bins[0] - capture_bins)], capture_bins * 2, &adjAmplitude, &index_of_max);
    arm_max_q15(&pixelnew[(cal_bins[1] - capture_bins)], capture_bins * 2, &refAmplitude, &index_of_max);
  }

  if (y_new > base_y) y_new = base_y;
  if (y_old > base_y) y_old = base_y;
  if (y_old2 > base_y) y_old2 = base_y;
  if (y1_new > base_y) y1_new = base_y;

  if (y_new < 0) y_new = 0;
  if (y_old < 0) y_old = 0;
  if (y_old2 < 0) y_old2 = 0;
  if (y1_new < 0) y1_new = 0;
  int x1a = 256 - 2 * (256 - x1);  //Widen the display
  // Erase the old spectrum and draw the new spectrum.
  tft.drawLine(x1a, spectrumNoiseFloor - y_old2, x1a, spectrumNoiseFloor - y_old, RA8875_BLACK);  // Erase old...
  tft.drawLine(x1a, spectrumNoiseFloor - y1_new, x1a, spectrumNoiseFloor - y_new, RA8875_CYAN);   // Draw new
  pixelCurrent[x1a] = pixelnew[x1a];                                                              //  This is the actual "old" spectrum!  This is required due to CW interrupts.  Copied to pixelold by the FFT function.

  //Transmit Cal
  adjdB = ((float)adjAmplitude - (float)refAmplitude) / 1.95;  // Cast to float and calculate the dB level.  
  tft.writeTo(L2);
  if (bands[currentBand].mode == DEMOD_LSB) {
    tft.fillRect(cal_bins[0]-14, SPECTRUM_TOP_Y + 20, 20, h - 6, RA8875_BLUE);  // Adjusted height due to other graphics changes.   August 3, 2023
    tft.fillRect(cal_bins[01]-2, SPECTRUM_TOP_Y + 20, 20, h - 6, DARK_RED);
  } else {
    if (bands[currentBand].mode == DEMOD_USB) {  //mode == DEMOD_USB
      tft.fillRect(cal_bins[0] - 2, SPECTRUM_TOP_Y + 20, 20, h - 6, RA8875_BLUE);
      tft.fillRect(cal_bins[1] - 16, SPECTRUM_TOP_Y + 20, 20, h - 6, DARK_RED);
    }
  }

  tft.writeTo(L1);
  return adjdB;
}

#endif // V12HWR