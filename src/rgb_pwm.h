#ifndef MY_RGB_PWM_H
#define MY_RGB_PWM_H

//#include "mqtt.h"

extern void setupRgb();
extern void loopRgb();

extern void enableRGB(bool);
extern void setModeRGB(uint8_t);

extern void setAbsoluteBrightnessRGB(uint8_t);
extern void setRelativeBrightnessRGB(int16_t);

extern void setAbsoluteSpeedRGB(uint8_t);
extern void setRelativeSpeedRGB(int16_t);

extern void enableBeepRGB(void);
extern void disableBeepRGB(void);


#endif