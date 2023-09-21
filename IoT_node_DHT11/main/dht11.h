#ifndef _DHT11_
#define _DHT11_
#include "driver/gpio.h"

void DHT_init(int gpio);
bool DHT_sample(int gpio, double *p_humdity, double *p_temperature_c);

#endif