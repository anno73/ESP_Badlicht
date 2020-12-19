/**********************************************************************

project: PCA9685 based RGB Panel with LED strips
author: 
description: RGB controller for IIC PCA9685 PWM
version: 

Persist in EEPROM
  cycle mode
  brightness
  speed
  seed of random number generator
  debug cycle function that 
    walks through each colour on each strip in order
  
ToDo:
  make delay non blocking
  add Serial1 for commands from external
  add pause after end of fade
**********************************************************************/


#define _GNU_SOURCE
#include <arduino.h>
#include <stdarg.h>
#include <Streaming.h>

#include "global.h"
#include "rgb_pwm.h"

//#include <TwiMap.h>
//#include <I2cMaster.h>
#include <brzo_i2c.h>

// #include <digitalWriteFast.h>  // currently not supported by Arduino Leonardo

// 15 strips in prod
// 5 in test
#define MAX_STRIPS  5

#define PWM_OE_PIN  9

//          SDA    SCL
// NodeMCU   4      5
// ESP-01    0      2
// Nano      2      3
#define IIC_DTA   4
#define IIC_CLK   5
#define IIC_STRETCH  1000   // time in us to allow stretch from a slave
#define IIC_FREQ 100        // SCL frequency in kHz

#define BUZZER    7       // Pull active Buzzer to low
#define BUZZER_ON 0       // Pull to ground to turn buzzer on
#define BUZZER_OFF  1

//#define SERIAL_BAUD   0L
//#define SERIAL_BAUD   19200L
//#define SERIAL_BAUD   115200L
//#define SERIAL1_BAUD  4800L
//#define SERIAL1_BAUD  9600L
//#define SERIAL1_BAUD  19200L
//#define SERIAL1_BAUD  9600L

// Address is 1 A5 A4 A3 A2 A1 A0 R/!W
// So if A0-A5 == 0, 1st address is 0x80
#define PCA_BASE_ADDRESS  (0x80 >> 1)
#define PCA_ALLCALL_ADDRESS (0xE0 >> 1)

// Every PCA has 16 separate PWM lines.
// Every strip attached requires 3 lines.
// That's a max of 5 strips per PCA plus leaves 1 line free.
#define PCA_LAST_ADDRESS (PCA_BASE_ADDRESS + (((MAX_STRIPS * 3) / 16) * 2))

// Streaming c++ like output
// http://playground.arduino.cc/Main/StreamingOutput
// Serial << "text 1" << someVariable << "Text 2" ... ;
//template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

#if 1   // EEPROM related stuff
/**********************************************************************
  We have about 1k of EEPROM to use to store data across restarts
  
  Layout of EEPROM
  
  Addr  Size  Variable
  0   1   Program to execute
  2   2   Speed
  4   1   Brightness
  6   2   Random Seed
  8   1   Beep on command executed
  10    .   ...
**********************************************************************/
//#include <EEPROMAnything.h>
#include <EEPROM.h>

#define EEPROM_ADDR_BASE        1024                     // Template
#define EEPROM_ADDR_CYCLEMODE   (EEPROM_ADDR_BASE + 0)   // cycleMode
#define EEPROM_ADDR_SPEED       (EEPROM_ADDR_BASE + 2)   // stepDelay
#define EEPROM_ADDR_BRIGHT      (EEPROM_ADDR_BASE + 4)   // pwm_oe
#define EEPROM_ADDR_SEED        (EEPROM_ADDR_BASE + 6)   // seedValue
#define EEPROM_ADDR_DOBEEP      (EEPROM_ADDR_BASE + 8)   // Beep on command
#define EEPROM_ADDR_SPEED_UNI   (EEPROM_ADDR_BASE + 10)
#define EEPROM_ADDR_SPEED_CLOUD (EEPROM_ADDR_BASE + 12)
#define EEPROM_ADDR_SPEED_WAVE  (EEPROM_ADDR_BASE + 14)
#define EEPROM_SIZE             32

#endif

#if 1   // Abort & Debug Macros
// Define macro for error handling
#define ABORT_UNLESS(A)         \
  if (! (A) )             \
  {                   \
    _ABORT_LINE = __LINE__;     \
    goto ABORT;           \
  }
uint16_t _ABORT_LINE;

// Define macro for debugging
// Initialize debug 
#define DBG_INIT(BUFF)              \
    {                   \
      const char bl = (BUFF);       \
      char buff[bl];            \
      uint8_t bp = snprintf(buff, bl,     

// Put the printf compatible debug statement here
// e.g.: "Initial s0=%g sn=%g, sd=%g", s0, sn, sd

// Close the debug block and do some sanity checking on buffer limits
// bl is length of buffer
// bp is printed chars to buffer
// It is possible prepend debug output with chars unused in buffer + '|' separator
// Tell user if there would have been more to print but was not due to buffer size
#define DBG_DONE                        \
    );                            \
      if (1) { Serial << (bl - bp) << '|'; }        \
      Serial.println(buff);               \
      if (bp > bl - 1)                  \
      {                         \
        Serial << F("snprintf: buffer exceeded: need ") \
          << bp                   \
          << F(" chars on line ")           \
          << __LINE__ - 1            \
          << endl                   \
        ;                       \
      }                         \
    }
#endif

typedef struct
{
  double h, s, v;
} hsv_st;

typedef struct
{
  uint16_t r, g, b;
} rgb_st;

// ### union or just an additional array pointer???
typedef union
{
  rgb_st rgb[MAX_STRIPS];
  uint16_t a[MAX_STRIPS * 3];
} pca_rgb_ut;

hsv_st    strip[MAX_STRIPS];    // Every strip's color
hsv_st    hsvDelta[MAX_STRIPS]; // Delta color for cycling
uint16_t  countMaxSteps = 0;    // Steps for a cycle periode
uint16_t  countSteps = 0;     // Step within cycle

pca_rgb_ut  pca_rgb;

// enum for all valid program cycle modes
enum cycleMode_t { 
  noChange = 0,   // First element. Stop running program, keep last HLS

  allOff = 1, 
  allOn = 2, 
  
  red = 3,      // all strips same color
  green = 4, 
  blue = 5,     // all strips same color
  
  uniform = 6,    // all strips same color, cycle through HLS
  cloud = 7, 
  wave = 8,
  
  debug = 9,
  
  illegal = 10    // End element. Maybe we cycle through programs in the future.
};

boolean newCommandAvail = false;
char newCommand = ' ';

// https://stackoverflow.com/questions/4437527/why-do-we-use-volatile-keyword-in-c
// why here?
volatile cycleMode_t cycleMode;
volatile cycleMode_t cycleModeOld;

