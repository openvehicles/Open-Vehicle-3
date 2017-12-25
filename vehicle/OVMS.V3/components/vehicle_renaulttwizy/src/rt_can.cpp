/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy: low level CAN communication
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ovms_log.h"
// static const char *TAG = "v-renaulttwizy";

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"

using namespace std;


/**
 * CAN RX handler
 */

void OvmsVehicleRenaultTwizy::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  unsigned int u;
  
  uint8_t *can_databuffer = p_frame->data.u8;
  
  // CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
  #define CAN_BYTE(b)     can_databuffer[b]
  #define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
  #define CAN_UINT24(b)   (((UINT32)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) \
                            | CAN_BYTE(b+2))
  #define CAN_UINT32(b)   (((UINT32)CAN_BYTE(b) << 24) | ((UINT32)CAN_BYTE(b+1) << 16) \
                            | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
  #define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
  #define CAN_NIBH(b)     (can_databuffer[b] >> 4)
  #define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))
  
  
  switch (p_frame->MsgID)
  {
    case 0x081:
      // --------------------------------------------------------------------------
      // CAN ID 0x081: CANopen error message from SEVCON (Node #1)
      
      // count errors to detect manual CFG RESET request:
      if ((CAN_BYTE(1)==0x10) && (CAN_BYTE(2)==0x01))
        twizy_button_cnt++;
      
      break;
    
    
    case 0x155:
      // --------------------------------------------------------------------------
      // *** BMS: POWER STATUS ***
      
      // Overwrite BMS>>CHG protocol to limit charge power:
      // cfg_chargelevel = maximum power, 1..7 = 300..2100 W
      if ((twizy_status & CAN_STATUS_CHARGING) &&
          (twizy_flags.EnableWrite) &&
          (cfg_chargelevel > 0) &&
          (((INT8)CAN_BYTE(0)) > cfg_chargelevel))
      {
        CAN_frame_t txframe = *p_frame;
        txframe.data.u8[0] = cfg_chargelevel;
        txframe.Write();
      }
      
      // Basic validation:
      // Byte 4:  0x94 = init/exit phase (CAN data invalid)
      //          0x54 = Twizy online (CAN data valid)
      if (can_databuffer[3] == 0x54)
      {
        unsigned int t;
        
        // BMS to CHG power level request:
        // (take only while charging to keep finishing level for "done" detection)
        if ((twizy_status & 0x60) == 0x20)
          twizy_chg_power_request = CAN_BYTE(0);
        
        // SOC:
        t = ((unsigned int) can_databuffer[4] << 8) + can_databuffer[5];
        if (t > 0 && t <= 40000)
        {
          twizy_soc = t >> 2;
          // car value derived in ticker1()
          
          // Remember maximum SOC for charging "done" distinction:
          if (twizy_soc > twizy_soc_max)
            twizy_soc_max = twizy_soc;
          
          // ...and minimum SOC for range calculation during charging:
          if (twizy_soc < twizy_soc_min)
          {
            twizy_soc_min = twizy_soc;
            twizy_soc_min_range = twizy_range_est;
          }
        }
        
        // CURRENT & POWER:
        t = ((unsigned int) (can_databuffer[1] & 0x0f) << 8) + can_databuffer[2];
        if (t > 0 && t < 0x0f00)
        {
          twizy_current = 2000 - (signed int) t;
          // ...in 1/4 A
          
          // set min/max:
          if (twizy_current < twizy_current_min)
            twizy_current_min = twizy_current;
          if (twizy_current > twizy_current_max)
            twizy_current_max = twizy_current;
          
          // calculate power:
          twizy_power = (twizy_current < 0)
            ? -((((long) -twizy_current) * twizy_batt[0].volt_act + 128) >> 8)
            : ((((long) twizy_current) * twizy_batt[0].volt_act + 128) >> 8);
          // ...in 256/40 W = 6.4 W
          
          // set min/max:
          if (twizy_power < twizy_power_min)
            twizy_power_min = twizy_power;
          if (twizy_power > twizy_power_max)
            twizy_power_max = twizy_power;
          
          // calculate distance from ref:
          if (twizy_dist >= twizy_speed_distref)
            t = twizy_dist - twizy_speed_distref;
          else
            t = twizy_dist + (0x10000L - twizy_speed_distref);
          twizy_speed_distref = twizy_dist;
          
          // add to speed state:
          twizy_speedpwr[twizy_speed_state].dist += t;
          if (twizy_current > 0)
          {
            twizy_speedpwr[twizy_speed_state].use += twizy_power;
            twizy_level_use += twizy_power;
            twizy_charge_use += twizy_current;
          }
          else
          {
            twizy_speedpwr[twizy_speed_state].rec += -twizy_power;
            twizy_level_rec += -twizy_power;
            twizy_charge_rec += -twizy_current;
          }
          
          // do we need to take base power consumption into account?
          // i.e. for lights etc. -- varies...
        }
      }
      break; // case 0x155
    
    
    case 0x196:
      // --------------------------------------------------------------------------
      // CAN ID 0x196: 10 ms period
      
      // MOTOR TEMPERATURE:
      if (CAN_BYTE(5) > 0 && CAN_BYTE(5) < 0xf0)
        twizy_tmotor = (signed int) CAN_BYTE(5) - 40;
      else
        twizy_tmotor = 0;
      
      break;
    
    
    case 0x424:
      // --------------------------------------------------------------------------
      // CAN ID 0x424: sent every 100 ms (10 per second)
      
      // Overwrite BMS>>CHG protocol to stop charge:
      // requested by setting twizy_chg_stop_request to true
      if ((twizy_status & CAN_STATUS_CHARGING) &&
          (twizy_flags.EnableWrite) &&
          (twizy_chg_stop_request))
      {
        CAN_frame_t txframe = *p_frame;
        txframe.data.u8[0] = 0x12; // charge stop request
        txframe.Write();
      }
      
      // max drive (discharge) + recup (charge) power:
      if (CAN_BYTE(2) != 0xff)
        twizy_batt[0].max_recup_pwr = CAN_BYTE(2);
      if (CAN_BYTE(3) != 0xff)
        twizy_batt[0].max_drive_pwr = CAN_BYTE(3);
      
      // BMS SOH:
      twizy_soh = CAN_BYTE(5);
      
      break;
    
    
    case 0x554:
      // --------------------------------------------------------------------------
      // CAN ID 0x554: Battery cell module temperatures
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        for (int i = 0; i < BATT_CMODS; i++)
          twizy_cmod[i].temp_act = CAN_BYTE(i);
      }
      break;
    
    case 0x556:
      // --------------------------------------------------------------------------
      // CAN ID 0x556: Battery cell voltages 1-5
      // 100 ms = 10 per second
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[0].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[1].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[2].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[3].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
        twizy_cell[4].volt_act = ((UINT) CAN_BYTE(6) << 4) | ((UINT) CAN_NIBH(7));
      }
      
      break;
    
    case 0x557:
      // --------------------------------------------------------------------------
      // CAN ID 0x557: Battery cell voltages 6-10
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[5].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[6].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[7].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[8].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
        twizy_cell[9].volt_act = ((UINT) CAN_BYTE(6) << 4) | ((UINT) CAN_NIBH(7));
      }
      break;
    
    case 0x55E:
      // --------------------------------------------------------------------------
      // CAN ID 0x55E: Battery cell voltages 11-14
      // (1000 ms = 1 per second)
      if (CAN_BYTE(0) != 0x0ff)
      {
        twizy_cell[10].volt_act = ((UINT) CAN_BYTE(0) << 4) | ((UINT) CAN_NIBH(1));
        twizy_cell[11].volt_act = ((UINT) CAN_NIBL(1) << 8) | ((UINT) CAN_BYTE(2));
        twizy_cell[12].volt_act = ((UINT) CAN_BYTE(3) << 4) | ((UINT) CAN_NIBH(4));
        twizy_cell[13].volt_act = ((UINT) CAN_NIBL(4) << 8) | ((UINT) CAN_BYTE(5));
      }
      break;
    
    case 0x55F:
      // --------------------------------------------------------------------------
      // CAN ID 0x55F: Battery pack voltages
      // (1000 ms = 1 per second)
      if (CAN_BYTE(5) != 0x0ff)
      {
        // we still don't know why there are two pack voltages
        // best guess: take avg
        UINT v1, v2;
        
        v1 = ((UINT) CAN_BYTE(5) << 4) | ((UINT) CAN_NIBH(6));
        v2 = ((UINT) CAN_NIBL(6) << 8) | ((UINT) CAN_BYTE(7));
        
        twizy_batt[0].volt_act = (v1 + v2 + 1) >> 1;
      }
      break;
    
    
