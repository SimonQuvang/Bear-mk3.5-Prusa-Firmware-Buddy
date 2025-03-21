/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2019 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
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

/**************
 * ui_api.cpp *
 **************/

/****************************************************************************
 *   Written By Marcio Teixeira 2018 - Aleph Objects, Inc.                  *
 *                                                                          *
 *   This program is free software: you can redistribute it and/or modify   *
 *   it under the terms of the GNU General Public License as published by   *
 *   the Free Software Foundation, either version 3 of the License, or      *
 *   (at your option) any later version.                                    *
 *                                                                          *
 *   This program is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU General Public License for more details.                           *
 *                                                                          *
 *   To view a copy of the GNU General Public License, go to the following  *
 *   location: <http://www.gnu.org/licenses/>.                              *
 ****************************************************************************/

#include "../../inc/MarlinConfigPre.h"

#if ENABLED(EXTENSIBLE_UI)

#include "../ultralcd.h"
#include "../../gcode/queue.h"
#include "../../module/motion.h"
#include "../../module/planner.h"
#include "../../module/probe.h"
#include "../../module/temperature.h"
#include "../../module/printcounter.h"
#include "../../libs/duration_t.h"
#include "../../HAL/shared/Delay.h"

#if ENABLED(PRINTCOUNTER)
  #include "../../core/utility.h"
  #include "../../libs/numtostr.h"
#endif

#if EXTRUDERS > 1
  #include "../../module/tool_change.h"
#endif

#if ENABLED(EMERGENCY_PARSER)
  #include "../../feature/emergency_parser.h"
#endif

#if ENABLED(SDSUPPORT)
  #include "../../sd/cardreader.h"
  #define IFSD(A,B) (A)
#else
  #define IFSD(A,B) (B)
#endif

#if HAS_TRINAMIC
  #include "../../feature/tmc_util.h"
  #include "../../module/stepper/indirection.h"
#endif

#include "ui_api.h"

#if ENABLED(BACKLASH_GCODE)
  #include "../../feature/backlash.h"
#endif

#if HAS_LEVELING
  #include "../../feature/bedlevel/bedlevel.h"
#endif

#if ENABLED(CRASH_RECOVERY)
  #include "../../feature/prusa/crash_recovery.hpp"
#endif

#if HAS_FILAMENT_SENSOR
  #include "../../feature/runout.h"
#endif

#if ENABLED(BABYSTEPPING)
  #include "../../feature/babystep.h"
#endif

#if ENABLED(HOST_PROMPT_SUPPORT)
  #include "../../feature/host_actions.h"
#endif

namespace ExtUI {
  static struct {
    uint8_t printer_killed : 1;
    #if ENABLED(JOYSTICK)
      uint8_t jogging : 1;
    #endif
  } flags;

  #ifdef __SAM3X8E__
    /**
     * Implement a special millis() to allow time measurement
     * within an ISR (such as when the printer is killed).
     *
     * To keep proper time, must be called at least every 1s.
     */
    uint32_t safe_millis() {
      // Not killed? Just call millis()
      if (!flags.printer_killed) return millis();

      static uint32_t currTimeHI = 0; /* Current time */

      // Machine was killed, reinit SysTick so we are able to compute time without ISRs
      if (currTimeHI == 0) {
        // Get the last time the Arduino time computed (from CMSIS) and convert it to SysTick
        currTimeHI = (uint32_t)((GetTickCount() * (uint64_t)(F_CPU / 8000)) >> 24);

        // Reinit the SysTick timer to maximize its period
        SysTick->LOAD  = SysTick_LOAD_RELOAD_Msk;                    // get the full range for the systick timer
        SysTick->VAL   = 0;                                          // Load the SysTick Counter Value
        SysTick->CTRL  = // MCLK/8 as source
                         // No interrupts
                         SysTick_CTRL_ENABLE_Msk;                    // Enable SysTick Timer
     }

      // Check if there was a timer overflow from the last read
      if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
        // There was. This means (SysTick_LOAD_RELOAD_Msk * 1000 * 8)/F_CPU ms has elapsed
        currTimeHI++;
      }

      // Calculate current time in milliseconds
      uint32_t currTimeLO = SysTick_LOAD_RELOAD_Msk - SysTick->VAL; // (in MCLK/8)
      uint64_t currTime = ((uint64_t)currTimeLO) | (((uint64_t)currTimeHI) << 24);