void (*init_func)(void);
void (*step_func)(void);
void (*speed_func)(int8_t);
bool newCycleModeSelected;
uint32_t lastStepTS = 0;
uint16_t stepDelay = 10;    // ### uint16_t??? maybe uit8_t is sufficient

//TwiMaster iic(true);

uint8_t deb_strip = 0;
uint8_t deb_color = 0;

uint8_t pwm_oe = 0;       // 0 -> full On, 255 -> full Off

uint32_t loopCount = 0;

uint16_t seedValue = 0;

// Reset on ATMega328. On ESP possibly different.
void (*doSoftwareReset)(void) = 0;  //declare reset function at address 0

uint8_t doBeep = 1;       // Beep if true on various occasions

/**********************************************************************
  Function Prototypes
**********************************************************************/
bool pca_init(uint8_t addr);
int freeRam(void);
void hsv2pca(double h, double s, double v, uint16_t *_r, uint16_t *_g, uint16_t *_b);
void pca_rgb_update(uint16_t *arr, uint8_t chip, uint8_t count);
void initPanel(void);
void updatePanel(void);
void cloud_ReInit(void);
void cloud_Step(void);
void cloud_Init(void);
void uniform_Step(void);
void uniform_Init(void);
void wave_Step(void);
void wave_Init(void);
void debug_Step(void);
void debug_Init(void);
void blue_Init(void);
void green_Init(void);
void red_Step(void);
void red_Init(void);
void uni_Init(uint16_t r, uint16_t g, uint16_t b);
void allOn_Step(void);
void allOn_Init(void);
void allOff_Step(void);
void allOff_Init(void);
void dummy_Step(void);
void dummy_Init(void);
void dump_structures(void);
uint8_t processSerialInput(char);
uint8_t setCycleMode(cycleMode_t cm);
void initEEPROM(uint8_t, uint16_t, uint16_t);
void dumpEEPROM(uint16_t, uint16_t);
void loop();
void setup();
void beep(uint16_t firstArg, ...);
void serialPerfTest(void);
void dumpHelp(void);
//*********************************************************************

#if 0   // Arrays for Init, Step, Name of cycleMode
PROGMEM const char *cycleName[] =
{
  "noChange",
  "allOff",
  "allOn",
  "red",
  "green",
  "blue",
  "uniform",
  "cloud",
  "wave"
};

PROGMEM void (*cycleInitFuncs[])(void) =
{
  &dummy_Init,
  &allOff_Init,
  &allOn_Init,
  &red_Init,
  &green_Init,
  &blue_Init,
  &uniform_Init,
  &cloud_Init,
  &wave_Init
};

PROGMEM void (*cycleStepFuncs[])(void) =
{
  &dummy_Step,
  &allOff_Step,
  &allOn_Step,
  &dummy_Step,
  &dummy_Step,
  &dummy_Step,
  &uniform_Step,
  &cloud_Step,
  &wave_Step
};
#endif

/**********************************************************************
  Arduino Setup
  
  Initialize all the hardware and variables and ...
**********************************************************************/
void setupRgb()
{            
  // Init Serial
//  Serial.begin(1000000);
//  Serial.begin(SERIAL_BAUD);
//  Serial1.begin(SERIAL1_BAUD);

//  while (! Serial);       // For Leonardo to settle USB

  delay(10);

//  pinMode(IIC_DTA, OUTPUT); while ( 1 ) { digitalWrite(IIC_DTA, ! digitalRead(IIC_DTA)); delay(1); };
//  pinMode(IIC_CLK, OUTPUT); while ( 1 ) { digitalWrite(IIC_CLK, ! digitalRead(IIC_CLK)); delay(2); };

//  serialPerfTest();

  brzo_i2c_setup(IIC_DTA, IIC_CLK, IIC_STRETCH);
  
  Serial << F("RGB Panel Controller") << endl << endl;
//  Serial1 << F("RGB\n");

  Serial << F("Setup...") << endl;
  
//  pinMode(13, OUTPUT);
  
  // Init PCA
  Serial << F("Init Panel...") << endl;
  initPanel();
  
  // Init RGB Structures
  Serial << F("RGB Structures...") << endl;
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    pca_rgb.rgb[i].r = pca_rgb.rgb[i].g = pca_rgb.rgb[i].b = 0;
    strip[i].h = strip[i].s = strip[i].v = 0;
    hsvDelta[i].h = hsvDelta[i].s = hsvDelta[i].v = 0;
  }

#if 1
  init_func = &allOn_Init;
  step_func = &allOn_Step;
  
  cycleMode = cycleModeOld = allOn;
  newCycleModeSelected = true;
  seedValue = 0;
  stepDelay = 10;
  pwm_oe = 0;
#endif

#if 0   // Init parameters from EEPROM
  //
  // Initialize some parameters with previously persisted values from EEPROM
  //

//  initEEPROM();
  EEPROM.begin(EEPROM_SIZE);
  
  dumpEEPROM(EEPROM_ADDR_BASE, EEPROM_SIZE);
  
  Serial << F("Init default values from EEPROM...") << endl;
  { // Open block for local variable

    // Fetch cycleMode from EEPROM
    cycleMode_t cm = (cycleMode_t) EEPROM.read(EEPROM_ADDR_CYCLEMODE);
    Serial << F("cycleMode: ") << cm << ' ';
    // Initialize system
    if (! setCycleMode(cm))
    { // cycleMode read from EEPROM was not a valid one - e.g. uninitialized parameters
      // Initialize to allOn instead to see the system is working.
      setCycleMode(allOn);
    }
  }
  
  // Initialize stepDelay with values from EEPROM
  ((uint8_t *)(&stepDelay))[0] = EEPROM.read(EEPROM_ADDR_SPEED);
  ((uint8_t *)(&stepDelay))[1] = EEPROM.read(EEPROM_ADDR_SPEED + 1);
  Serial << F("stepDelay: ") << stepDelay << endl;

  // Initialize random number generator seed with values from EEPROM
  ((uint8_t *)(&seedValue))[0] = EEPROM.read(EEPROM_ADDR_SEED);
  ((uint8_t *)(&seedValue))[1] = EEPROM.read(EEPROM_ADDR_SEED + 1);
  Serial << F("seedValue: ") << seedValue << endl;
  randomSeed(seedValue);
  // next time use different seed value for different patterns
  seedValue++;
  EEPROM.write(EEPROM_ADDR_SEED, ((uint8_t *)(&seedValue))[0]);
  EEPROM.write(EEPROM_ADDR_SEED + 1, ((uint8_t *)(&seedValue))[1]);
  EEPROM.commit();

  // Initialize pwm_oe with values from EEPROM
  pwm_oe = EEPROM.read(EEPROM_ADDR_BRIGHT);
  Serial << F("pwm_oe: ") << pwm_oe << ".\n";
  
  doBeep = EEPROM.read(EEPROM_ADDR_DOBEEP);
  Serial << F("doBeep: ") << doBeep << ".\n";
  