#ifdef OVMS_TWIZY_CFG
    case 0x581:
      // --------------------------------------------------------------------------
      // CAN ID 0x581: CANopen SDO reply from SEVCON (Node #1)
      //
      
      // copy message into twizy_sdo object:
      for (u = 0; u < can_datalength; u++)
        twizy_sdo.byte[u] = CAN_BYTE(u);
      for (; u < 8; u++)
        twizy_sdo.byte[u] = 0;
      
      break;
#endif // OVMS_TWIZY_CFG
    
    
    case 0x597:
      // --------------------------------------------------------------------------
      // CAN ID 0x597: sent every 100 ms (10 per second)
      
      // VEHICLE state:
      //  [0]: 0x20 = power line connected
      if (CAN_BYTE(0) & 0x20)
        *StdMetrics.ms_v_charge_voltage = (float) 230; // fix 230 V
      else
        *StdMetrics.ms_v_charge_voltage = (float) 0;
      
      // twizy_status high nibble:
      //  [1] bit 4 = 0x10 CAN_STATUS_KEYON: 1 = Car ON (key switch)
      //  [1] bit 5 = 0x20 CAN_STATUS_CHARGING: 1 = Charging
      //  [1] bit 6 = 0x40 CAN_STATUS_OFFLINE: 1 = Switch-ON/-OFF phase
      //
      // low nibble: taken from 59B, clear GO flag while OFFLINE
      // to prevent sticky GO after switch-off
      // (597 comes before/after 59B in init/exit phase)
      
      // init cyclic distance counter on switch-on:
      if ((CAN_BYTE(1) & CAN_STATUS_KEYON) && (!(twizy_status & CAN_STATUS_KEYON)))
        twizy_dist = twizy_speed_distref = 0;
      
      if (CAN_BYTE(1) & CAN_STATUS_OFFLINE)
        twizy_status = (twizy_status & 0x07) | (CAN_BYTE(1) & 0xF0);
      else
        twizy_status = (twizy_status & 0x0F) | (CAN_BYTE(1) & 0xF0);
      
      // Read 12V DC converter current level:
      *StdMetrics.ms_v_bat_12v_current = (float) CAN_BYTE(2) / 5;
      
      // Read 12V DC converter status:
      twizy_flags.Charging12V = ((CAN_BYTE(3) & 0xC0) != 0xC0);
      
      // CHARGER temperature:
      if (CAN_BYTE(7) > 0 && CAN_BYTE(7) < 0xf0)
        *StdMetrics.ms_v_charge_temp = (float) CAN_BYTE(7) - 40;
      else
        *StdMetrics.ms_v_charge_temp = (float) 0;
      
      break; // case 0x597
    
    
    case 0x599:
      // --------------------------------------------------------------------------
      // CAN ID 0x599: sent every 100 ms (10 per second)
      
      // RANGE:
      // we need to check for charging, as the Twizy
      // does not update range during charging
      if (((twizy_status & 0x60) == 0)
        && (can_databuffer[5] != 0xff) && (can_databuffer[5] > 0))
      {
        twizy_range_est = can_databuffer[5];
        // car values derived in ticker1()
      }
      
      // SPEED:
      u = ((unsigned int) can_databuffer[6] << 8) + can_databuffer[7];
      if (u != 0xffff)
      {
        int delta = (int) u - (int) twizy_speed;
        
        // set min/max:
        if (delta < twizy_accel_min)
          twizy_accel_min = delta;
        if (delta > twizy_accel_max)
          twizy_accel_max = delta;
        
        // running average over 4 samples:
        twizy_accel_avg = twizy_accel_avg * 3 + delta;
        // C18: no arithmetic >> sign propagation on negative ints
        twizy_accel_avg = (twizy_accel_avg < 0)
          ? -((-twizy_accel_avg + 2) >> 2)
          : ((twizy_accel_avg + 2) >> 2);
        
        // switch speed state:
        if (twizy_accel_avg >= CAN_ACCEL_THRESHOLD)
          twizy_speed_state = CAN_SPEED_ACCEL;
        else if (twizy_accel_avg <= -CAN_ACCEL_THRESHOLD)
          twizy_speed_state = CAN_SPEED_DECEL;
        else
          twizy_speed_state = CAN_SPEED_CONST;
        
        // speed/delta sum statistics while driving:
        if (u >= CAN_SPEED_THRESHOLD)
        {
          // overall speed avg:
          twizy_speedpwr[0].spdcnt++;
          twizy_speedpwr[0].spdsum += u;
          
          // accel/decel speed avg:
          if (twizy_speed_state != 0)
          {
            twizy_speedpwr[twizy_speed_state].spdcnt++;
            twizy_speedpwr[twizy_speed_state].spdsum += ABS(twizy_accel_avg);
          }
        }
        
        twizy_speed = u;
        // car value derived in ticker1()
      }
      
      break; // case 0x599
    
    
    case 0x59B:
      // --------------------------------------------------------------------------
      // CAN ID 0x59B: sent every 100 ms (10 per second)
      
      // twizy_status low nibble:
      twizy_status = (twizy_status & 0xF0) | (CAN_BYTE(1) & 0x09);
      if (CAN_BYTE(0) == 0x80)
        twizy_status |= CAN_STATUS_MODE_D;
      else if (CAN_BYTE(0) == 0x08)
        twizy_status |= CAN_STATUS_MODE_R;
      
      #ifdef OVMS_TWIZY_CFG
      
        // accelerator pedal:
        u = CAN_BYTE(3);
        
        // running average over 2 samples:
        u = (twizy_accel_pedal + u + 1) >> 1;
        
        // kickdown detection:
        s = KICKDOWN_THRESHOLD(twizy_accel_pedal);
        if ( ((s > 0) && (u > ((unsigned int)twizy_accel_pedal + s)))
          || ((twizy_kickdown_level > 0) && (u > twizy_kickdown_level)) )
        {
          twizy_kickdown_level = u;
        }
        
        twizy_accel_pedal = u;
      
      #endif // OVMS_TWIZY_CFG
      
      break;
      
      
    case 0x59E:
      // --------------------------------------------------------------------------
      // CAN ID 0x59E: sent every 100 ms (10 per second)
      
      // CYCLIC DISTANCE COUNTER:
      twizy_dist = ((UINT) CAN_BYTE(0) << 8) + CAN_BYTE(1);
      
      // SEVCON TEMPERATURE:
      if (CAN_BYTE(5) > 0 && CAN_BYTE(5) < 0xf0)
        *StdMetrics.ms_v_inv_temp = (float) CAN_BYTE(5) - 40;
      else
        *StdMetrics.ms_v_inv_temp = (float) 0;
      
      break;
    
    
    case 0x5D7:
      // --------------------------------------------------------------------------
      // *** ODOMETER ***
      twizy_odometer = ((unsigned long) CAN_BYTE(5) >> 4)
        | ((unsigned long) CAN_BYTE(4) << 4)
        | ((unsigned long) CAN_BYTE(3) << 12)
        | ((unsigned long) CAN_BYTE(2) << 20);
      break;
      
      
    case 0x69F:
      // --------------------------------------------------------------------------
      // *** VIN ***
      // last 7 digits of real VIN, in nibbles, reverse:
      // (assumption: no hex digits)
      if (!twizy_vin[0]) // we only need to process this once
      {
        twizy_vin[0] = '0' + CAN_NIB(7);
        twizy_vin[1] = '0' + CAN_NIB(6);
        twizy_vin[2] = '0' + CAN_NIB(5);
        twizy_vin[3] = '0' + CAN_NIB(4);
        twizy_vin[4] = '0' + CAN_NIB(3);
        twizy_vin[5] = '0' + CAN_NIB(2);
        twizy_vin[6] = '0' + CAN_NIB(1);
        twizy_vin[7] = 0;
        *StdMetrics.ms_v_vin = (string) twizy_vin;
      }
      break;
    
    
    case 0x700:
      // --------------------------------------------------------------------------
      // CAN ID 0x700: VirtualBMS extension:
      // see https://github.com/dexterbg/Twizy-Virtual-BMS/blob/master/API.md#extended-info-frame
      //   - Byte 0: BMS specific state #1 (main state, i.e. twizy.state())
      //   - Byte 1: highest 3 bits = BMS type ID (see below), remaining 5 bits = BMS specific error code (see below)
      //   - Bytes 2-4: cell voltages #15 & #16 (encoded in 12 bits like #1-#14)
      //   - Bytes 5-6: balancing status (bits 15…0 = cells 16…1, 1 = balancing active)
      //   - Byte 7: BMS specific state #2 (auxiliary state or data)
      if (CAN_BYTE(0) != 0x0ff)
      {
        // Battery cell voltages 15 + 16:
        twizy_cell[14].volt_act = ((UINT) CAN_BYTE(2) << 4) | ((UINT) CAN_NIBH(3));
        twizy_cell[15].volt_act = ((UINT) CAN_NIBL(3) << 8) | ((UINT) CAN_BYTE(4));
      }
      break;
      
  }
  
}


