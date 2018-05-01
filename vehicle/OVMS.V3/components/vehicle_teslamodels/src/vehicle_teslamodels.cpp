/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

#include "ovms_log.h"
static const char *TAG = "v-teslamodels";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslamodels.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

OvmsVehicleTeslaModelS::OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Tesla Model S vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  }

OvmsVehicleTeslaModelS::~OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Model S vehicle module");
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x102: // BMS current and voltage
      {
      StandardMetrics.ms_v_bat_voltage->SetValue(((float)((int)d[1]<<8)+d[0])/100);
      StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      break;
      }
    case 0x116: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_awake->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          break;
        case 3: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          break;
        case 4: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          break;
        default:
          break;
        }
      break;
      }
    case 0x256: // Speed
      {
      StandardMetrics.ms_v_pos_speed->SetValue( ((((int)d[3]&0x0f)<<8) + (int)d[2])/10, (d[3]&0x80)?Kph:Mph );
      break;
      }
    case 0x302: // SOC
      {
      StandardMetrics.ms_v_bat_soc->SetValue( (((int)d[1]>>2) + (((int)d[2] & 0x0f)<<6))/10 );
      break;
      }
    case 0x398: // Country
      {
      m_type[0] = 'T';
      m_type[1] = 'S';
      m_type[2] = d[0];
      m_type[3] = d[1];
      StandardMetrics.ms_v_type->SetValue(m_type);
      break;
      }
    case 0x508: // VIN
      {
      switch(d[0])
        {
        case 0:
          memcpy(m_vin,d+1,7);
          break;
        case 1:
          memcpy(m_vin+7,d+1,7);
          break;
        case 2:
          memcpy(m_vin+14,d+1,3);
          m_vin[17] = 0;
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          break;
        }
      break;
      }
    case 0x5d8: // Odometer (0x562 is battery, so this is motor or car?)
      {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[3]<<24)
                                                + ((uint32_t)d[2]<<16)
                                                + ((uint32_t)d[1]<<8)
                                                + d[0])/1000, Miles);
      break;
      }
    default:
      break;
    }
  }

class OvmsVehicleTeslaModelSInit
  {
  public: OvmsVehicleTeslaModelSInit();
} MyOvmsVehicleTeslaModelSInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaModelSInit::OvmsVehicleTeslaModelSInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Model S (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaModelS>("TS","Tesla Model S");
  }