#endif

  Serial << F("PWM /OE...\n");
//  analogWrite(PWM_OE_PIN, pwm_oe);

  // Buzzer
  Serial << F("Buzzer...\n");
//  pinMode(BUZZER, OUTPUT);
//  digitalWrite(BUZZER, BUZZER_OFF);
  beep(50, 50, 50, 0);
  
  Serial << F("Setup done...\n\n");
  Serial << F("Start loop...\n");
} // setup

/**********************************************************************
  Arduino Loop
  
  Main loop
**********************************************************************/
void loopRgb()
{
//  uint32_t loopStartUS = micros();
  
//  digitalWriteFast(13, ! digitalReadFast(13));
  
//  Serial << loopStartUS << '\n';
//  Serial << F("FreeRAM: ") << freeRam() << '\n';
//  Serial << F("CycleMode: ") << cycleMode << '\n';

//  Serial1 << '.';

  if (! newCommandAvail && Serial.available())
  {
    newCommand = Serial.read();
    if (newCommand != '\n' && newCommand != '\r')
    {
      Serial << F("Serial.Read: ") << (char) newCommand << '\n';

      newCommandAvail = true;
    }
  } // if (Serial.available())

#if 0
  if (! newCommandAvail && Serial1.available())
  {
    newCommand = Serial1.read();
    Serial1 << F("Serial1.Read: '") << newCommand << "'\n";

    newCommandAvail = true;
  } // if (Serial1.available())
#endif

  if (newCommandAvail)
  {
    processSerialInput(newCommand);
    newCommandAvail = false;
  }

//  if (loopCount > 1200) return;

  if (newCycleModeSelected)
  {
    // Initialize new cycle mode first
    (*init_func)();
    newCycleModeSelected = false;
  } else {
    // Execute step function of currently selected mode
    (*step_func)();
  }

  if (stepDelay)
  {
    // Handle millis wrap around 
    // ### really necessary? I don't think so.
    // Happens every 49d 17h 02m 47,295 - so what ...
//    if (millis() < lastStepTS)
//      lastStepTS = 0;
  
    uint32_t next_ts = lastStepTS + stepDelay;
    
    // Did the calculation last longer than we intended to wait?
    if (next_ts > millis())
    { // No, so wait till delay is over
      while (next_ts > millis());
      lastStepTS += stepDelay;
    }
    else
      // Yes, reset for next iteration
      // If not, the wait to time and the real time would drift apart
      lastStepTS = millis();
  }
  // Execute update
  updatePanel();

//  uint32_t loopStopUS = micros();
//  Serial << F("Loop duration[") << loopCount << F("]: ") << loopStopUS - loopStartUS << '\n';
//  loopCount ++;
} // loop

/**********************************************************************
  putNewCommand

  put a new command in the command queue.

**********************************************************************/
void putNewCommand(char c)
{
  if (newCommandAvail)
  {
    Serial << F("Not accepting new command. Still a command pending.") << endl;
    return;
  }
  newCommand = c;
  newCommandAvail = true;
  
  return;
} // putNewCommand

/**********************************************************************
  processSerialInput
  
  Processes commands from serial input
  Returns 
    1 on success
    0 on invalid code given
  
  Initialize function pointers for each mode
    init_func
    step_func
  Mark as newCycleModeSelected
  
  0 .. 9  Program modes
    0 All Off
    1 All On
    2 All red
    3 All green
    4 All blue
    5 Uniform
    6 Cloud
    7 Wave
    8 -
    9 -
  !   Set program to on hold
  b, B  Brightness
    b Decrease brightness
    B Increase brightness
  s, S  Speed
    s Decrease speed
    S Increase speed
  d   Dump
**********************************************************************/
uint8_t processSerialInput(char c)
{
  Serial << F("processInput: '") << c << "' " << endl;
  Serial.print((uint8_t) c, HEX);
  Serial.println('.');
  
  uint8_t res = 1;  // assume we have got a valid code
  
  Serial << F("Selected mode:") << endl;
  switch (c)
  {
  // Cycle Mode changes
  case '0': 
    setCycleMode(allOff);
    break;
  case '1': 
    setCycleMode(allOn);
    break;
  case '2':
    setCycleMode(red);
    break;
  case '3':
    setCycleMode(green);
    break;
  case '4':
    setCycleMode(blue);
    break;
  case '5': 
    setCycleMode(uniform);
    break;
  case '6':
    setCycleMode(cloud);
    break;      
  case '7':
    setCycleMode(wave);
    break;      
/*  case '8':
    setCycleMode(allOff);
    break;
  case '9':
    setCycleMode(allOff);
    break;
*/
  case 'd': // dump/debug ...
    setCycleMode(debug);
    break; 

  case '!':       // onHold ### new variable
    if (cycleMode == noChange)
    {
      cycleMode = cycleModeOld;
    } else {
      cycleModeOld = cycleMode;
      cycleMode = noChange;
    }
    newCycleModeSelected = true;
    break;
  // Execution control
  case 'b': // dim brightness
    if (pwm_oe >= 10)
      pwm_oe -= 10;
    else
      pwm_oe = 0;
//    analogWrite(PWM_OE_PIN, pwm_oe);
    Serial.print(F("Set PWM_OE to ")); Serial.println(pwm_oe);
    // Persist to EEPROM for next restart
    EEPROM.write(EEPROM_ADDR_BRIGHT, pwm_oe);
    EEPROM.commit();
    break; 
  case 'B': 
    if (pwm_oe <= 245)
      pwm_oe += 10;
    else
      pwm_oe = 255;
//    analogWrite(PWM_OE_PIN, pwm_oe);
    Serial.print(F("Set PWM_OE to ")); Serial.println(pwm_oe);
    // Persist to EEPROM for next restart
    EEPROM.write(EEPROM_ADDR_BRIGHT, pwm_oe);
    EEPROM.commit();
    break;
  case 's': // speed
    if (stepDelay >= 1)
      stepDelay--;
    else
      stepDelay = 0;
    Serial.print(F("Set stepDelay to ")); Serial.println(stepDelay);
    // Persist to EEPROM for next restart
    EEPROM.write(EEPROM_ADDR_SPEED,     ((uint8_t *)(&stepDelay))[0]);
    EEPROM.write(EEPROM_ADDR_SPEED + 1, ((uint8_t *)(&stepDelay))[1]);
    EEPROM.commit();
    break;
  case 'S':
    if (stepDelay <= 254)
      stepDelay++;
    else
      stepDelay = 255;
    Serial.print(F("Set stepDelay to ")); Serial.println(stepDelay);
    // Persist to EEPROM for next restart
    EEPROM.write(EEPROM_ADDR_SPEED,     ((uint8_t *)(&stepDelay))[0]);
    EEPROM.write(EEPROM_ADDR_SPEED + 1, ((uint8_t *)(&stepDelay))[1]);
    EEPROM.commit();
    break;
  case 'g' :
    doBeep = 1;
    Serial.print(F("Set doBeep to ")); Serial.println(doBeep);
    EEPROM.write(EEPROM_ADDR_DOBEEP, doBeep);
    EEPROM.commit();
    break;
  case 'G' :
    doBeep = 0;
    Serial.print(F("Set doBeep to ")); Serial.println(doBeep);
    EEPROM.write(EEPROM_ADDR_DOBEEP, doBeep);
    EEPROM.commit();
    break;
  case '@':
    Serial.print(F("INITIATE SOFTWARE RESET.\n"));
    
    doSoftwareReset();
    break;
  case 'e':
    dumpEEPROM(0, 16);      // from, count
    break;
  case 'E':
    initEEPROM(0, 0, 16);   // value, from, count
    break;
  case '?':
    dumpHelp();
    break;
  default:
    Serial << F("Command not implemented: '") << c << "'. Use '?' for help.\n";
    res = 0;
    break;
  }
  
  Serial << F("processSerialInput: done.") << endl;
  
  if (res)
    beep(50, 50, 50, 0, 0);
  else
    beep(100, 50, 100, 0, 0);
  
  return res;
} // processSerialInput

