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
 * motion.cpp
 */

#include "motion.h"
#include "endstops.h"
#include "stepper.h"
#include "planner.h"
#include "temperature.h"

#include "../gcode/gcode.h"
#include "../core/serial.h"

#include "../inc/MarlinConfig.h"

// #if ENABLED(DUAL_X_CARRIAGE)
//   #include "tool_change.h"
// #endif

#if HAS_BED_PROBE
  #include "probe.h"
#endif

#if HAS_LEVELING
  #include "../feature/bedlevel/bedlevel.h"
#endif

#if NEED_UNHOMED_ERR && ENABLED(ULTRA_LCD)
  #include "../lcd/ultralcd.h"
#endif

#if ENABLED(SENSORLESS_HOMING)
  #include "../feature/tmc2130.h"
#endif

#define XYZ_CONSTS(type, array, CONFIG) const PROGMEM type array##_P[XYZ] = { X_##CONFIG, Y_##CONFIG, Z_##CONFIG }

XYZ_CONSTS(float, base_min_pos,   MIN_POS);
XYZ_CONSTS(float, base_max_pos,   MAX_POS);
XYZ_CONSTS(float, base_home_pos,  HOME_POS);
XYZ_CONSTS(float, max_length,     MAX_LENGTH);
XYZ_CONSTS(float, home_bump_mm,   HOME_BUMP_MM);
XYZ_CONSTS(signed char, home_dir, HOME_DIR);

// Relative Mode. Enable with G91, disable with G90.
bool relative_mode = false;

/**
 * Cartesian Current Position
 *   Used to track the logical position as moves are queued.
 *   Used by 'line_to_current_position' to do a move after changing it.
 *   Used by 'SYNC_PLAN_POSITION_KINEMATIC' to update 'planner.position'.
 */
float current_position[XYZE] = { 0.0 };

/**
 * Cartesian Destination
 *   A temporary position, usually applied to 'current_position'.
 *   Set with 'get_destination_from_command' or 'set_destination_to_current'.
 *   'line_to_destination' sets 'current_position' to 'destination'.
 */
float destination[XYZE] = { 0.0 };

// The active extruder (tool). Set with T<extruder> command.
uint8_t active_extruder = 0;

// Extruder offsets
#if HOTENDS > 1
  float hotend_offset[XYZ][HOTENDS]; // Initialized by settings.load()
#endif

// The feedrate for the current move, often used as the default if
// no other feedrate is specified. Overridden for special moves.
// Set by the last G0 through G5 command's "F" parameter.
// Functions that override this for custom moves *must always* restore it!
float feedrate_mm_s = MMM_TO_MMS(1500.0);

// Homing feedrate is const progmem - compare to constexpr in the header
const float homing_feedrate_mm_s[4] PROGMEM = {
  #if ENABLED(DELTA)
    MMM_TO_MMS(HOMING_FEEDRATE_Z), MMM_TO_MMS(HOMING_FEEDRATE_Z),
  #else
    MMM_TO_MMS(HOMING_FEEDRATE_XY), MMM_TO_MMS(HOMING_FEEDRATE_XY),
  #endif
  MMM_TO_MMS(HOMING_FEEDRATE_Z), 0
};

// Cartesian conversion result goes here:
float cartes[XYZ];

// Until kinematics.cpp is created, create this here
#if IS_KINEMATIC
  float delta[ABC];
#endif

#if OLDSCHOOL_ABL
  float xy_probe_feedrate_mm_s = MMM_TO_MMS(XY_PROBE_SPEED);
#endif

/**
 * Output the current position to serial
 */
void report_current_position() {
  SERIAL_PROTOCOLPGM("X:");
  SERIAL_PROTOCOL(current_position[X_AXIS]);
  SERIAL_PROTOCOLPGM(" Y:");
  SERIAL_PROTOCOL(current_position[Y_AXIS]);
  SERIAL_PROTOCOLPGM(" Z:");
  SERIAL_PROTOCOL(current_position[Z_AXIS]);
  SERIAL_PROTOCOLPGM(" E:");
  SERIAL_PROTOCOL(current_position[E_AXIS]);

  stepper.report_positions();

  #if IS_SCARA
    scara_report_positions();
  #endif
}

/**
 * sync_plan_position
 *
 * Set the planner/stepper positions directly from current_position with
 * no kinematic translation. Used for homing axes and cartesian/core syncing.
 */
void sync_plan_position() {
  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) DEBUG_POS("sync_plan_position", current_position);
  #endif
  planner.set_position_mm(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS]);
}

void sync_plan_position_e() { planner.set_e_position_mm(current_position[E_AXIS]); }

/**
 * Get the stepper positions in the cartes[] array.
 * Forward kinematics are applied for DELTA and SCARA.
 *
 * The result is in the current coordinate space with
 * leveling applied. The coordinates need to be run through
 * unapply_leveling to obtain the "ideal" coordinates
 * suitable for current_position, etc.
 */
