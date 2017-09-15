/**
 * Marlin 3D Printer Firmware
 * Copyright (C) 2016 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (C) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __ARDUINO_H__
#define __ARDUINO_H__
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#define LOW          0x00
#define HIGH         0x01
#define CHANGE       0x02
#define FALLING      0x03
#define RISING       0x04

#define INPUT        0x00
#define OUTPUT       0x01
#define INPUT_PULLUP 0x02

#define _BV(bit) (1 << (bit))

#define E2END 4096 //EEPROM end address

typedef uint8_t byte;
#define PROGMEM
#define PSTR(v) (v)
#define PGM_P const char *

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))
#ifndef isnan
#define isnan std::isnan
#endif
#ifndef isinf
#define isinf std::isinf
#endif

//not constexpr until c++14
//#define max(v1, v2) std::max((int)v1,(int)v2)
//#define min(v1, v2) std::min((int)v1,(int)v2)
//#define abs(v) std::abs(v)

#define sq(v) ((v) * (v))
#define square(v) sq(v)
#define constrain(value, arg_min, arg_max) ((value) < (arg_min) ? (arg_min) :((value) > (arg_max) ? (arg_max) : (value)))

//Interrupts
void cli(void); // Disable
void sei(void); // Enable
void attachInterrupt(uint32_t pin, void (*callback)(void), uint32_t mode);
void detachInterrupt(uint32_t pin);
extern "C" void GpioEnableInt(uint32_t port, uint32_t pin, uint32_t mode);
extern "C" void GpioDisableInt(uint32_t port, uint32_t pin);

// Program Memory
#define pgm_read_ptr(address_short) (*(address_short))
#define pgm_read_byte_near(address_short) (*address_short)
#define pgm_read_byte(address_short) pgm_read_byte_near(address_short)
#define pgm_read_float_near(address_short) (*address_short)
#define pgm_read_float(address_short) pgm_read_float_near(address_short)
#define pgm_read_word_near(address_short) (*address_short)
#define pgm_read_word(address_short) pgm_read_word_near(address_short)
#define pgm_read_dword_near(address_short) (*address_short)
#define pgm_read_dword(address_short) pgm_read_dword_near(address_short)

#define sprintf_P sprintf
#define strstr_P strstr
#define strncpy_P strncpy
#define vsnprintf_P vsnprintf
#define strcpy_P strcpy
#define snprintf_P snprintf

void serialprintPGM(const char *);

// Time functions
extern "C" {
void delay(int milis);
}
void _delay_ms(int delay);
void delayMicroseconds(unsigned long);
uint32_t millis();

//IO functions
void pinMode(int pin_number, int mode);
void digitalWrite(int pin_number, int pin_status);
bool digitalRead(int pin);
void analogWrite(int pin_number, int pin_status);
uint16_t analogRead(int adc_pin);

// EEPROM
void eeprom_write_byte(unsigned char *pos, unsigned char value);
unsigned char eeprom_read_byte(unsigned char *pos);
void eeprom_read_block (void *__dst, const void *__src, size_t __n);
void eeprom_update_block (const void *__src, void *__dst, size_t __n);

int32_t random(int32_t);
int32_t random(int32_t, int32_t);
void randomSeed(uint32_t);

char *dtostrf (double __val, signed char __width, unsigned char __prec, char *__s);

int map(uint16_t x, uint16_t in_min, uint16_t in_max, uint16_t out_min, uint16_t out_max);

#endif // __ARDUINO_DEF_H__