void dumpHelp()
{
  Serial << F(
    "0 All OFF\n"
    "1 All ON\n"
    "2 Red\n"
    "3 Green\n"
    "4 Blue\n"
    "5 Uniform\n"
    "6 Cloud\n"
    "7 Wave\n"
    "b Increase Brightness\n"
    "B Decrease Brightness\n"
    "s Decrease Step Delay\n"
    "S Increase Step Delay\n"
    "g Enable Beep\n"
    "G Disable Beep\n"
    "@ Software Reset\n"
    "e Dump EEPROM\n"
    "E Reinit EEPROM\n"
    "? This help\n"
  );
} // dumpHelp

/*---------------------------------------------------------------------
  setCycleMode
  
  Sets one of the allowed cycle modes
  Ignores setting if cycle already in progress

  Returns
    0 invalid cycle mode
    1 cycle mode set
---------------------------------------------------------------------*/
uint8_t setCycleMode(cycleMode_t cm)
{
  switch (cm)
  {
  case allOff: 
    init_func = &allOff_Init;
    step_func = &allOff_Step;
    Serial.println(F("allOff"));
    break;
  case allOn: 
    init_func = &allOn_Init;
    step_func = &allOn_Step;
    Serial.println(F("allOn"));
    break;
  case red:
    init_func = &red_Init;
    step_func = &dummy_Step;
    Serial.println(F("red"));
    break;
  case green:
    init_func = &green_Init;
    step_func = &dummy_Step;
    Serial.println(F("green"));
    break;
  case blue:
    init_func = &blue_Init;
    step_func = &dummy_Step;
    Serial.println(F("blue"));
    break;
  case uniform: 
    init_func = &uniform_Init;
    step_func = &uniform_Step;
    Serial.println(F("uniform"));
    break;
  case cloud:
    init_func = &cloud_Init;
    step_func = &cloud_Step;
    Serial.println(F("cloud"));
    break;      
  case wave:
    init_func = &wave_Init;
    step_func = &wave_Step;
    Serial.println(F("wave"));
    break;      
//  case '8': break;
//  case '9': break;
  case debug:
    init_func = &debug_Init;
    step_func = &debug_Step;
    Serial.println(F("debug"));
    break;
  default:
    Serial << F("Cycle mode not implemented: ") << cycleMode << ".\n";
    newCycleModeSelected = false;
    return 0;
    break;
  }

  // We have a valid new cycle mode
  newCycleModeSelected = true;  // Initialize in loop on next pass
  cycleMode = cm;         // Store new cycle mode
  
  // Persist new cycleMode to EEPROM
  EEPROM.write(EEPROM_ADDR_CYCLEMODE, cycleMode);
  EEPROM.commit();

  return 1;
} // setCycleMode

void dump_structures(void)
{
  char buff[64];
  
  Serial.println(F("----------"));
  snprintf(buff, sizeof(buff), "Loop Count: %u\n", loopCount);
  Serial.print(buff);
  
  snprintf(buff, sizeof(buff), "MaxSteps: %u CountSteps: %u\n", countMaxSteps, countSteps);
  Serial.print(buff); 
  
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    snprintf(buff, sizeof(buff), "strip[%u]: %g %g %g\n", i, strip[i].h, strip[i].s, strip[i].v);
    Serial.print(buff);   
  }

  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    snprintf(buff, sizeof(buff), "hsvDelta[%u]: %g %g %g\n", i, hsvDelta[i].h, hsvDelta[i].s, hsvDelta[i].v);
    Serial.print(buff);   
  }

  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    snprintf(buff, sizeof(buff), "pca_rgb.rgb[%u]: %u %u %u\n", i, pca_rgb.rgb[i].r, pca_rgb.rgb[i].g, pca_rgb.rgb[i].b);
    Serial.print(buff);   
  }

  Serial.println(F("----------"));
} // dump_structures

#if 1   // dummy, allOff, allOn color cycles
/*---------------------------------------------------------------------
  dummy
  
  Some cycle m odes do not need a init or step function that does
  something useful.
---------------------------------------------------------------------*/
void dummy_Init(void) {}
void dummy_Step(void) {}

/*---------------------------------------------------------------------
  allOff
  
  Turn off all led strips
---------------------------------------------------------------------*/
void allOff_Init(void)
{
  // set RGB values to 0 here or better use the PCA native ALL_OFF flags?
  // Will result in black color :-)
  
  uni_Init(0, 0, 0);
}

void allOff_Step(void) {}