void get_cartesian_from_steppers() {
  #if ENABLED(DELTA)
    forward_kinematics_DELTA(
      stepper.get_axis_position_mm(A_AXIS),
      stepper.get_axis_position_mm(B_AXIS),
      stepper.get_axis_position_mm(C_AXIS)
    );
    cartes[X_AXIS] += LOGICAL_X_POSITION(0);
    cartes[Y_AXIS] += LOGICAL_Y_POSITION(0);
    cartes[Z_AXIS] += LOGICAL_Z_POSITION(0);
  #elif IS_SCARA
    forward_kinematics_SCARA(
      stepper.get_axis_position_degrees(A_AXIS),
      stepper.get_axis_position_degrees(B_AXIS)
    );
    cartes[X_AXIS] += LOGICAL_X_POSITION(0);
    cartes[Y_AXIS] += LOGICAL_Y_POSITION(0);
    cartes[Z_AXIS] = stepper.get_axis_position_mm(Z_AXIS);
  #else
    cartes[X_AXIS] = stepper.get_axis_position_mm(X_AXIS);
    cartes[Y_AXIS] = stepper.get_axis_position_mm(Y_AXIS);
    cartes[Z_AXIS] = stepper.get_axis_position_mm(Z_AXIS);
  #endif
}

/**
 * Set the current_position for an axis based on
 * the stepper positions, removing any leveling that
 * may have been applied.
 */
void set_current_from_steppers_for_axis(const AxisEnum axis) {
  get_cartesian_from_steppers();
  #if PLANNER_LEVELING
    planner.unapply_leveling(cartes);
  #endif
  if (axis == ALL_AXES)
    COPY(current_position, cartes);
  else
    current_position[axis] = cartes[axis];
}

/**
 * Move the planner to the current position from wherever it last moved
 * (or from wherever it has been told it is located).
 */
void line_to_current_position() {
  planner.buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], feedrate_mm_s, active_extruder);
}

/**
 * Move the planner to the position stored in the destination array, which is
 * used by G0/G1/G2/G3/G5 and many other functions to set a destination.
 */
void line_to_destination(const float fr_mm_s) {
  planner.buffer_line(destination[X_AXIS], destination[Y_AXIS], destination[Z_AXIS], destination[E_AXIS], fr_mm_s, active_extruder);
}

#if IS_KINEMATIC

  void sync_plan_position_kinematic() {
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) DEBUG_POS("sync_plan_position_kinematic", current_position);
    #endif
    planner.set_position_mm_kinematic(current_position);
  }

  /**
   * Calculate delta, start a line, and set current_position to destination
   */
  void prepare_uninterpolated_move_to_destination(const float fr_mm_s/*=0.0*/) {
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) DEBUG_POS("prepare_uninterpolated_move_to_destination", destination);
    #endif

    gcode.refresh_cmd_timeout();

    #if UBL_DELTA
      // ubl segmented line will do z-only moves in single segment
      ubl.prepare_segmented_line_to(destination, MMS_SCALED(fr_mm_s ? fr_mm_s : feedrate_mm_s));
    #else
      if ( current_position[X_AXIS] == destination[X_AXIS]
        && current_position[Y_AXIS] == destination[Y_AXIS]
        && current_position[Z_AXIS] == destination[Z_AXIS]
        && current_position[E_AXIS] == destination[E_AXIS]
      ) return;

      planner.buffer_line_kinematic(destination, MMS_SCALED(fr_mm_s ? fr_mm_s : feedrate_mm_s), active_extruder);
    #endif

    set_current_to_destination();
  }

#endif // IS_KINEMATIC

/**
 *  Plan a move to (X, Y, Z) and set the current_position
 *  The final current_position may not be the one that was requested
 */
