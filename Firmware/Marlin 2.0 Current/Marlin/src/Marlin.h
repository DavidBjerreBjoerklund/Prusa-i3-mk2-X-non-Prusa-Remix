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
#ifndef MARLIN_H
#define MARLIN_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "inc/MarlinConfig.h"

#ifdef DEBUG_GCODE_PARSER
  #include "gcode/parser.h"
#endif

#include "core/utility.h"
#include "core/serial.h"

// PrintCounter or Stopwatch
#include "module/printcounter.h"

void stop();

void idle(
  #if ENABLED(ADVANCED_PAUSE_FEATURE)
    bool no_stepper_sleep = false  // pass true to keep steppers from disabling on timeout
  #endif
);

void manage_inactivity(bool ignore_stepper_queue = false);

#if HAS_X2_ENABLE
  #define  enable_X() do{ X_ENABLE_WRITE( X_ENABLE_ON); X2_ENABLE_WRITE( X_ENABLE_ON); }while(0)
  #define disable_X() do{ X_ENABLE_WRITE(!X_ENABLE_ON); X2_ENABLE_WRITE(!X_ENABLE_ON); axis_known_position[X_AXIS] = false; }while(0)
#elif HAS_X_ENABLE
  #define  enable_X() X_ENABLE_WRITE( X_ENABLE_ON)
  #define disable_X() do{ X_ENABLE_WRITE(!X_ENABLE_ON); axis_known_position[X_AXIS] = false; }while(0)
#else
  #define  enable_X() NOOP
  #define disable_X() NOOP
#endif

#if HAS_Y2_ENABLE
  #define  enable_Y() do{ Y_ENABLE_WRITE( Y_ENABLE_ON); Y2_ENABLE_WRITE(Y_ENABLE_ON); }while(0)
  #define disable_Y() do{ Y_ENABLE_WRITE(!Y_ENABLE_ON); Y2_ENABLE_WRITE(!Y_ENABLE_ON); axis_known_position[Y_AXIS] = false; }while(0)
#elif HAS_Y_ENABLE
  #define  enable_Y() Y_ENABLE_WRITE( Y_ENABLE_ON)
  #define disable_Y() do{ Y_ENABLE_WRITE(!Y_ENABLE_ON); axis_known_position[Y_AXIS] = false; }while(0)
#else
  #define  enable_Y() NOOP
  #define disable_Y() NOOP
#endif

#if HAS_Z2_ENABLE
  #define  enable_Z() do{ Z_ENABLE_WRITE( Z_ENABLE_ON); Z2_ENABLE_WRITE(Z_ENABLE_ON); }while(0)
  #define disable_Z() do{ Z_ENABLE_WRITE(!Z_ENABLE_ON); Z2_ENABLE_WRITE(!Z_ENABLE_ON); axis_known_position[Z_AXIS] = false; }while(0)
#elif HAS_Z_ENABLE
  #define  enable_Z() Z_ENABLE_WRITE( Z_ENABLE_ON)
  #define disable_Z() do{ Z_ENABLE_WRITE(!Z_ENABLE_ON); axis_known_position[Z_AXIS] = false; }while(0)
#else
  #define  enable_Z() NOOP
  #define disable_Z() NOOP
#endif

#if ENABLED(MIXING_EXTRUDER)

  /**
   * Mixing steppers synchronize their enable (and direction) together
   */
  #if MIXING_STEPPERS > 3
    #define  enable_E0() { E0_ENABLE_WRITE( E_ENABLE_ON); E1_ENABLE_WRITE( E_ENABLE_ON); E2_ENABLE_WRITE( E_ENABLE_ON); E3_ENABLE_WRITE( E_ENABLE_ON); }
    #define disable_E0() { E0_ENABLE_WRITE(!E_ENABLE_ON); E1_ENABLE_WRITE(!E_ENABLE_ON); E2_ENABLE_WRITE(!E_ENABLE_ON); E3_ENABLE_WRITE(!E_ENABLE_ON); }
  #elif MIXING_STEPPERS > 2
    #define  enable_E0() { E0_ENABLE_WRITE( E_ENABLE_ON); E1_ENABLE_WRITE( E_ENABLE_ON); E2_ENABLE_WRITE( E_ENABLE_ON); }
    #define disable_E0() { E0_ENABLE_WRITE(!E_ENABLE_ON); E1_ENABLE_WRITE(!E_ENABLE_ON); E2_ENABLE_WRITE(!E_ENABLE_ON); }
  #else
    #define  enable_E0() { E0_ENABLE_WRITE( E_ENABLE_ON); E1_ENABLE_WRITE( E_ENABLE_ON); }
    #define disable_E0() { E0_ENABLE_WRITE(!E_ENABLE_ON); E1_ENABLE_WRITE(!E_ENABLE_ON); }
  #endif
  #define  enable_E1() NOOP
  #define disable_E1() NOOP
  #define  enable_E2() NOOP
  #define disable_E2() NOOP
  #define  enable_E3() NOOP
  #define disable_E3() NOOP
  #define  enable_E4() NOOP
  #define disable_E4() NOOP