/*---------------------------------------------------------------------
  allOn
  
  Turn on all led strips
---------------------------------------------------------------------*/
void allOn_Init(void) 
{
  // set RGB values to 4095 here or better use the PCA native ALL_ON flags?
  // Will result in white color :-)
  
  uni_Init(4095, 4095, 4095);
}

void allOn_Step(void) {}
#endif

#if 1   // uni colour cycle
/*---------------------------------------------------------------------
  Uni colour cycle

  Set all colours on all strips to identical RGB colour
---------------------------------------------------------------------*/
void uni_Init(uint16_t r, uint16_t g, uint16_t b)
{
  // Set RGB value directly
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    pca_rgb.rgb[i].r = r;
    pca_rgb.rgb[i].g = g;
    pca_rgb.rgb[i].b = b;
  }
} // uni_Init

void red_Init(void)
{
  uni_Init(4095, 0, 0);
}

void green_Init(void)
{
  uni_Init(0, 4095, 0);
}

void blue_Init(void)
{
  uni_Init(0, 0, 4095);
}
#endif

#if 1   // debug colour cycle
/*---------------------------------------------------------------------
  Debug colour cycle

  Cycle through all colours on all strips synchronously
---------------------------------------------------------------------*/
void debug_Init(void)
{
  uni_Init(0, 0, 0);
  
//  uint8_t strip = 0;
//  uint8_t color = 0;
} // debug_Init

void debug_Step(void)
{

  // Clear current strips color
  switch (deb_color)
  {
  case 0:
    pca_rgb.rgb[deb_strip].r = 0;
    break;
  case 1:
    pca_rgb.rgb[deb_strip].g = 0;
    break;
  case 2:
    pca_rgb.rgb[deb_strip].b = 0;
    break;
  default:
    break;
  }

  // Calculate new strip and color
  deb_color++;
  if (deb_color > 2)
  {
    deb_color = 0;
    deb_strip++;
  }

  if (deb_strip > MAX_STRIPS)
    deb_strip = 0;

  // Set new color on strip
  switch (deb_color)
  {
  case 0:
    pca_rgb.rgb[deb_strip].r = 4095;
    break;
  case 1:
    pca_rgb.rgb[deb_strip].g = 4095;
    break;
  case 2:
    pca_rgb.rgb[deb_strip].b = 4095;
    break;
  default:
    break;
  }
  
} // debug_Step
#endif

#if 1   // wave colour cycle
/*---------------------------------------------------------------------
  Wave colour cycle
  
---------------------------------------------------------------------*/
void wave_Init(void)
{
  // Initialize strips
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    strip[i].h = ((double) i) * 0.01;
    strip[i].s = 1; 
    strip[i].v = 1;
    
    hsv2pca(strip[i].h, strip[i].s, strip[i].v, 
      &pca_rgb.rgb[i].r, &pca_rgb.rgb[i].g, &pca_rgb.rgb[i].b);

  }
} // wave_Init

void wave_Step(void)
{

  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    strip[i].h += 0.001;
    while (strip[i].h > 1.0)
      strip[i].h -= 1.0;

    strip[i].s = 1; 
    strip[i].v = 1;

    hsv2pca(strip[i].h, strip[i].s, strip[i].v, 
      &pca_rgb.rgb[i].r, &pca_rgb.rgb[i].g, &pca_rgb.rgb[i].b);
  }
} // wave_Step
#endif

#if 1   // uniform colour cycle
/*---------------------------------------------------------------------
  uniform colour cycle

  Cycle through all colours on all strips synchronously
---------------------------------------------------------------------*/
void uniform_Init(void)
{
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    strip[i].h = 0;
    strip[i].s = 1;
    strip[i].v = 1;
  }
} // uniform_Init

void uniform_Step(void)
{
//  double d;
  // Calculate next step

  // First strip is reference
  // Advance in color
  strip[0].h += 0.001;
  
  while (strip[0].h > 1.0)
    strip[0].h -= 1.0;
//    strip[0].h = modf(strip[0].h, &d);

  while (strip[0].h < 0)
    strip[0].h += 1;

  // Calculate RGB
  hsv2pca(strip[0].h, strip[0].s, strip[0].v, 
    &pca_rgb.rgb[0].r, &pca_rgb.rgb[0].g, &pca_rgb.rgb[0].b);

  // Populate RGB
  uni_Init(pca_rgb.rgb[0].r, pca_rgb.rgb[0].g, pca_rgb.rgb[0].b);
} // uniform_Step
#endif

#if 1   // cloud colour cycle
/*---------------------------------------------------------------------
  Cloud colour cycle

  Pick two random colours for first (s0) and last (sn) strip
  Interpolate linearly each strip in between
  Pick two more random colours for first and last strip
  Fade to the new colours across all strips in random steps
---------------------------------------------------------------------*/
void cloud_Init(void)
{
  // Initialize first and last strip
  // Interpolate each strip in between
  // Prepare for first ReInit

  Serial.println(F("cloud_init..."));

  double s0, sn;

  // Get some random numbers [0..1)
  s0 = random(100) / 100.0;
  sn = random(100) / 100.0;

  s0 = 0.0;
  sn = 1.0 / 6.0;

  s0 = 0.0;
  sn = 0.0;
  
  // Calculate interval size between strips
  double sd = (sn - s0) * (1.0 / (double) (MAX_STRIPS - 1));

#if 1
  DBG_INIT(40)
//  "Initial s0=%g sn=%g, sd=%g", s0, sn, sd
  "Initial s0=%f sn=%f, sd=%f", s0, sn, sd
  DBG_DONE
#endif

  // Initialize strips
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    strip[i].h = s0 + sd * (double) i;
//    sx = s0 + sd * (double) i;
//    strip[i].h = sx;
    strip[i].s = 1; 
    strip[i].v = 1;

#if 1
    DBG_INIT(24)
    "strip[%d] = %g", i, strip[i].h
    DBG_DONE
#endif  

    hsv2pca(strip[i].h, strip[i].s, strip[i].v, 
      &pca_rgb.rgb[i].r, &pca_rgb.rgb[i].g, &pca_rgb.rgb[i].b);
  }
  
  countSteps = 0;   // Next step is to recalculate new delta and step values
  Serial << F("cloud_init...done") << endl;
} // cloud_init