void do_blocking_move_to(const float &lx, const float &ly, const float &lz, const float &fr_mm_s/*=0.0*/) {
  const float old_feedrate_mm_s = feedrate_mm_s;

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) print_xyz(PSTR(">>> do_blocking_move_to"), NULL, lx, ly, lz);
  #endif

  #if ENABLED(DELTA)

    if (!position_is_reachable_xy(lx, ly)) return;

    feedrate_mm_s = fr_mm_s ? fr_mm_s : XY_PROBE_FEEDRATE_MM_S;

    set_destination_to_current();          // sync destination at the start

    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) DEBUG_POS("set_destination_to_current", destination);
    #endif

    // when in the danger zone
    if (current_position[Z_AXIS] > delta_clip_start_height) {
      if (lz > delta_clip_start_height) {   // staying in the danger zone
        destination[X_AXIS] = lx;           // move directly (uninterpolated)
        destination[Y_AXIS] = ly;
        destination[Z_AXIS] = lz;
        prepare_uninterpolated_move_to_destination(); // set_current_to_destination
        #if ENABLED(DEBUG_LEVELING_FEATURE)
          if (DEBUGGING(LEVELING)) DEBUG_POS("danger zone move", current_position);
        #endif
        return;
      }
      else {
        destination[Z_AXIS] = delta_clip_start_height;
        prepare_uninterpolated_move_to_destination(); // set_current_to_destination
        #if ENABLED(DEBUG_LEVELING_FEATURE)
          if (DEBUGGING(LEVELING)) DEBUG_POS("zone border move", current_position);
        #endif
      }
    }

    if (lz > current_position[Z_AXIS]) {    // raising?
      destination[Z_AXIS] = lz;
      prepare_uninterpolated_move_to_destination();   // set_current_to_destination
      #if ENABLED(DEBUG_LEVELING_FEATURE)
        if (DEBUGGING(LEVELING)) DEBUG_POS("z raise move", current_position);
      #endif
    }

    destination[X_AXIS] = lx;
    destination[Y_AXIS] = ly;
    prepare_move_to_destination();         // set_current_to_destination
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) DEBUG_POS("xy move", current_position);
    #endif

    if (lz < current_position[Z_AXIS]) {    // lowering?
      destination[Z_AXIS] = lz;
      prepare_uninterpolated_move_to_destination();   // set_current_to_destination
      #if ENABLED(DEBUG_LEVELING_FEATURE)
        if (DEBUGGING(LEVELING)) DEBUG_POS("z lower move", current_position);
      #endif
    }

  #elif IS_SCARA

    if (!position_is_reachable_xy(lx, ly)) return;

    set_destination_to_current();

    // If Z needs to raise, do it before moving XY
    if (destination[Z_AXIS] < lz) {
      destination[Z_AXIS] = lz;
      prepare_uninterpolated_move_to_destination(fr_mm_s ? fr_mm_s : homing_feedrate(Z_AXIS));
    }

    destination[X_AXIS] = lx;
    destination[Y_AXIS] = ly;
    prepare_uninterpolated_move_to_destination(fr_mm_s ? fr_mm_s : XY_PROBE_FEEDRATE_MM_S);

    // If Z needs to lower, do it after moving XY
    if (destination[Z_AXIS] > lz) {
      destination[Z_AXIS] = lz;
      prepare_uninterpolated_move_to_destination(fr_mm_s ? fr_mm_s : homing_feedrate(Z_AXIS));
    }

  #else

    // If Z needs to raise, do it before moving XY
    if (current_position[Z_AXIS] < lz) {
      feedrate_mm_s = fr_mm_s ? fr_mm_s : homing_feedrate(Z_AXIS);
      current_position[Z_AXIS] = lz;
      line_to_current_position();
    }

    feedrate_mm_s = fr_mm_s ? fr_mm_s : XY_PROBE_FEEDRATE_MM_S;
    current_position[X_AXIS] = lx;
    current_position[Y_AXIS] = ly;
    line_to_current_position();

    // If Z needs to lower, do it after moving XY
    if (current_position[Z_AXIS] > lz) {
      feedrate_mm_s = fr_mm_s ? fr_mm_s : homing_feedrate(Z_AXIS);
      current_position[Z_AXIS] = lz;
      line_to_current_position();
    }

  #endif

  stepper.synchronize();

  feedrate_mm_s = old_feedrate_mm_s;

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("<<< do_blocking_move_to");
  #endif
}
void do_blocking_move_to_x(const float &lx, const float &fr_mm_s/*=0.0*/) {
  do_blocking_move_to(lx, current_position[Y_AXIS], current_position[Z_AXIS], fr_mm_s);
}
void do_blocking_move_to_z(const float &lz, const float &fr_mm_s/*=0.0*/) {
  do_blocking_move_to(current_position[X_AXIS], current_position[Y_AXIS], lz, fr_mm_s);
}
void do_blocking_move_to_xy(const float &lx, const float &ly, const float &fr_mm_s/*=0.0*/) {
  do_blocking_move_to(lx, ly, current_position[Z_AXIS], fr_mm_s);
}

//
// Prepare to do endstop or probe moves
// with custom feedrates.
//
//  - Save current feedrates
//  - Reset the rate multiplier
//  - Reset the command timeout
//  - Enable the endstops (for endstop moves)
//
void bracket_probe_move(const bool before) {
  static float saved_feedrate_mm_s;
  static int16_t saved_feedrate_percentage;
  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) DEBUG_POS("bracket_probe_move", current_position);
  #endif
  if (before) {
    saved_feedrate_mm_s = feedrate_mm_s;
    saved_feedrate_percentage = feedrate_percentage;
    feedrate_percentage = 100;
    gcode.refresh_cmd_timeout();
  }
  else {
    feedrate_mm_s = saved_feedrate_mm_s;
    feedrate_percentage = saved_feedrate_percentage;
    gcode.refresh_cmd_timeout();
  }
}

void setup_for_endstop_or_probe_move() { bracket_probe_move(true); }
void clean_up_after_endstop_or_probe_move() { bracket_probe_move(false); }

// Software Endstops are based on the configured limits.
float soft_endstop_min[XYZ] = { X_MIN_BED, Y_MIN_BED, Z_MIN_POS },
      soft_endstop_max[XYZ] = { X_MAX_BED, Y_MAX_BED, Z_MAX_POS };