#else // !MIXING_EXTRUDER

  #if HAS_E0_ENABLE
    #define  enable_E0() E0_ENABLE_WRITE( E_ENABLE_ON)
    #define disable_E0() E0_ENABLE_WRITE(!E_ENABLE_ON)
  #else
    #define  enable_E0() NOOP
    #define disable_E0() NOOP
  #endif

  #if E_STEPPERS > 1 && HAS_E1_ENABLE
    #define  enable_E1() E1_ENABLE_WRITE( E_ENABLE_ON)
    #define disable_E1() E1_ENABLE_WRITE(!E_ENABLE_ON)
  #else
    #define  enable_E1() NOOP
    #define disable_E1() NOOP
  #endif

  #if E_STEPPERS > 2 && HAS_E2_ENABLE
    #define  enable_E2() E2_ENABLE_WRITE( E_ENABLE_ON)
    #define disable_E2() E2_ENABLE_WRITE(!E_ENABLE_ON)
  #else
    #define  enable_E2() NOOP
    #define disable_E2() NOOP
  #endif

  #if E_STEPPERS > 3 && HAS_E3_ENABLE
    #define  enable_E3() E3_ENABLE_WRITE( E_ENABLE_ON)
    #define disable_E3() E3_ENABLE_WRITE(!E_ENABLE_ON)
  #else
    #define  enable_E3() NOOP
    #define disable_E3() NOOP
  #endif

  #if E_STEPPERS > 4 && HAS_E4_ENABLE
    #define  enable_E4() E4_ENABLE_WRITE( E_ENABLE_ON)
    #define disable_E4() E4_ENABLE_WRITE(!E_ENABLE_ON)
  #else
    #define  enable_E4() NOOP
    #define disable_E4() NOOP
  #endif

#endif // !MIXING_EXTRUDER

#if ENABLED(G38_PROBE_TARGET)
  extern bool G38_move,        // flag to tell the interrupt handler that a G38 command is being run
              G38_endstop_hit; // flag from the interrupt handler to indicate if the endstop went active
#endif

/**
 * The axis order in all axis related arrays is X, Y, Z, E
 */
#define _AXIS(AXIS) AXIS ##_AXIS

void enable_all_steppers();
void disable_e_steppers();
void disable_all_steppers();

void kill(const char*);

void quickstop_stepper();

#if ENABLED(FILAMENT_RUNOUT_SENSOR)
  void handle_filament_runout();
#endif

extern bool Running;
inline bool IsRunning() { return  Running; }
inline bool IsStopped() { return !Running; }

/**
 * Feedrate scaling and conversion
 */
extern int16_t feedrate_percentage;

#define MMS_SCALED(MM_S) ((MM_S)*feedrate_percentage*0.01)

extern bool volumetric_enabled;
extern float filament_size[EXTRUDERS]; // cross-sectional area of filament (in millimeters), typically around 1.75 or 2.85, 0 disables the volumetric calculations for the extruder.
extern float volumetric_multiplier[EXTRUDERS]; // reciprocal of cross-sectional area of filament (in square millimeters), stored this way to reduce computational burden in planner

extern bool axis_known_position[XYZ];
extern bool axis_homed[XYZ];
extern volatile bool wait_for_heatup;

#if HAS_RESUME_CONTINUE
  extern volatile bool wait_for_user;
#endif

#if HAS_WORKSPACE_OFFSET || ENABLED(DUAL_X_CARRIAGE)
  void update_software_endstops(const AxisEnum axis);
#endif

#if ENABLED(AUTO_BED_LEVELING_UBL)
  typedef struct { double A, B, D; } linear_fit;
  linear_fit* lsf_linear_fit(double x[], double y[], double z[], const int);
#endif

#if ENABLED(Z_DUAL_ENDSTOPS)
  extern float z_endstop_adj;
#endif

#if HAS_SERVOS
  #include "HAL/servo.h"
  extern HAL_SERVO_LIB servo[NUM_SERVOS];
  #define MOVE_SERVO(I, P) servo[I].move(P)
  #if HAS_Z_SERVO_ENDSTOP
    #define DEPLOY_Z_SERVO() MOVE_SERVO(Z_ENDSTOP_SERVO_NR, z_servo_angle[0])
    #define STOW_Z_SERVO() MOVE_SERVO(Z_ENDSTOP_SERVO_NR, z_servo_angle[1])
  #endif
#endif

#if FAN_COUNT > 0
  extern int16_t fanSpeeds[FAN_COUNT];
  #if ENABLED(PROBING_FANS_OFF)
    extern bool fans_paused;
    extern int16_t paused_fanSpeeds[FAN_COUNT];
  #endif
#endif

#if ENABLED(BARICUDA)
  extern uint8_t baricuda_valve_pressure, baricuda_e_to_p_pressure;
#endif

#if ENABLED(ADVANCED_PAUSE_FEATURE)
  extern AdvancedPauseMenuResponse advanced_pause_menu_response;
#endif

#if ENABLED(PID_EXTRUSION_SCALING)
  extern int lpq_len;
#endif

#if HAS_TEMP_HOTEND || HAS_TEMP_BED
  void print_heaterstates();
#endif

#if ENABLED(MIXING_EXTRUDER)
  extern float mixing_factor[MIXING_STEPPERS];
  #if MIXING_VIRTUAL_TOOLS > 1
    extern float mixing_virtual_tool_mix[MIXING_VIRTUAL_TOOLS][MIXING_STEPPERS];
  #endif
#endif

void calculate_volumetric_multipliers();

void set_axis_is_at_home(const AxisEnum axis);

// Buzzer - Use a BEEPER_PIN if specified
#if ENABLED(LCD_USE_I2C_BUZZER)
  // will be defined in ultralcd.h
#elif PIN_EXISTS(BEEPER)
  #include "libs/buzzer.h"
  #define BUZZ(d,f) buzzer.tone(d, f)
#else
  #define BUZZ(d,f) NOOP
#endif

#endif // MARLIN_H