void cloud_Step(void)
{

#if 0
    DBG_INIT(24)
    "cloud_step[%d|%d]...", countSteps, countMaxSteps
    DBG_DONE
#endif  
  
  if (countSteps <= 0)
  {
//    digitalWriteFast(13, HIGH);
    digitalWrite(13, HIGH);
    cloud_ReInit();
    delay(1000);
//    digitalWriteFast(13, LOW);
    digitalWrite(13, HIGH);
    return;
  }

  // Update HSV & RGB values for current step
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    strip[i].h += hsvDelta[i].h;
    
    // Handle over- and underrun of hue
    if (strip[i].h > 1)
      strip[i].h -= 1;
    if (strip[i].h < 0)
      strip[i].h += 1;

//    Serial.print(strip[i].h, 6); Serial.print(F(" "));

    // Calculate RGB
    hsv2pca(strip[i].h, strip[i].s, strip[i].v, 
      &pca_rgb.rgb[i].r, &pca_rgb.rgb[i].g, &pca_rgb.rgb[i].b);
  }

//  Serial.println();

  countSteps--;
  return;
} // cloud_step

void cloud_ReInit(void)
{
  Serial << F("cloud_ReInit...") << millis() << endl;
  
  double s0, sn;
  
  // Calculate new targets
  // ### need to take care that targets do not differ too much?
//  s0 = random(100) / 100.0;
//  sn = random(100) / 100.0;

  // Make distance large enough. One color has ~ 100/6 = 16.6
  // Make distance not too large so it spreads nice across MAX_STRIPS
  s0 = random(100) / 100.0;
  sn = s0 + (random(25, 75) / 100.0) * ( (random(0, 2) == 0) ? 1.0 : -1.0);

//  s0 = strip[MAX_STRIPS - 1].h; // Start where we left off
//  sn = s0 + 1.0 / 6.0;      // Advance one color

//  s0 = strip[MAX_STRIPS - 1].h; // Start where we left off
//  s0 += 1.0 / 6.0;
//  sn = s0 + 1.0 / 6.0;      // Advance one color


  // Rebound sx to [0, 1)
  if (s0 >= 1.0)
    s0 -= 1.0;
  if (s0 < 0.0)
    s0 += 1.0;

  if (sn >= 1.0)
    sn -= 1.0;
  if (sn < 0.0)
    sn += 1.0;
  
  // Calculate interval between first and last strip
  double sd = (sn - s0); 
  // Take the shorter way in color circle
  if (sd > 0.5)
    sd = - (1.0 - sd);
  if (sd < -0.5)
    sd = 1.0 + sd;
  // Delta from one strip to the next
  sd *= (1.0 / (double) (MAX_STRIPS - 1));

#if 1
//  DBG_INIT(45)
//  "New s0=%f sn=%f, sd=%f", s0, sn, sd
//  DBG_DONE

  Serial << F("New s0="); Serial.print(s0, 8);
  Serial << F(" sn="); Serial.print(sn, 8);
  Serial << F(" sd="); Serial.print(sd, 8);
  Serial << '\n';
#endif  

  // Calculate steps to reach till new target
  // ### need to take care that enough steps for large target diffs?
  countSteps = countMaxSteps = 100; // random(256);

#if 1
  DBG_INIT(20)
  "countMaxSteps=%d", countMaxSteps
  DBG_DONE
#endif  
  
  // Next cycle goes from strip[i].h to s0 + sd * i in countMaxSteps steps
  for (uint8_t i = 0; i < MAX_STRIPS; i++)
  {
    double from = strip[i].h;
    double to = s0 + sd * (double) i;
    double diff = (to - from) / (double) (countMaxSteps);

    hsvDelta[i].h = diff;
    hsvDelta[i].s = 0;
    hsvDelta[i].v = 0;

#if 1
//    DBG_INIT(55)
//    "strip[%d]=%f to=%f diff=%f", i, strip[i].h, to, diff
//    DBG_DONE

    Serial << F("strip[") << i << F("]="); Serial.print(strip[i].h, 8);
    Serial << F(" to="); Serial.print(to, 8);
    Serial << F(" diff="); Serial.print(diff, 8);
    Serial << '\n';
#endif  

  }
  Serial.println(F("cloud_ReInit...done"));
} // cloud_ReInit
#endif

#if 1   // Panel related routines
/**********************************************************************
  Panel related functions
**********************************************************************/

/*---------------------------------------------------------------------
  updatePanel
  
  Write colour arrays to the panel for display
---------------------------------------------------------------------*/
void updatePanel(void)
{

//  dump_structures();
//  return;

  uint8_t channels = MAX_STRIPS * 3;
  uint8_t chip = PCA_BASE_ADDRESS;
  
  // Each PCA has 16 channels. We only use 15 at most (R, G, B)
  // Every Strip requires 3 channels (RGB), so we have at most 5 strips per PCA

  for (uint8_t i = 0; i < MAX_STRIPS * 3; i += 15, chip += 2)
  {
    pca_rgb_update(&pca_rgb.a[i], chip, min(channels, (uint8_t) 15));
    
    if (channels > 15) channels -= 15;
  }
  
  // IIC Stop here to update all PCAs on STOP?
  // Night Require restart in update function!
}

/*---------------------------------------------------------------------
  initPanel
  
  Initialize all PCA9685 ICs as required for operation
---------------------------------------------------------------------*/
void initPanel(void)
{
  uint8_t buff[1];
  uint8_t b = 0;
  
  // Reset IIC bus
  Serial << F("initPanel: Reset IIC.") << endl;
//  iic.start(0x00 | I2C_WRITE);
//  iic.write(0x06);
//  iic.stop();

  buff[b++] = 0x06;

  brzo_i2c_start_transaction(0x00, IIC_FREQ);
  brzo_i2c_write(buff, b, false);
  uint8_t res = brzo_i2c_end_transaction();
  Serial << F("initPanel: IIC Status: ") << res << endl;

  for (uint8_t addr = PCA_BASE_ADDRESS; addr <= PCA_LAST_ADDRESS; addr += 2)
  {
    // Init PCAs to default values
//    Serial << F("initPanel: init PCA9685 at ") << _HEX(addr) << ':' << addr << '\n';
    pca_init(addr);
  }
} // initPanel
#endif