#if HAS_SOFTWARE_ENDSTOPS

  // Software Endstops are based on the configured limits.
  bool soft_endstops_enabled = true;

  /**
   * Constrain the given coordinates to the software endstops.
   */

  // NOTE: This makes no sense for delta beds other than Z-axis.
  //       For delta the X/Y would need to be clamped at
  //       DELTA_PRINTABLE_RADIUS from center of bed, but delta
  //       now enforces is_position_reachable for X/Y regardless
  //       of HAS_SOFTWARE_ENDSTOPS, so that enforcement would be
  //       redundant here.

  void clamp_to_software_endstops(float target[XYZ]) {
    if (!soft_endstops_enabled) return;
    #if ENABLED(MIN_SOFTWARE_ENDSTOPS)
      #if DISABLED(DELTA)
        NOLESS(target[X_AXIS], soft_endstop_min[X_AXIS]);
        NOLESS(target[Y_AXIS], soft_endstop_min[Y_AXIS]);
      #endif
      NOLESS(target[Z_AXIS], soft_endstop_min[Z_AXIS]);
    #endif
    #if ENABLED(MAX_SOFTWARE_ENDSTOPS)
      #if DISABLED(DELTA)
        NOMORE(target[X_AXIS], soft_endstop_max[X_AXIS]);
        NOMORE(target[Y_AXIS], soft_endstop_max[Y_AXIS]);
      #endif
      NOMORE(target[Z_AXIS], soft_endstop_max[Z_AXIS]);
    #endif
  }

#endif

#if IS_KINEMATIC && !UBL_DELTA

  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    #if ENABLED(DELTA)
      #define ADJUST_DELTA(V) \
        if (planner.abl_enabled) { \
          const float zadj = bilinear_z_offset(V); \
          delta[A_AXIS] += zadj; \
          delta[B_AXIS] += zadj; \
          delta[C_AXIS] += zadj; \
        }
    #else
      #define ADJUST_DELTA(V) if (planner.abl_enabled) { delta[Z_AXIS] += bilinear_z_offset(V); }
    #endif
  #else
    #define ADJUST_DELTA(V) NOOP
  #endif

  /**
   * Prepare a linear move in a DELTA or SCARA setup.
   *
   * This calls planner.buffer_line several times, adding
   * small incremental moves for DELTA or SCARA.
   */
  inline bool prepare_kinematic_move_to(float ltarget[XYZE]) {

    // Get the top feedrate of the move in the XY plane
    const float _feedrate_mm_s = MMS_SCALED(feedrate_mm_s);

    // If the move is only in Z/E don't split up the move
    if (ltarget[X_AXIS] == current_position[X_AXIS] && ltarget[Y_AXIS] == current_position[Y_AXIS]) {
      planner.buffer_line_kinematic(ltarget, _feedrate_mm_s, active_extruder);
      return false;
    }

    // Fail if attempting move outside printable radius
    if (!position_is_reachable_xy(ltarget[X_AXIS], ltarget[Y_AXIS])) return true;

    // Get the cartesian distances moved in XYZE
    const float difference[XYZE] = {
      ltarget[X_AXIS] - current_position[X_AXIS],
      ltarget[Y_AXIS] - current_position[Y_AXIS],
      ltarget[Z_AXIS] - current_position[Z_AXIS],
      ltarget[E_AXIS] - current_position[E_AXIS]
    };

    // Get the linear distance in XYZ
    float cartesian_mm = SQRT(sq(difference[X_AXIS]) + sq(difference[Y_AXIS]) + sq(difference[Z_AXIS]));

    // If the move is very short, check the E move distance
    if (UNEAR_ZERO(cartesian_mm)) cartesian_mm = FABS(difference[E_AXIS]);

    // No E move either? Game over.
    if (UNEAR_ZERO(cartesian_mm)) return true;

    // Minimum number of seconds to move the given distance
    const float seconds = cartesian_mm / _feedrate_mm_s;

    // The number of segments-per-second times the duration
    // gives the number of segments
    uint16_t segments = delta_segments_per_second * seconds;

    // For SCARA minimum segment size is 0.25mm
    #if IS_SCARA
      NOMORE(segments, cartesian_mm * 4);
    #endif

    // At least one segment is required
    NOLESS(segments, 1);

    // The approximate length of each segment
    const float inv_segments = 1.0 / float(segments),
                segment_distance[XYZE] = {
                  difference[X_AXIS] * inv_segments,
                  difference[Y_AXIS] * inv_segments,
                  difference[Z_AXIS] * inv_segments,
                  difference[E_AXIS] * inv_segments
                };

    // SERIAL_ECHOPAIR("mm=", cartesian_mm);
    // SERIAL_ECHOPAIR(" seconds=", seconds);
    // SERIAL_ECHOLNPAIR(" segments=", segments);

    #if IS_SCARA && ENABLED(SCARA_FEEDRATE_SCALING)
      // SCARA needs to scale the feed rate from mm/s to degrees/s
      const float inv_segment_length = min(10.0, float(segments) / cartesian_mm), // 1/mm/segs
                  feed_factor = inv_segment_length * _feedrate_mm_s;
      float oldA = stepper.get_axis_position_degrees(A_AXIS),
            oldB = stepper.get_axis_position_degrees(B_AXIS);
    #endif

    // Get the logical current position as starting point
    float logical[XYZE];
    COPY(logical, current_position);

    // Drop one segment so the last move is to the exact target.
    // If there's only 1 segment, loops will be skipped entirely.
    --segments;

    // Calculate and execute the segments
    for (uint16_t s = segments + 1; --s;) {
      LOOP_XYZE(i) logical[i] += segment_distance[i];
      #if ENABLED(DELTA)
        DELTA_LOGICAL_IK(); // Delta can inline its kinematics
      #else
        inverse_kinematics(logical);
      #endif

      ADJUST_DELTA(logical); // Adjust Z if bed leveling is enabled

      #if IS_SCARA && ENABLED(SCARA_FEEDRATE_SCALING)
        // For SCARA scale the feed rate from mm/s to degrees/s
        // Use ratio between the length of the move and the larger angle change
        const float adiff = abs(delta[A_AXIS] - oldA),
                    bdiff = abs(delta[B_AXIS] - oldB);
        planner.buffer_line(delta[A_AXIS], delta[B_AXIS], delta[C_AXIS], logical[E_AXIS], max(adiff, bdiff) * feed_factor, active_extruder);
        oldA = delta[A_AXIS];
        oldB = delta[B_AXIS];
      #else
        planner.buffer_line(delta[A_AXIS], delta[B_AXIS], delta[C_AXIS], logical[E_AXIS], _feedrate_mm_s, active_extruder);
      #endif
    }

    // Since segment_distance is only approximate,
    // the final move must be to the exact destination.

    #if IS_SCARA && ENABLED(SCARA_FEEDRATE_SCALING)
      // For SCARA scale the feed rate from mm/s to degrees/s
      // With segments > 1 length is 1 segment, otherwise total length
      inverse_kinematics(ltarget);
      ADJUST_DELTA(ltarget);
      const float adiff = abs(delta[A_AXIS] - oldA),
                  bdiff = abs(delta[B_AXIS] - oldB);
      planner.buffer_line(delta[A_AXIS], delta[B_AXIS], delta[C_AXIS], logical[E_AXIS], max(adiff, bdiff) * feed_factor, active_extruder);
    #else
      planner.buffer_line_kinematic(ltarget, _feedrate_mm_s, active_extruder);
    #endif

    return false;
  }

