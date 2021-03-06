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
 * probe.h - Move, deploy, enable, etc.
 */

#ifndef PROBE_H
#define PROBE_H

#include "../inc/MarlinConfig.h"

bool set_probe_deployed(const bool deploy);
float probe_pt(const float &lx, const float &ly, const bool, const uint8_t, const bool printable=true);

#if HAS_BED_PROBE
  extern float zprobe_zoffset;
  void refresh_zprobe_zoffset(const bool no_babystep=false);
  #define DEPLOY_PROBE() set_probe_deployed(true)
  #define STOW_PROBE() set_probe_deployed(false)
#else
  #define DEPLOY_PROBE()
  #define STOW_PROBE()
#endif

#if HAS_Z_SERVO_ENDSTOP
  extern const int z_servo_angle[2];
  void servo_probe_init();
#endif

#endif // PROBE_H