#if 1   // PCA low level routines
/**********************************************************************
  PCA9685
  
  Low Level Routines
**********************************************************************/
/*---------------------------------------------------------------------
  pca_init
  
  Initialize a PCA9685 IC as required for operation
---------------------------------------------------------------------*/
bool pca_init(uint8_t addr)
{
  uint8_t buff[3];
  uint8_t b = 0;

  Serial << F("pca_init[") << _HEX(addr) << F("]...");
//  return true;

//  ABORT_UNLESS(iic.start(addr | I2C_WRITE));
//  brzo_i2c_start_transaction(addr, IIC_FREQ);

  // Select Mode 1 register
//  ABORT_UNLESS(iic.write(0));
  buff[b++] = 0;
//  brzo_i2c_write(buff, 1, false);

  /*
    Mode 1 register
    7 restart 0
    6 extclk  0 // 0: Use internal clock
    5 ai    1 // 1: Auto Increment on
    4 sleep   0
    3 sub1    0
    2 sub2    0
    1 sub3    0
    0 allcall 1 // 1: Enable All_CALL address
    
    0 0 1 0 . 0 0 0 1
  */
//  ABORT_UNLESS(iic.write(0x21));
  buff[b++] = 0x21;
//  brzo_i2c_write(buff, 1, false);

  /*
    Mode 2 register
    7 reserved  0
    6 reserved  0
    5 reserved  0
    4 invrt   0 // 0: not inverted (driver), 1: inverted (no driver)
    3 och   0 // output change: 0: stop cmd, 1: ack
    2 outdrv  1 // 0: open-drain, 1: totem pole
    1 outne1  0 // /OE=1 & 00: LEDn=0   10: high impedance
    0 outne0  0 //  01 & OUTDRV=1: LDEn=1, OUTDRV=0: high impedance

    0 0 0 0 . 0 1 0 0
  */
//  ABORT_UNLESS(iic.write(0x04));
  buff[b++] = 0x04;

  Serial << F("pca_init(): count buff elements: ") << b << "/" << sizeof(buff) << endl;

  brzo_i2c_start_transaction(addr, IIC_FREQ);
  brzo_i2c_write(buff, b, false);

//  iic.stop();
//  ABORT_UNLESS(res = brzo_i2c_end_transaction());
  uint8_t res = brzo_i2c_end_transaction();
  Serial << res << F("... done.") << endl;
  return (res == 0);

// ABORT:
  // Do some magic here
//  iic.stop();
//  Serial << F("pca_init: IIC ERROR on line: ") << _ABORT_LINE << endl;
//  Serial << F("pca_init: IIC ERROR on line: ") << _ABORT_LINE << endl;
//  return false;
} // pca_init

/*---------------------------------------------------------------------
  pca_rgb_update
  
  Write RGB values to count PWM registers of the PCA
  Starting with first
---------------------------------------------------------------------*/
void pca_rgb_update(uint16_t *arr, uint8_t chip, uint8_t count)
{
#if 0
  DBG_INIT(32)
  "pca_rgb_update(arr, %X, %d):", chip, count
  DBG_DONE
#endif  

  uint8_t buff[62];
  uint8_t b = 0;
  
//  ABORT_UNLESS(iic.start(chip | I2C_WRITE));  // Start communication, select chip
//  brzo_i2c_start_transaction(chip, IIC_FREQ);

//  ABORT_UNLESS(iic.write(0x06));        // select 1st PWM register
/*  { 
    uint8_t reg = 0x06;
    brzo_i2c_write(&reg, 1, false);
  }
*/
  buff[b++] = 0x06;

  // Update all PWM registers
  for (uint8_t i = 0; i < count; i++)
  {
//    uint16_t on = i * 1;
    uint16_t on = 0;
    uint16_t off = arr[i] + on;
    
    uint8_t on_l = on & 0xFF;
    uint8_t on_h = (on >> 8) & 0x0F;

    uint8_t off_l = off & 0xFF;
    uint8_t off_h = (off >> 8) & 0x0F;

#if 0
    DBG_INIT(64)
    "arr[%d]: %d(%d:%d) %d(%d:%d)", 
        arr[i], 
        on, on_h, on_l, 
        off, off_h, off_l
    DBG_DONE
#endif
    // LED on LOW
//    ABORT_UNLESS(iic.write(on_l));
//    brzo_i2c_write(&on_l, 1, false);
    // LED on HIGH
//    ABORT_UNLESS(iic.write(on_h));
//    brzo_i2c_write(&on_h, 1, false);
    // LED off LOW
//    ABORT_UNLESS(iic.write(off_l));
//    brzo_i2c_write(&off_l, 1, false);
    // LED off HIGH (4 bits)
//    ABORT_UNLESS(iic.write(off_h));
//    brzo_i2c_write(&off_h, 1, false);
    buff[b++] = on_l;
    buff[b++] = on_h;
    buff[b++] = off_l;
    buff[b++] = off_h;
  }

//  Serial << F("pca_rgb_update(): count buff elements: ") << b << "/" << sizeof(buff) << endl;

  brzo_i2c_start_transaction(chip, IIC_FREQ);
  brzo_i2c_write(buff, b, false);

  uint8_t res = brzo_i2c_end_transaction();
  if ( res )
  {
    Serial << F("pca_rgb_update(): IIC error code: ") << res << endl;
  }
  return;

//ABORT:
//  Serial << F("pca_rgb_update: IIC ERROR on line: "); Serial.println(_ABORT_LINE);
//  iic.stop();
} // pca_rgb_update
#endif

#if 1   // HSV, RGB, PCA colour conversion
/**********************************************************************
  hsv2pca
  
  Taken from RGBConverter class
  Robert Atkins, December 2010 (ratkins_at_fastmail_dot_fm).
  https://github.com/ratkins/RGBConverter
  
  Adapted to convert RGB values to PCA value range of 12bit (0..4095)
**********************************************************************/
void hsv2pca(double h, double s, double v, uint16_t *_r, uint16_t *_g, uint16_t *_b)
{
    double r = 0, g = 0, b = 0;

    int    i = int(h * 6);
    double f = h * 6 - i;
    double p = v * (1 - s);
    double q = v * (1 - f * s);
    double t = v * (1 - (1 - f) * s);

    switch(i % 6){
        case 0: r = v, g = t, b = p; break;
        case 1: r = q, g = v, b = p; break;
        case 2: r = p, g = v, b = t; break;
        case 3: r = p, g = q, b = v; break;
        case 4: r = t, g = p, b = v; break;
        case 5: r = v, g = p, b = q; break;
        default: r = g = b = 0; break;
    }

  // ranges from 0..4095
  // typecast works as truncate?
    *_r = ((uint16_t)(r * 4095.0)) & 0x0FFF;
//    *_g = ((uint16_t)(g * 4095.0)) & 0x0FFF;
    *_g = ((uint16_t)(g * 1023.0)) & 0x0FFF;
//    *_b = ((uint16_t)(b * 4095.0)) & 0x0FFF;
    *_b = ((uint16_t)(b * 1023.0)) & 0x0FFF;
} // hsv2pca

#endif