#else // !IS_KINEMATIC || UBL_DELTA

  /**
   * Prepare a linear move in a Cartesian setup.
   * If Mesh Bed Leveling is enabled, perform a mesh move.
   *
   * Returns true if the caller didn't update current_position.
   */
  inline bool prepare_move_to_destination_cartesian() {
    #if ENABLED(AUTO_BED_LEVELING_UBL)
      const float fr_scaled = MMS_SCALED(feedrate_mm_s);
      if (ubl.state.active) { // direct use of ubl.state.active for speed
        ubl.line_to_destination_cartesian(fr_scaled, active_extruder);
        return true;
      }
      else
        line_to_destination(fr_scaled);
    #else
      // Do not use feedrate_percentage for E or Z only moves
      if (current_position[X_AXIS] == destination[X_AXIS] && current_position[Y_AXIS] == destination[Y_AXIS])
        line_to_destination();
      else {
        const float fr_scaled = MMS_SCALED(feedrate_mm_s);
        #if ENABLED(MESH_BED_LEVELING)
          if (mbl.active()) { // direct used of mbl.active() for speed
            mesh_line_to_destination(fr_scaled);
            return true;
          }
          else
        #elif ENABLED(AUTO_BED_LEVELING_BILINEAR)
          if (planner.abl_enabled) { // direct use of abl_enabled for speed
            bilinear_line_to_destination(fr_scaled);
            return true;
          }
          else
        #endif
            line_to_destination(fr_scaled);
      }
    #endif
    return false;
  }

#endif // !IS_KINEMATIC || UBL_DELTA

#if ENABLED(DUAL_X_CARRIAGE) || ENABLED(DUAL_NOZZLE_DUPLICATION_MODE)
  bool extruder_duplication_enabled = false;                              // Used in Dual X mode 2
#endif

