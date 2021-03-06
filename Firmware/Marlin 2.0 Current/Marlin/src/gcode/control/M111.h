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

/**
 * M111: Set the debug level
 */
void gcode_M111() {
  if (parser.seen('S')) marlin_debug_flags = parser.byteval('S');

  const static char str_debug_1[] PROGMEM = MSG_DEBUG_ECHO,
                    str_debug_2[] PROGMEM = MSG_DEBUG_INFO,
                    str_debug_4[] PROGMEM = MSG_DEBUG_ERRORS,
                    str_debug_8[] PROGMEM = MSG_DEBUG_DRYRUN,
                    str_debug_16[] PROGMEM = MSG_DEBUG_COMMUNICATION
                    #if ENABLED(DEBUG_LEVELING_FEATURE)
                      , str_debug_32[] PROGMEM = MSG_DEBUG_LEVELING
                    #endif
                    ;

  const static char* const debug_strings[] PROGMEM = {
    str_debug_1, str_debug_2, str_debug_4, str_debug_8, str_debug_16
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      , str_debug_32
    #endif
  };

  SERIAL_ECHO_START();
  SERIAL_ECHOPGM(MSG_DEBUG_PREFIX);
  if (marlin_debug_flags) {
    uint8_t comma = 0;
    for (uint8_t i = 0; i < COUNT(debug_strings); i++) {
      if (TEST(marlin_debug_flags, i)) {
        if (comma++) SERIAL_CHAR(',');
        serialprintPGM((char*)pgm_read_word(&debug_strings[i]));
      }
    }
  }
  else {
    SERIAL_ECHOPGM(MSG_DEBUG_OFF);
  }
  SERIAL_EOL();
}