#if 1   // Buzzer beep
/**********************************************************************
  beep
  
  list of on/off times in milli seconds
  last argument needs to be 0
  
  blocking
**********************************************************************/
void beep(uint16_t firstArg, ...)
//void beep(int firstArg, ...)
{
  return; 
  
  va_list argList;
  va_start(argList, firstArg);

  if (! doBeep)
    return;
  
//  uint16_t duration = firstArg;
  uint16_t duration = (uint16_t) firstArg;
  
  digitalWrite(BUZZER, BUZZER_ON);    // Make some noise
  do 
  {
//      Serial << F("Buzzer delay[") << digitalRead(BUZZER) << F("]: ") << duration << "ms.\n";
      delay(duration);        // Delay execution - make noise or silence
      digitalWrite(BUZZER, ! digitalRead(BUZZER));
  }
//  while (duration = va_arg(argList, uint16_t)); 
  while (duration = (uint16_t) va_arg(argList, int)); 
  digitalWrite(BUZZER, BUZZER_OFF);   // Be sure to turn off buzzer upon leave
  
  va_end(argList);
}
#endif

#if 1   // EEPROM related functions
/*---------------------------------------------------------------------
  initEEPROM
  
  Initialize EEPROM with some reasonable values
---------------------------------------------------------------------*/
void initEEPROM(uint8_t value, uint16_t from, uint16_t count)
{
  uint8_t i;

  Serial << F("initEEPROM with ") << value << F(" from ") << from << F(" to ") << from + count;
  
  for (i = from; i < from + count; i++)
    EEPROM.write(i, value);
    
  EEPROM.commit();

  Serial << F(" ... done.\n");
    
} // initEEPROM

/*---------------------------------------------------------------------
  dumpEEPROM
  
  Dump EEPROM values
---------------------------------------------------------------------*/
void dumpEEPROM(uint16_t from, uint16_t count)
{
  char buff[16];
  
  Serial << F("Dump EEPROM contents from ") << from << F(" to ") << from + count << '(' << count << F(")bytes\n");
  snprintf(buff, sizeof(buff), "%04d %03X: ", 0, 0);
  Serial << buff;

  uint16_t i = from;
  uint8_t j = 1;
  while (i < from + count)
  {
    snprintf(buff, sizeof(buff), "%02X ", EEPROM.read(i));
    Serial << buff;

    i++;  // advance here for the print below

    if (j < 16)
      Serial << ' ';
    else
    {
      if (i < from + count)   // skip the last empty preamble of the line as while would end anyway
      {
        j = 0;
        snprintf(buff, sizeof(buff), "\n%04d %03X: ", i, i);
        Serial << buff;
      }
    }
    j++;
  }
  Serial << '\n';
} // dumpEEPROM
#endif

#if 1   // Various stuff - 
/**********************************************************************
  freeRam
  
  Taken from 
  http://www.controllerprojects.com/2011/05/23/determining-sram-usage-on-arduino/
  ---
  As stated on the JeeLabs site:

    There are three areas in RAM:

    * static data, i.e. global variables and arrays … and strings !
    * the “heap”, which gets used if you call malloc() and free()
    * the “stack”, which is what gets consumed as one function calls 
      another

    The heap grows up, and is used in a fairly unpredictable manner. 
    If you release areas, then they will be lead to unused gaps in 
    the heap, which get re-used by new calls to malloc() if the 
    requested block fits in those gaps.

    At any point in time, there is a highest point in RAM occupied 
    by the heap. This value can be found in a system variable 
    called __brkval.

    The stack is located at the end of RAM, and expands and 
    contracts down towards the heap area. Stack space gets 
    allocated and released as needed by functions calling other 
    functions. That’s where local variables get stored.
  ---
  
  IMO int v; sits at the lowest address of the stack currently
  
**********************************************************************/
int freeRam(void) 
{
  extern int __heap_start, *__brkval; 
  int v;
  
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
} // freeRam

/**********************************************************************
  serialPerfTest
  
  Measure throughput of serial interface

  Arduino IDE Serial Console on USB
    100x10byte: 16852     57,95KiB/s
    1000x10byte: 167140
    10000x10byte: 1669280
    100x100byte: 164024
    1000x100byte: 1638468
    10000x100byte: 16383004   59,6KiB/s
    100x10byte: 16844
    1000x10byte: 167144
    10000x10byte: 1669268
    100x100byte: 164032
    1000x100byte: 1638476
    10000x100byte: 16383012
  Arduino IDE Serial Console stopped
    Output blocked
  Leonardo USB Disconnect
    Output blocked
  Leonardo USB Reconnect
    100x10byte: 4820
    1000x10byte: 48068
    10000x10byte: 477644
    100x100byte: 44856
    1000x100byte: 446860
    10000x100byte: 4466964
    100x10byte: 4816
    1000x10byte: 48064
    10000x10byte: 477640
    100x100byte: 44856
    1000x100byte: 446864
    10000x100byte: 4466964
  Leonardo USB Re-Disconnect
    100x10byte: 4784      204KiB/s
    1000x10byte: 47728
    10000x10byte: 474276
    100x100byte: 44540
    1000x100byte: 443720
    10000x100byte: 4435512    220KiB/s
    100x10byte: 4784
    1000x10byte: 47732
    10000x10byte: 474272
    100x100byte: 44544
    1000x100byte: 443720
    10000x100byte: 4435512
    
**********************************************************************/
#if 0
void serialPerfTest(void)
{
  uint32_t startTs, endTs;
  uint16_t i;
  
  Serial1 << F("USB Serial Write Test...\n");
  
  while (1)
  {
    Serial1 << F("100x10byte: ");
    startTs = micros();
    for (i = 100; i; i--)
      Serial << F("123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";
    
    Serial1 << F("1000x10byte: ");
    startTs = micros();
    for (i = 1000; i; i--)
      Serial << F("123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";

    Serial1 << F("10000x10byte: ");
    startTs = micros();
    for (i = 10000; i; i--)
      Serial << F("123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";


    Serial1 << F("100x100byte: ");
    startTs = micros();
    for (uint16_t i = 100; i; i--)
      Serial << F("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";

    Serial1 << F("1000x100byte: ");
    startTs = micros();
    for (uint16_t i = 1000; i; i--)
      Serial << F("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";
    
    Serial1 << F("10000x100byte: ");
    startTs = micros();
    for (uint16_t i = 10000; i; i--)
      Serial << F("123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789\n");
    endTs = micros();
    Serial1 << endTs - startTs << "\n\r";
  }
} // serialPerfTest
#endif

#endif