#if ENABLED(DUAL_X_CARRIAGE)

  DualXMode dual_x_carriage_mode         = DEFAULT_DUAL_X_CARRIAGE_MODE;
  float inactive_extruder_x_pos          = X2_MAX_POS,                    // used in mode 0 & 1
        raised_parked_position[XYZE],                                     // used in mode 1
        duplicate_extruder_x_offset      = DEFAULT_DUPLICATION_X_OFFSET;  // used in mode 2
  bool active_extruder_parked            = false;                         // used in mode 1 & 2
  millis_t delayed_move_time             = 0;                             // used in mode 1
  int16_t duplicate_extruder_temp_offset = 0;                             // used in mode 2

  float x_home_pos(const int extruder) {
    if (extruder == 0)
      return LOGICAL_X_POSITION(base_home_pos(X_AXIS));
    else
      /**
       * In dual carriage mode the extruder offset provides an override of the
       * second X-carriage position when homed - otherwise X2_HOME_POS is used.
       * This allows soft recalibration of the second extruder home position
       * without firmware reflash (through the M218 command).
       */
      return LOGICAL_X_POSITION(hotend_offset[X_AXIS][1] > 0 ? hotend_offset[X_AXIS][1] : X2_HOME_POS);
  }

  /**
   * Prepare a linear move in a dual X axis setup
   */
  inline bool prepare_move_to_destination_dualx() {
    if (active_extruder_parked) {
      switch (dual_x_carriage_mode) {
        case DXC_FULL_CONTROL_MODE:
          break;
        case DXC_AUTO_PARK_MODE:
          if (current_position[E_AXIS] == destination[E_AXIS]) {
            // This is a travel move (with no extrusion)
            // Skip it, but keep track of the current position
            // (so it can be used as the start of the next non-travel move)
            if (delayed_move_time != 0xFFFFFFFFUL) {
              set_current_to_destination();
              NOLESS(raised_parked_position[Z_AXIS], destination[Z_AXIS]);
              delayed_move_time = millis();
              return true;
            }
          }
          // unpark extruder: 1) raise, 2) move into starting XY position, 3) lower
          for (uint8_t i = 0; i < 3; i++)
            planner.buffer_line(
              i == 0 ? raised_parked_position[X_AXIS] : current_position[X_AXIS],
              i == 0 ? raised_parked_position[Y_AXIS] : current_position[Y_AXIS],
              i == 2 ? current_position[Z_AXIS] : raised_parked_position[Z_AXIS],
              current_position[E_AXIS],
              i == 1 ? PLANNER_XY_FEEDRATE() : planner.max_feedrate_mm_s[Z_AXIS],
              active_extruder
            );
          delayed_move_time = 0;
          active_extruder_parked = false;
          #if ENABLED(DEBUG_LEVELING_FEATURE)
            if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Clear active_extruder_parked");
          #endif
          break;
        case DXC_DUPLICATION_MODE:
          if (active_extruder == 0) {
            #if ENABLED(DEBUG_LEVELING_FEATURE)
              if (DEBUGGING(LEVELING)) {
                SERIAL_ECHOPAIR("Set planner X", LOGICAL_X_POSITION(inactive_extruder_x_pos));
                SERIAL_ECHOLNPAIR(" ... Line to X", current_position[X_AXIS] + duplicate_extruder_x_offset);
              }
            #endif
            // move duplicate extruder into correct duplication position.
            planner.set_position_mm(
              LOGICAL_X_POSITION(inactive_extruder_x_pos),
              current_position[Y_AXIS],
              current_position[Z_AXIS],
              current_position[E_AXIS]
            );
            planner.buffer_line(
              current_position[X_AXIS] + duplicate_extruder_x_offset,
              current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS],
              planner.max_feedrate_mm_s[X_AXIS], 1
            );
            SYNC_PLAN_POSITION_KINEMATIC();
            stepper.synchronize();
            extruder_duplication_enabled = true;
            active_extruder_parked = false;
            #if ENABLED(DEBUG_LEVELING_FEATURE)
              if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Set extruder_duplication_enabled\nClear active_extruder_parked");
            #endif
          }
          else {
            #if ENABLED(DEBUG_LEVELING_FEATURE)
              if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Active extruder not 0");
            #endif
          }
          break;
      }
    }
    return false;
  }

#endif // DUAL_X_CARRIAGE

/**
 * Prepare a single move and get ready for the next one
 *
 * This may result in several calls to planner.buffer_line to
 * do smaller moves for DELTA, SCARA, mesh moves, etc.
 */
void prepare_move_to_destination() {
  clamp_to_software_endstops(destination);
  gcode.refresh_cmd_timeout();

  #if ENABLED(PREVENT_COLD_EXTRUSION)

    if (!DEBUGGING(DRYRUN)) {
      if (destination[E_AXIS] != current_position[E_AXIS]) {
        if (thermalManager.tooColdToExtrude(active_extruder)) {
          current_position[E_AXIS] = destination[E_AXIS]; // Behave as if the move really took place, but ignore E part
          SERIAL_ECHO_START();
          SERIAL_ECHOLNPGM(MSG_ERR_COLD_EXTRUDE_STOP);
        }
        #if ENABLED(PREVENT_LENGTHY_EXTRUDE)
          if (destination[E_AXIS] - current_position[E_AXIS] > EXTRUDE_MAXLENGTH) {
            current_position[E_AXIS] = destination[E_AXIS]; // Behave as if the move really took place, but ignore E part
            SERIAL_ECHO_START();
            SERIAL_ECHOLNPGM(MSG_ERR_LONG_EXTRUDE_STOP);
          }
        #endif
      }
    }

  #endif

  if (
    #if UBL_DELTA // Also works for CARTESIAN (smaller segments follow mesh more closely)
      ubl.prepare_segmented_line_to(destination, feedrate_mm_s)
    #elif IS_KINEMATIC
      prepare_kinematic_move_to(destination)
    #elif ENABLED(DUAL_X_CARRIAGE)
      prepare_move_to_destination_dualx() || prepare_move_to_destination_cartesian()
    #else
      prepare_move_to_destination_cartesian()
    #endif
  ) return;

  set_current_to_destination();
}