      // The ms count is
      return (uint32_t)(currTime / (F_CPU / 8000));
    }
  #endif // __SAM3X8E__

  void delay_us(unsigned long us) { DELAY_US(us); }

  void delay_ms(unsigned long ms) {
    if (flags.printer_killed)
      DELAY_US(ms * 1000);
    else
      safe_delay(ms);
  }

  void yield() {
    if (!flags.printer_killed)
      thermalManager.manage_heater();
  }

  void enableHeater(const extruder_t extruder) {
    #if HOTENDS && HEATER_IDLE_HANDLER
      thermalManager.reset_heater_idle_timer(extruder - E0);
    #else
      UNUSED(extruder);
    #endif
  }

  void enableHeater(const heater_t heater) {
    #if HEATER_IDLE_HANDLER
      switch (heater) {
        #if HAS_HEATED_BED
          case BED:
            thermalManager.reset_bed_idle_timer();
            return;
        #endif
        #if HAS_HEATED_CHAMBER
          case CHAMBER: return; // Chamber has no idle timer
        #endif
        default:
          #if HOTENDS
            thermalManager.reset_heater_idle_timer(heater - H0);
          #endif
          break;
      }
    #else
      UNUSED(heater);
    #endif
  }

  #if ENABLED(JOYSTICK)
    /**
     * Jogs in the direction given by the vector (dx, dy, dz).
     * The values range from -1 to 1 mapping to the maximum
     * feedrate for an axis.
     *
     * The axis will continue to jog until this function is
     * called with all zeros.
     */
    void jog(const xyz_float_t &dir) {
      // The "destination" variable is used as a scratchpad in
      // Marlin by GCODE routines, but should remain untouched
      // during manual jogging, allowing us to reuse the space
      // for our direction vector.
      destination = dir;
      flags.jogging = !NEAR_ZERO(dir.x) || !NEAR_ZERO(dir.y) || !NEAR_ZERO(dir.z);
    }

    // Called by the polling routine in "joystick.cpp"
    void _joystick_update(xyz_float_t &norm_jog) {
      if (flags.jogging) {
        #define OUT_OF_RANGE(VALUE) (VALUE < -1.0f || VALUE > 1.0f)

        if (OUT_OF_RANGE(destination.x) || OUT_OF_RANGE(destination.y) || OUT_OF_RANGE(destination.z)) {
          // If destination on any axis is out of range, it
          // probably means the UI forgot to stop jogging and
          // ran GCODE that wrote a position to destination.
          // To prevent a disaster, stop jogging.
          flags.jogging = false;
          return;
        }
        norm_jog = destination;
      }
    }
  #endif

  bool isHeaterIdle(const extruder_t extruder) {
    return false
      #if HOTENDS && HEATER_IDLE_HANDLER
        || thermalManager.hotend_idle[extruder - E0].timed_out
      #else
        ; UNUSED(extruder)
      #endif
    ;
  }

  bool isHeaterIdle(const heater_t heater) {
    #if HEATER_IDLE_HANDLER
      switch (heater) {
        #if HAS_HEATED_BED
          case BED: return thermalManager.bed_idle.timed_out;
        #endif
        #if HAS_HEATED_CHAMBER
          case CHAMBER: return false; // Chamber has no idle timer
        #endif
        default:
          #if HOTENDS
            return thermalManager.hotend_idle[heater - H0].timed_out;
          #else
            return false;
          #endif
      }
    #else
      UNUSED(heater);
      return false;
    #endif
  }

  float getActualTemp_celsius(const heater_t heater) {
    switch (heater) {
      #if HAS_HEATED_BED
        case BED: return thermalManager.degBed();
      #endif
      #if HAS_HEATED_CHAMBER
        case CHAMBER: return thermalManager.degChamber();
      #endif
      default: return thermalManager.degHotend(heater - H0);
    }
  }

  float getActualTemp_celsius(const extruder_t extruder) {
    return thermalManager.degHotend(extruder - E0);
  }

  float getTargetTemp_celsius(const heater_t heater) {
    switch (heater) {
      #if HAS_HEATED_BED
        case BED: return thermalManager.degTargetBed();
      #endif
      #if HAS_HEATED_CHAMBER
        case CHAMBER: return thermalManager.degTargetChamber();
      #endif
      default: return thermalManager.degTargetHotend(heater - H0);
    }
  }

  float getTargetTemp_celsius(const extruder_t extruder) {
    return thermalManager.degTargetHotend(extruder - E0);
  }

  float getTargetFan_percent(const fan_t fan) {
    #if FAN_COUNT > 0
      return thermalManager.fanPercent(thermalManager.fan_speed[fan - FAN0]);
    #else
      UNUSED(fan);
      return 0;
    #endif
  }

  float getActualFan_percent(const fan_t fan) {
    #if FAN_COUNT > 0
      return thermalManager.fanPercent(thermalManager.scaledFanSpeed(fan - FAN0));
    #else
      UNUSED(fan);
      return 0;
    #endif
  }

  float getAxisPosition_mm(const axis_t axis) {
    return
      #if ENABLED(JOYSTICK)
        flags.jogging ? destination[axis] :
      #endif
      current_position[axis];
  }

  float getAxisPosition_mm(const extruder_t extruder) {
    const extruder_t old_tool = getActiveTool();
    setActiveTool(extruder);
    const float epos = (
      #if ENABLED(JOYSTICK)
        flags.jogging ? destination.e :
      #endif
      current_position.e
    );
    setActiveTool(old_tool);
    return epos;
  }

  void setAxisPosition_mm(const float position, const axis_t axis) {
    // Start with no limits to movement
    float min = current_position[axis] - 1000,
          max = current_position[axis] + 1000;

    // Limit to software endstops, if enabled
    #if HAS_SOFTWARE_ENDSTOPS
      if (soft_endstops_enabled) switch (axis) {
        case X_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_X)
            min = soft_endstop.min.x;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_X)
            max = soft_endstop.max.x;
          #endif
          break;
        case Y_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_Y)
            min = soft_endstop.min.y;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_Y)
            max = soft_endstop.max.y;
          #endif
          break;
        case Z_AXIS:
          #if ENABLED(MIN_SOFTWARE_ENDSTOP_Z)
            min = soft_endstop.min.z;
          #endif
          #if ENABLED(MAX_SOFTWARE_ENDSTOP_Z)
            max = soft_endstop.max.z;
          #endif
        default: break;
      }
    #endif // HAS_SOFTWARE_ENDSTOPS

    // Delta limits XY based on the current offset from center
    // This assumes the center is 0,0
    #if ENABLED(DELTA)
      if (axis != Z_AXIS) {
        max = SQRT(sq((float)(DELTA_PRINTABLE_RADIUS)) - sq(current_position[Y_AXIS - axis])); // (Y_AXIS - axis) == the other axis
        min = -max;
      }
    #endif

    current_position[axis] = constrain(position, min, max);
    line_to_current_position(MMM_TO_MMS(manual_feedrate_mm_m[axis]));
  }

  void setAxisPosition_mm(const float position, const extruder_t extruder) {
    setActiveTool(extruder);

    current_position.e = position;
    line_to_current_position(MMM_TO_MMS(manual_feedrate_mm_m.e));
  }

  void setActiveTool(const extruder_t extruder) {
    #if EXTRUDERS > 1
      const uint8_t e = extruder - E0;
      if (e != active_extruder) tool_change(e, tool_return_t::no_return);
      active_extruder = e;
    #else
      UNUSED(extruder);
    #endif
  }

  extruder_t getActiveTool() {
    switch (active_extruder) {
      case 5:  return E5;
      case 4:  return E4;
      case 3:  return E3;
      case 2:  return E2;
      case 1:  return E1;
      default: return E0;
    }
  }

  bool isMoving() { return planner.has_blocks_queued(); }

  bool canMove(const axis_t axis) {
    switch (axis) {
      #if IS_KINEMATIC || ENABLED(NO_MOTION_BEFORE_HOMING)
        case X: return TEST(axis_homed, X_AXIS);
        case Y: return TEST(axis_homed, Y_AXIS);
        case Z: return TEST(axis_homed, Z_AXIS);
      #else
        case X: case Y: case Z: return true;
      #endif
      default: return false;
    }
  }

  bool canMove(const extruder_t extruder) {
    return !thermalManager.tooColdToExtrude(extruder - E0);
  }

  #if HAS_SOFTWARE_ENDSTOPS
    bool getSoftEndstopState() { return soft_endstops_enabled; }
    void setSoftEndstopState(const bool value) { soft_endstops_enabled = value; }
  #endif

  #if HAS_TRINAMIC
    float getAxisCurrent_mA(const axis_t axis) {
      switch (axis) {
        #if AXIS_IS_TMC(X)
          case X: return stepperX.getMilliamps();
        #endif
        #if AXIS_IS_TMC(Y)
          case Y: return stepperY.getMilliamps();
        #endif
        #if AXIS_IS_TMC(Z)
          case Z: return stepperZ.getMilliamps();
        #endif
        default: return NAN;
      };
    }

    float getAxisCurrent_mA(const extruder_t extruder) {
      switch (extruder) {
        #if AXIS_IS_TMC(E0)
          case E0: return stepperE0.getMilliamps();
        #endif
        #if AXIS_IS_TMC(E1)
          case E1: return stepperE1.getMilliamps();
        #endif
        #if AXIS_IS_TMC(E2)
          case E2: return stepperE2.getMilliamps();
        #endif
        #if AXIS_IS_TMC(E3)
          case E3: return stepperE3.getMilliamps();
        #endif
        #if AXIS_IS_TMC(E4)
          case E4: return stepperE4.getMilliamps();
        #endif
        #if AXIS_IS_TMC(E5)
          case E5: return stepperE5.getMilliamps();
        #endif
        default: return NAN;
      };
    }

    void  setAxisCurrent_mA(const float mA, const axis_t axis) {
      switch (axis) {
        #if AXIS_IS_TMC(X)
          case X: stepperX.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(Y)
          case Y: stepperY.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(Z)
          case Z: stepperZ.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        default: break;
      };
    }

    void  setAxisCurrent_mA(const float mA, const extruder_t extruder) {
      switch (extruder) {
        #if AXIS_IS_TMC(E0)
          case E0: stepperE0.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(E1)
          case E1: stepperE1.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(E2)
          case E2: stepperE2.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(E3)
          case E3: stepperE3.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(E4)
          case E4: stepperE4.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        #if AXIS_IS_TMC(E5)
          case E5: stepperE5.rms_current(constrain(mA, 500, 1500)); break;
        #endif
        default: break;
      };
    }

    int getTMCBumpSensitivity(const axis_t axis) {
      switch (axis) {
        #if X_SENSORLESS
          case X: return stepperX.stall_sensitivity();
        #endif
        #if Y_SENSORLESS
          case Y: return stepperY.stall_sensitivity();
        #endif
        #if Z_SENSORLESS
          case Z: return stepperZ.stall_sensitivity();
        #endif
        default: return 0;
      }
    }

    void setTMCBumpSensitivity(const float value, const axis_t axis) {
      switch (axis) {
        #if X_SENSORLESS || Y_SENSORLESS || Z_SENSORLESS
          #if X_SENSORLESS
            #if ENABLED(CRASH_RECOVERY)
              case X: crash_s.home_sensitivity[0] = value; break;
            #else
              case X: stepperX.stall_sensitivity(value); break;
            #endif
          #endif
          #if Y_SENSORLESS
            #if ENABLED(CRASH_RECOVERY)
              case Y: crash_s.home_sensitivity[1] = value; break;
            #else
              case Y: stepperY.stall_sensitivity(value); break;
            #endif
          #endif
          #if Z_SENSORLESS
            #if ENABLED(CRASH_RECOVERY)
              case Z: crash_s.home_sensitivity[2] = value; break;
            #else
              case Z: stepperZ.stall_sensitivity(value); break;
            #endif
          #endif
        #else
          UNUSED(value);
        #endif
        default: break;
      }
    }
  #endif

  float getAxisSteps_per_mm(const axis_t axis) {
    return planner.settings.axis_steps_per_mm[axis];
  }

  float getAxisSteps_per_mm(const extruder_t extruder) {
    UNUSED_E(extruder);
    return planner.settings.axis_steps_per_mm[E_AXIS_N(extruder - E0)];
  }

  void setAxisSteps_per_mm(const float value, const axis_t axis) {
    auto s = planner.user_settings;
    s.axis_steps_per_mm[axis] = value;
    s.axis_msteps_per_mm[axis] = value * PLANNER_STEPS_MULTIPLIER;
    planner.apply_settings(s);
  }

  void setAxisSteps_per_mm(const float value, const extruder_t extruder) {
    UNUSED_E(extruder);
    auto s = planner.user_settings;
    s.axis_steps_per_mm[E_AXIS_N(axis - E0)] = value;
    s.axis_msteps_per_mm[E_AXIS_N(axis - E0)] = value * PLANNER_STEPS_MULTIPLIER;
    planner.apply_settings(s);
  }

  feedRate_t getAxisMaxFeedrate_mm_s(const axis_t axis) {
    return planner.settings.max_feedrate_mm_s[axis];
  }

  feedRate_t getAxisMaxFeedrate_mm_s(const extruder_t extruder) {
    UNUSED_E(extruder);
    return planner.settings.max_feedrate_mm_s[E_AXIS_N(axis - E0)];
  }

  void setAxisMaxFeedrate_mm_s(const feedRate_t value, const axis_t axis) {
    planner.set_max_feedrate(axis, value);
  }

  void setAxisMaxFeedrate_mm_s(const feedRate_t value, const extruder_t extruder) {
    planner.set_max_feedrate(E_AXIS_N(extruder - E0), value);
  }

  float getAxisMaxAcceleration_mm_s2(const axis_t axis) {
    return planner.settings.max_acceleration_mm_per_s2[axis];
  }

  float getAxisMaxAcceleration_mm_s2(const extruder_t extruder) {
    UNUSED_E(extruder);
    return planner.settings.max_acceleration_mm_per_s2[E_AXIS_N(extruder - E0)];
  }

  void setAxisMaxAcceleration_mm_s2(const float value, const axis_t axis) {
    planner.set_max_acceleration(axis, value);
  }

  void setAxisMaxAcceleration_mm_s2(const float value, const extruder_t extruder) {
    planner.set_max_acceleration(E_AXIS_N(extruder - E0), value);
  }

  #if HAS_FILAMENT_SENSOR
    bool getFilamentRunoutEnabled()                 { return runout.enabled; }
    void setFilamentRunoutEnabled(const bool value) { runout.enabled = value; }

    #ifdef FILAMENT_RUNOUT_DISTANCE_MM
      float getFilamentRunoutDistance_mm()                 { return runout.runout_distance(); }
      void setFilamentRunoutDistance_mm(const float value) { runout.set_runout_distance(constrain(value, 0, 999)); }
    #endif
  #endif

  #if ENABLED(LIN_ADVANCE)
    float getLinearAdvance_mm_mm_s(const extruder_t extruder) {
      return (extruder < EXTRUDERS) ? planner.extruder_advance_K[extruder - E0] : 0;
    }

    void setLinearAdvance_mm_mm_s(const float value, const extruder_t extruder) {
      if (extruder < EXTRUDERS)
        planner.extruder_advance_K[extruder - E0] = constrain(value, 0, 999);
    }
  #endif

  #if DISABLED(CLASSIC_JERK)

    float getJunctionDeviation_mm() {
      return planner.junction_deviation_mm;
    }

    void setJunctionDeviation_mm(const float value) {
      planner.junction_deviation_mm = constrain(value, 0.01, 0.3);
      #if ENABLED(LIN_ADVANCE)
        planner.recalculate_max_e_jerk();
      #endif
    }

  #else

    float getAxisMaxJerk_mm_s(const axis_t axis) {
      return planner.settings.max_jerk[axis];
    }

    float getAxisMaxJerk_mm_s(const extruder_t) {
      return planner.settings.max_jerk.e;
    }

    void setAxisMaxJerk_mm_s(const float value, const axis_t axis) {
      planner.set_max_jerk((AxisEnum)axis, value);
    }

    void setAxisMaxJerk_mm_s(const float value, const extruder_t) {
      planner.set_max_jerk(E_AXIS, value);
    }
  #endif

  feedRate_t getFeedrate_mm_s()                       { return feedrate_mm_s; }
  feedRate_t getMinFeedrate_mm_s()                    { return planner.settings.min_feedrate_mm_s; }
  feedRate_t getMinTravelFeedrate_mm_s()              { return planner.settings.min_travel_feedrate_mm_s; }
  float getPrintingAcceleration_mm_s2()               { return planner.settings.acceleration; }
  float getRetractAcceleration_mm_s2()                { return planner.settings.retract_acceleration; }
  float getTravelAcceleration_mm_s2()                 { return planner.settings.travel_acceleration; }
  void setFeedrate_mm_s(const feedRate_t fr)          { feedrate_mm_s = fr; }
  void setMinFeedrate_mm_s(const feedRate_t fr)       {
    auto s = planner.user_settings;
    s.min_feedrate_mm_s = fr;
    planner.apply_settings(s);
  }
  void setMinTravelFeedrate_mm_s(const feedRate_t fr) {
    auto s = planner.user_settings;
    s.min_travel_feedrate_mm_s = fr;
    planner.apply_settings(s);
  }
  void setPrintingAcceleration_mm_s2(const float acc) {
    auto s = planner.user_settings;
    s.acceleration = acc;
    planner.apply_settings(s);
  }
  void setRetractAcceleration_mm_s2(const float acc)  {
    auto s = planner.user_settings;
    s.retract_acceleration = acc;
    planner.apply_settings(s);
  }
  void setTravelAcceleration_mm_s2(const float acc)   {
    auto s = planner.user_settings;
    s.travel_acceleration = acc;
    planner.apply_settings(s);
  }

  #if ENABLED(BABYSTEPPING)
    bool babystepAxis_steps(const int16_t steps, const axis_t axis) {
      switch (axis) {
        #if ENABLED(BABYSTEP_XY)
          case X: babystep.add_steps(X_AXIS, steps); break;
          case Y: babystep.add_steps(Y_AXIS, steps); break;
        #endif
        case Z: babystep.add_steps(Z_AXIS, steps); break;
        default: return false;
      };
      return true;
    }

    /**
     * This function adjusts an axis during a print.
     *
     * When linked_nozzles is false, each nozzle in a multi-nozzle
     * printer can be babystepped independently of the others. This
     * lets the user to fine tune the Z-offset and Nozzle Offsets
     * while observing the first layer of a print, regardless of
     * what nozzle is printing.
     */
    void smartAdjustAxis_steps(const int16_t steps, const axis_t axis, bool linked_nozzles) {
      const float mm = steps * planner.mm_per_step[axis];

      if (!babystepAxis_steps(steps, axis)) return;

      #if ENABLED(BABYSTEP_ZPROBE_OFFSET)
        // Make it so babystepping in Z adjusts the Z probe offset.
        if (axis == Z
          #if EXTRUDERS > 1
            && (linked_nozzles || active_extruder == 0)
          #endif
        ) probe_offset.z += mm;
      #else
        UNUSED(mm);
      #endif

      #if EXTRUDERS > 1 && HAS_HOTEND_OFFSET
        /**
         * When linked_nozzles is false, as an axis is babystepped
         * adjust the hotend offsets so that the other nozzles are
         * unaffected by the babystepping of the active nozzle.
         */
        if (!linked_nozzles) {
          HOTEND_LOOP()
            if (e != active_extruder)
              hotend_offset[e][axis] += mm;

          normalizeNozzleOffset(X);
          normalizeNozzleOffset(Y);
          normalizeNozzleOffset(Z);
        }
      #else
        UNUSED(linked_nozzles);
        UNUSED(mm);
      #endif
    }

    /**
     * Converts a mm displacement to a number of whole number of
     * steps that is at least mm long.
     */
    int16_t mmToWholeSteps(const float mm, const axis_t axis) {
      const float steps = mm / planner.mm_per_step[axis];
      return steps > 0 ? ceil(steps) : floor(steps);
    }
  #endif

  float getZOffset_mm() {
    #if HAS_BED_PROBE
      return probe_offset.z;
    #elif ENABLED(BABYSTEP_DISPLAY_TOTAL)
      return babystep.axis_total[BS_TOTAL_AXIS(Z_AXIS) + 1];
    #else
      return 0.0;
    #endif
  }

  void setZOffset_mm(const float value) {
    #if HAS_BED_PROBE
      if (WITHIN(value, Z_PROBE_OFFSET_RANGE_MIN, Z_PROBE_OFFSET_RANGE_MAX))
        probe_offset.z = value;
    #elif ENABLED(BABYSTEP_DISPLAY_TOTAL)
      babystep.add_mm(Z_AXIS, (value - babystep.axis_total[BS_TOTAL_AXIS(Z_AXIS) + 1]));
    #else
      UNUSED(value);
    #endif
  }

  #if HAS_HOTEND_OFFSET

    float getNozzleOffset_mm(const axis_t axis, const extruder_t extruder) {
      if (extruder - E0 >= HOTENDS) return 0;
      return hotend_offset[extruder - E0][axis];
    }

    void setNozzleOffset_mm(const float value, const axis_t axis, const extruder_t extruder) {
      if (extruder - E0 >= HOTENDS) return;
      hotend_offset[extruder - E0][axis] = value;
    }

    /**
     * The UI should call this if needs to guarantee the first
     * nozzle offset is zero (such as when it doesn't allow the
     * user to edit the offset the first nozzle).
     */
    void normalizeNozzleOffset(const axis_t axis) {
      const float offs = hotend_offset[0][axis];
      HOTEND_LOOP() hotend_offset[e][axis] -= offs;
    }

  #endif // HAS_HOTEND_OFFSET

  #if ENABLED(BACKLASH_GCODE)
    float getAxisBacklash_mm(const axis_t axis)       { return backlash.distance_mm[axis]; }
    void setAxisBacklash_mm(const float value, const axis_t axis)
                                                      { backlash.distance_mm[axis] = constrain(value,0,5); }

    float getBacklashCorrection_percent()             { return ui8_to_percent(backlash.correction); }
    void setBacklashCorrection_percent(const float value) { backlash.correction = map(constrain(value, 0, 100), 0, 100, 0, 255); }

    #ifdef BACKLASH_SMOOTHING_MM
      float getBacklashSmoothing_mm()                 { return backlash.smoothing_mm; }
      void setBacklashSmoothing_mm(const float value) { backlash.smoothing_mm = constrain(value, 0, 999); }
    #endif
  #endif

  uint8_t getProgress_percent() {
    return ui.get_progress_percent();
  }

  uint32_t getProgress_seconds_elapsed() {
    const duration_t elapsed = print_job_timer.duration();
    return elapsed.value;
  }

  #if HAS_LEVELING
    bool getLevelingActive() { return planner.leveling_active; }
    void setLevelingActive(const bool state) { set_bed_leveling_enabled(state); }
    bool getMeshValid() { return leveling_is_valid(); }
    #if HAS_MESH
      bed_mesh_t& getMeshArray() { return Z_VALUES_ARR; }
      float getMeshPoint(const xy_uint8_t &pos) { return Z_VALUES(pos.x, pos.y); }
      void setMeshPoint(const xy_uint8_t &pos, const float zoff) {
        if (WITHIN(pos.x, 0, GRID_MAX_POINTS_X) && WITHIN(pos.y, 0, GRID_MAX_POINTS_Y)) {
          Z_VALUES(pos.x, pos.y) = zoff;
          #if ENABLED(ABL_BILINEAR_SUBDIVISION)
            bed_level_virt_interpolate();
          #endif
        }
      }
    #endif
  #endif

  #if ENABLED(HOST_PROMPT_SUPPORT)
    void setHostResponse(const uint8_t response) { host_response_handler(response); }
  #endif

  #if ENABLED(PRINTCOUNTER)
    char* getTotalPrints_str(char buffer[21])    { strcpy(buffer,i16tostr3left(print_job_timer.getStats().totalPrints));    return buffer; }
    char* getFinishedPrints_str(char buffer[21]) { strcpy(buffer,i16tostr3left(print_job_timer.getStats().finishedPrints)); return buffer; }
    char* getTotalPrintTime_str(char buffer[21]) { return duration_t(print_job_timer.getStats().printTime).toString(buffer); }
    char* getLongestPrint_str(char buffer[21])   { return duration_t(print_job_timer.getStats().longestPrint).toString(buffer); }
    char* getFilamentUsed_str(char buffer[21])   {
      printStatistics stats = print_job_timer.getStats();
      sprintf_P(buffer, PSTR("%ld.%im"), long(stats.filamentUsed / 1000), int16_t(stats.filamentUsed / 100) % 10);
      return buffer;
    }
  #endif

  float getFeedrate_percent() { return feedrate_percentage; }

  void injectCommands_P(ConstexprString gcode) {
    queue.inject_P(gcode);
  }

  bool commandsInQueue() { return (queue.has_commands_queued() || planner.processing()); }

  bool isAxisPositionKnown(const axis_t axis) {
    return TEST(axis_known_position, axis);
  }

  bool isAxisPositionKnown(const extruder_t) {
    return TEST(axis_known_position, E_AXIS);
  }

  bool isPositionKnown() { return all_axes_known(); }
  bool isMachineHomed() { return all_axes_homed(); }

  PGM_P getFirmwareName_str() {
    static const char firmware_name[] PROGMEM = "Marlin " SHORT_BUILD_VERSION;
    return firmware_name;
  }

  void setTargetTemp_celsius(float value, const heater_t heater) {
    enableHeater(heater);
    #if HAS_HEATED_BED
      if (heater == BED)
        thermalManager.setTargetBed(constrain(value, 0, BED_MAXTEMP - BED_MAXTEMP_SAFETY_MARGIN));
      else
    #endif
      {
        #if HOTENDS
          static constexpr int16_t heater_maxtemp[HOTENDS] = ARRAY_BY_HOTENDS(HEATER_0_MAXTEMP, HEATER_1_MAXTEMP, HEATER_2_MAXTEMP, HEATER_3_MAXTEMP, HEATER_4_MAXTEMP, HEATER_5_MAXTEMP);
          const int16_t e = heater - H0;
          thermalManager.setTargetHotend(constrain(value, 0, heater_maxtemp[e] - HEATER_MAXTEMP_SAFETY_MARGIN), e);
        #endif
      }
  }

  void setTargetTemp_celsius(float value, const extruder_t extruder) {
    #if HOTENDS
      constexpr int16_t heater_maxtemp[HOTENDS] = ARRAY_BY_HOTENDS(HEATER_0_MAXTEMP, HEATER_1_MAXTEMP, HEATER_2_MAXTEMP, HEATER_3_MAXTEMP, HEATER_4_MAXTEMP, HEATER_5_MAXTEMP);
      const int16_t e = extruder - E0;
      enableHeater(extruder);
      thermalManager.setTargetHotend(constrain(value, 0, heater_maxtemp[e] - HEATER_MAXTEMP_SAFETY_MARGIN), e);
    #endif
  }

  void setTargetFan_percent(const float value, const fan_t fan) {
    #if FAN_COUNT > 0
      if (fan < FAN_COUNT)
        thermalManager.set_fan_speed(fan - FAN0, map(constrain(value, 0, 100), 0, 100, 0, 255));
    #else
      UNUSED(value);
      UNUSED(fan);
    #endif
  }

  void setFeedrate_percent(const float value) {
    feedrate_percentage = constrain(value, 10, 500);
  }

  void setUserConfirmed() {
    #if HAS_RESUME_CONTINUE
      wait_for_user = false;
    #endif
  }

  void printFile(const char *filename) {
    IFSD(card.openAndPrintFile(filename), NOOP);
  }

  bool isPrintingFromMediaPaused() {
    return IFSD(isPrintingFromMedia() && !IS_SD_PRINTING(), false);
  }

  bool isPrintingFromMedia() {
    return IFSD(card.isFileOpen(), false);
  }

  bool isPrinting() {
    return (planner.processing() || isPrintingFromMedia() || IFSD(IS_SD_PRINTING(), false));
  }

  bool isMediaInserted() {
    return IFSD(IS_SD_INSERTED() && card.isMounted(), false);
  }

  void pausePrint() {
    ui.pause_print();
  }

  void resumePrint() {
    ui.resume_print();
  }

  void stopPrint() {
    ui.abort_print();
  }

  void onUserConfirmRequired_P(PGM_P const pstr) {
    char msg[strlen_P(pstr) + 1];
    strcpy_P(msg, pstr);
    onUserConfirmRequired(msg);
  }

  FileList::FileList() { refresh(); }

  void FileList::refresh() { num_files = 0xFFFF; }

  bool FileList::seek(const uint16_t pos, const bool skip_range_check) {
    #if ENABLED(SDSUPPORT)
      if (!skip_range_check && (pos + 1) > count()) return false;
      const uint16_t nr =
        #if ENABLED(SDCARD_RATHERRECENTFIRST) && DISABLED(SDCARD_SORT_ALPHA)
          count() - 1 -
        #endif
      pos;

      card.getfilename_sorted(nr);
      return card.filename[0] != '\0';
    #else
      return false;
    #endif
  }

  const char* FileList::filename() {
    return IFSD(card.longFilename[0] ? card.longFilename : card.filename, "");
  }

  const char* FileList::shortFilename() {
    return IFSD(card.filename, "");
  }

  const char* FileList::longFilename() {
    return IFSD(card.longFilename, "");
  }

  bool FileList::isDir() {
    return IFSD(card.flag.filenameIsDir, false);
  }

  uint16_t FileList::count() {
    return IFSD((num_files = (num_files == 0xFFFF ? card.get_num_Files() : num_files)), 0);
  }

  bool FileList::isAtRootDir() {
    return (true
      #if ENABLED(SDSUPPORT)
        && card.flag.workDirIsRoot
      #endif
    );
  }

  void FileList::upDir() {
    #if ENABLED(SDSUPPORT)
      card.cdup();
      num_files = 0xFFFF;
    #endif
  }

  void FileList::changeDir(const char * const dirname) {
    #if ENABLED(SDSUPPORT)
      card.cd(dirname);
      num_files = 0xFFFF;
    #endif
  }

} // namespace ExtUI

// At the moment, we piggy-back off the ultralcd calls, but this could be cleaned up in the future

void MarlinUI::init() {
  #if ENABLED(SDSUPPORT) && PIN_EXISTS(SD_DETECT)
    SET_INPUT_PULLUP(SD_DETECT_PIN);
  #endif

  ExtUI::onStartup();
}

void MarlinUI::update() {
  #if ENABLED(SDSUPPORT)
    static bool last_sd_status;
    const bool sd_status = IS_SD_INSERTED();
    if (sd_status != last_sd_status) {
      last_sd_status = sd_status;
      if (sd_status) {
        card.mount();
        if (card.isMounted())
          ExtUI::onMediaInserted();
        else
          ExtUI::onMediaError();
      }
      else {
        const bool ok = card.isMounted();
        card.release();
        if (ok) ExtUI::onMediaRemoved();
      }
    }
  #endif // SDSUPPORT
  ExtUI::onIdle();
}

void MarlinUI::kill_screen(PGM_P const error, PGM_P const component) {
  using namespace ExtUI;
  if (!flags.printer_killed) {
    flags.printer_killed = true;
    onPrinterKilled(error, component);
  }
}

#endif // EXTENSIBLE_UI