#if NEED_UNHOMED_ERR

  bool axis_unhomed_error(const bool x/*=true*/, const bool y/*=true*/, const bool z/*=true*/) {
    #if ENABLED(HOME_AFTER_DEACTIVATE)
      const bool xx = x && !axis_known_position[X_AXIS],
                 yy = y && !axis_known_position[Y_AXIS],
                 zz = z && !axis_known_position[Z_AXIS];
    #else
      const bool xx = x && !axis_homed[X_AXIS],
                 yy = y && !axis_homed[Y_AXIS],
                 zz = z && !axis_homed[Z_AXIS];
    #endif
    if (xx || yy || zz) {
      SERIAL_ECHO_START();
      SERIAL_ECHOPGM(MSG_HOME " ");
      if (xx) SERIAL_ECHOPGM(MSG_X);
      if (yy) SERIAL_ECHOPGM(MSG_Y);
      if (zz) SERIAL_ECHOPGM(MSG_Z);
      SERIAL_ECHOLNPGM(" " MSG_FIRST);

      #if ENABLED(ULTRA_LCD)
        lcd_status_printf_P(0, PSTR(MSG_HOME " %s%s%s " MSG_FIRST), xx ? MSG_X : "", yy ? MSG_Y : "", zz ? MSG_Z : "");
      #endif
      return true;
    }
    return false;
  }

#endif

/**
 * The homing feedrate may vary
 */
inline float get_homing_bump_feedrate(const AxisEnum axis) {
  static const uint8_t homing_bump_divisor[] PROGMEM = HOMING_BUMP_DIVISOR;
  uint8_t hbd = pgm_read_byte(&homing_bump_divisor[axis]);
  if (hbd < 1) {
    hbd = 10;
    SERIAL_ECHO_START();
    SERIAL_ECHOLNPGM("Warning: Homing Bump Divisor < 1");
  }
  return homing_feedrate(axis) / hbd;
}

/**
 * Home an individual linear axis
 */
static void do_homing_move(const AxisEnum axis, const float distance, const float fr_mm_s=0.0) {

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) {
      SERIAL_ECHOPAIR(">>> do_homing_move(", axis_codes[axis]);
      SERIAL_ECHOPAIR(", ", distance);
      SERIAL_ECHOPAIR(", ", fr_mm_s);
      SERIAL_CHAR(')');
      SERIAL_EOL();
    }
  #endif

  #if HOMING_Z_WITH_PROBE && ENABLED(BLTOUCH)
    const bool deploy_bltouch = (axis == Z_AXIS && distance < 0);
    if (deploy_bltouch) set_bltouch_deployed(true);
  #endif

  #if QUIET_PROBING
    if (axis == Z_AXIS) probing_pause(true);
  #endif

  // Tell the planner we're at Z=0
  current_position[axis] = 0;

  #if IS_SCARA
    SYNC_PLAN_POSITION_KINEMATIC();
    current_position[axis] = distance;
    inverse_kinematics(current_position);
    planner.buffer_line(delta[A_AXIS], delta[B_AXIS], delta[C_AXIS], current_position[E_AXIS], fr_mm_s ? fr_mm_s : homing_feedrate(axis), active_extruder);
  #else
    sync_plan_position();
    current_position[axis] = distance;
    planner.buffer_line(current_position[X_AXIS], current_position[Y_AXIS], current_position[Z_AXIS], current_position[E_AXIS], fr_mm_s ? fr_mm_s : homing_feedrate(axis), active_extruder);
  #endif

  stepper.synchronize();

  #if QUIET_PROBING
    if (axis == Z_AXIS) probing_pause(false);
  #endif

  #if HOMING_Z_WITH_PROBE && ENABLED(BLTOUCH)
    if (deploy_bltouch) set_bltouch_deployed(false);
  #endif

  endstops.hit_on_purpose();

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) {
      SERIAL_ECHOPAIR("<<< do_homing_move(", axis_codes[axis]);
      SERIAL_CHAR(')');
      SERIAL_EOL();
    }
  #endif
}

/**
 * Home an individual "raw axis" to its endstop.
 * This applies to XYZ on Cartesian and Core robots, and
 * to the individual ABC steppers on DELTA and SCARA.
 *
 * At the end of the procedure the axis is marked as
 * homed and the current position of that axis is updated.
 * Kinematic robots should wait till all axes are homed
 * before updating the current position.
 */

void homeaxis(const AxisEnum axis) {

  #if IS_SCARA
    // Only Z homing (with probe) is permitted
    if (axis != Z_AXIS) { BUZZ(100, 880); return; }
  #else
    #define CAN_HOME(A) \
      (axis == A##_AXIS && ((A##_MIN_PIN > -1 && A##_HOME_DIR < 0) || (A##_MAX_PIN > -1 && A##_HOME_DIR > 0)))
    if (!CAN_HOME(X) && !CAN_HOME(Y) && !CAN_HOME(Z)) return;
  #endif

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) {
      SERIAL_ECHOPAIR(">>> homeaxis(", axis_codes[axis]);
      SERIAL_CHAR(')');
      SERIAL_EOL();
    }
  #endif

  const int axis_home_dir =
    #if ENABLED(DUAL_X_CARRIAGE)
      (axis == X_AXIS) ? x_home_dir(active_extruder) :
    #endif
    home_dir(axis);

  // Homing Z towards the bed? Deploy the Z probe or endstop.
  #if HOMING_Z_WITH_PROBE
    if (axis == Z_AXIS && DEPLOY_PROBE()) return;
  #endif

  // Set a flag for Z motor locking
  #if ENABLED(Z_DUAL_ENDSTOPS)
    if (axis == Z_AXIS) stepper.set_homing_flag(true);
  #endif

  // Disable stealthChop if used. Enable diag1 pin on driver.
  #if ENABLED(SENSORLESS_HOMING)
    #if ENABLED(X_IS_TMC2130)
      if (axis == X_AXIS) tmc2130_sensorless_homing(stepperX);
    #endif
    #if ENABLED(Y_IS_TMC2130)
      if (axis == Y_AXIS) tmc2130_sensorless_homing(stepperY);
    #endif
  #endif

  // Fast move towards endstop until triggered
  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Home 1 Fast:");
  #endif
  do_homing_move(axis, 1.5 * max_length(axis) * axis_home_dir);

  // When homing Z with probe respect probe clearance
  const float bump = axis_home_dir * (
    #if HOMING_Z_WITH_PROBE
      (axis == Z_AXIS) ? max(Z_CLEARANCE_BETWEEN_PROBES, home_bump_mm(Z_AXIS)) :
    #endif
    home_bump_mm(axis)
  );

  // If a second homing move is configured...
  if (bump) {
    // Move away from the endstop by the axis HOME_BUMP_MM
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Move Away:");
    #endif
    do_homing_move(axis, -bump);

    // Slow move towards endstop until triggered
    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("Home 2 Slow:");
    #endif
    do_homing_move(axis, 2 * bump, get_homing_bump_feedrate(axis));
  }

  #if ENABLED(Z_DUAL_ENDSTOPS)
    if (axis == Z_AXIS) {
      float adj = FABS(z_endstop_adj);
      bool lockZ1;
      if (axis_home_dir > 0) {
        adj = -adj;
        lockZ1 = (z_endstop_adj > 0);
      }
      else
        lockZ1 = (z_endstop_adj < 0);

      if (lockZ1) stepper.set_z_lock(true); else stepper.set_z2_lock(true);

      // Move to the adjusted endstop height
      do_homing_move(axis, adj);

      if (lockZ1) stepper.set_z_lock(false); else stepper.set_z2_lock(false);
      stepper.set_homing_flag(false);
    } // Z_AXIS
  #endif

  #if IS_SCARA

    set_axis_is_at_home(axis);
    SYNC_PLAN_POSITION_KINEMATIC();

  #elif ENABLED(DELTA)

    // Delta has already moved all three towers up in G28
    // so here it re-homes each tower in turn.
    // Delta homing treats the axes as normal linear axes.

    // retrace by the amount specified in endstop_adj + additional 0.1mm in order to have minimum steps
    if (endstop_adj[axis] * Z_HOME_DIR <= 0) {
      #if ENABLED(DEBUG_LEVELING_FEATURE)
        if (DEBUGGING(LEVELING)) SERIAL_ECHOLNPGM("endstop_adj:");
      #endif
      do_homing_move(axis, endstop_adj[axis] - 0.1);
    }

  #else

    // For cartesian/core machines,
    // set the axis to its home position
    set_axis_is_at_home(axis);
    sync_plan_position();

    destination[axis] = current_position[axis];

    #if ENABLED(DEBUG_LEVELING_FEATURE)
      if (DEBUGGING(LEVELING)) DEBUG_POS("> AFTER set_axis_is_at_home", current_position);
    #endif

  #endif

  // Re-enable stealthChop if used. Disable diag1 pin on driver.
  #if ENABLED(SENSORLESS_HOMING)
    #if ENABLED(X_IS_TMC2130)
      if (axis == X_AXIS) tmc2130_sensorless_homing(stepperX, false);
    #endif
    #if ENABLED(Y_IS_TMC2130)
      if (axis == Y_AXIS) tmc2130_sensorless_homing(stepperY, false);
    #endif
  #endif

  // Put away the Z probe
  #if HOMING_Z_WITH_PROBE
    if (axis == Z_AXIS && STOW_PROBE()) return;
  #endif

  #if ENABLED(DEBUG_LEVELING_FEATURE)
    if (DEBUGGING(LEVELING)) {
      SERIAL_ECHOPAIR("<<< homeaxis(", axis_codes[axis]);
      SERIAL_CHAR(')');
      SERIAL_EOL();
    }
  #endif
} // homeaxis()
