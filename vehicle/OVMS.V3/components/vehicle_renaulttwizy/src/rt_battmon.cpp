/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
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
static const char *TAG = "v-twizy";

#include <math.h>

#include "rt_battmon.h"
#include "metrics_standard.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"


// Battery cell/cmod deviation alert thresholds:
#define BATT_DEV_TEMP_ALERT         3       // = 3 °C
#define BATT_DEV_VOLT_ALERT         6       // = 30 mV

// ...thresholds for overall stddev:
#define BATT_STDDEV_TEMP_WATCH      2       // = 2 °C
#define BATT_STDDEV_TEMP_ALERT      3       // = 3 °C
#define BATT_STDDEV_VOLT_WATCH      3       // = 15 mV
#define BATT_STDDEV_VOLT_ALERT      5       // = 25 mV

// watch/alert flags for overall stddev:
#define BATT_STDDEV_TEMP_FLAG       31  // bit #31
#define BATT_STDDEV_VOLT_FLAG       31  // bit #31


// BEGIN Battery metrics names
// DO NOT EDIT -- generated by gen_metrics_names.sh
const char* const x_rt_b_pack_volt_min[] = { "xrt.b.pack.1.volt.min" };
const char* const x_rt_b_pack_volt_max[] = { "xrt.b.pack.1.volt.max" };
const char* const x_rt_b_pack_volt_watches[] = { "xrt.b.pack.1.volt.watches" };
const char* const x_rt_b_pack_volt_alerts[] = { "xrt.b.pack.1.volt.alerts" };
const char* const x_rt_b_pack_volt_stddev_max[] = { "xrt.b.pack.1.volt.stddev.max" };
const char* const x_rt_b_pack_temp_min[] = { "xrt.b.pack.1.temp.min" };
const char* const x_rt_b_pack_temp_max[] = { "xrt.b.pack.1.temp.max" };
const char* const x_rt_b_pack_temp_watches[] = { "xrt.b.pack.1.temp.watches" };
const char* const x_rt_b_pack_temp_alerts[] = { "xrt.b.pack.1.temp.alerts" };
const char* const x_rt_b_pack_temp_stddev_max[] = { "xrt.b.pack.1.temp.stddev.max" };
const char* const x_rt_b_cmod_temp_act[] = { "xrt.b.cmod.01.temp.act", "xrt.b.cmod.02.temp.act", "xrt.b.cmod.03.temp.act", "xrt.b.cmod.04.temp.act", "xrt.b.cmod.05.temp.act", "xrt.b.cmod.06.temp.act", "xrt.b.cmod.07.temp.act", "xrt.b.cmod.08.temp.act" };
const char* const x_rt_b_cmod_temp_min[] = { "xrt.b.cmod.01.temp.min", "xrt.b.cmod.02.temp.min", "xrt.b.cmod.03.temp.min", "xrt.b.cmod.04.temp.min", "xrt.b.cmod.05.temp.min", "xrt.b.cmod.06.temp.min", "xrt.b.cmod.07.temp.min", "xrt.b.cmod.08.temp.min" };
const char* const x_rt_b_cmod_temp_max[] = { "xrt.b.cmod.01.temp.max", "xrt.b.cmod.02.temp.max", "xrt.b.cmod.03.temp.max", "xrt.b.cmod.04.temp.max", "xrt.b.cmod.05.temp.max", "xrt.b.cmod.06.temp.max", "xrt.b.cmod.07.temp.max", "xrt.b.cmod.08.temp.max" };
const char* const x_rt_b_cmod_temp_maxdev[] = { "xrt.b.cmod.01.temp.maxdev", "xrt.b.cmod.02.temp.maxdev", "xrt.b.cmod.03.temp.maxdev", "xrt.b.cmod.04.temp.maxdev", "xrt.b.cmod.05.temp.maxdev", "xrt.b.cmod.06.temp.maxdev", "xrt.b.cmod.07.temp.maxdev", "xrt.b.cmod.08.temp.maxdev" };
const char* const x_rt_b_cell_volt_act[] = { "xrt.b.cell.01.volt.act", "xrt.b.cell.02.volt.act", "xrt.b.cell.03.volt.act", "xrt.b.cell.04.volt.act", "xrt.b.cell.05.volt.act", "xrt.b.cell.06.volt.act", "xrt.b.cell.07.volt.act", "xrt.b.cell.08.volt.act", "xrt.b.cell.09.volt.act", "xrt.b.cell.10.volt.act", "xrt.b.cell.11.volt.act", "xrt.b.cell.12.volt.act", "xrt.b.cell.13.volt.act", "xrt.b.cell.14.volt.act", "xrt.b.cell.15.volt.act", "xrt.b.cell.16.volt.act" };
const char* const x_rt_b_cell_volt_min[] = { "xrt.b.cell.01.volt.min", "xrt.b.cell.02.volt.min", "xrt.b.cell.03.volt.min", "xrt.b.cell.04.volt.min", "xrt.b.cell.05.volt.min", "xrt.b.cell.06.volt.min", "xrt.b.cell.07.volt.min", "xrt.b.cell.08.volt.min", "xrt.b.cell.09.volt.min", "xrt.b.cell.10.volt.min", "xrt.b.cell.11.volt.min", "xrt.b.cell.12.volt.min", "xrt.b.cell.13.volt.min", "xrt.b.cell.14.volt.min", "xrt.b.cell.15.volt.min", "xrt.b.cell.16.volt.min" };
const char* const x_rt_b_cell_volt_max[] = { "xrt.b.cell.01.volt.max", "xrt.b.cell.02.volt.max", "xrt.b.cell.03.volt.max", "xrt.b.cell.04.volt.max", "xrt.b.cell.05.volt.max", "xrt.b.cell.06.volt.max", "xrt.b.cell.07.volt.max", "xrt.b.cell.08.volt.max", "xrt.b.cell.09.volt.max", "xrt.b.cell.10.volt.max", "xrt.b.cell.11.volt.max", "xrt.b.cell.12.volt.max", "xrt.b.cell.13.volt.max", "xrt.b.cell.14.volt.max", "xrt.b.cell.15.volt.max", "xrt.b.cell.16.volt.max" };
const char* const x_rt_b_cell_volt_maxdev[] = { "xrt.b.cell.01.volt.maxdev", "xrt.b.cell.02.volt.maxdev", "xrt.b.cell.03.volt.maxdev", "xrt.b.cell.04.volt.maxdev", "xrt.b.cell.05.volt.maxdev", "xrt.b.cell.06.volt.maxdev", "xrt.b.cell.07.volt.maxdev", "xrt.b.cell.08.volt.maxdev", "xrt.b.cell.09.volt.maxdev", "xrt.b.cell.10.volt.maxdev", "xrt.b.cell.11.volt.maxdev", "xrt.b.cell.12.volt.maxdev", "xrt.b.cell.13.volt.maxdev", "xrt.b.cell.14.volt.maxdev", "xrt.b.cell.15.volt.maxdev", "xrt.b.cell.16.volt.maxdev" };
// END Battery metrics names


/**
 * vehicle_twizy_batt: command wrapper for CommandBatt
 */
void vehicle_twizy_batt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  OvmsVehicleRenaultTwizy* twizy = (OvmsVehicleRenaultTwizy*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  
  if (!twizy || type != "RT")
  {
    writer->puts("Error: Twizy vehicle module not selected");
    return;
  }

  twizy->CommandBatt(verbosity, writer, cmd, argc, argv);
}


/**
 * BatteryInit:
 */
void OvmsVehicleRenaultTwizy::BatteryInit()
{
  ESP_LOGI(TAG, "battmon subsystem init");
  
  int i;
  
  // init metrics
  
  m_batt_soc_min = MyMetrics.InitFloat("xrt.b.soc.min", SM_STALE_HIGH, (float) twizy_soc_min / 100, Percentage);
  m_batt_soc_max = MyMetrics.InitFloat("xrt.b.soc.max", SM_STALE_HIGH, (float) twizy_soc_max / 100, Percentage);
  
  m_batt_pack_count = MyMetrics.InitInt("xrt.b.pack.cnt", SM_STALE_HIGH, batt_pack_count);
  for (i = 0; i < BATT_PACKS; i++)
    twizy_batt[i].InitMetrics(i);
  
  m_batt_cmod_count = MyMetrics.InitInt("xrt.b.cmod.cnt", SM_STALE_HIGH, batt_cmod_count);
  for (i = 0; i < BATT_CMODS; i++)
    twizy_cmod[i].InitMetrics(i);
  
  m_batt_cell_count = MyMetrics.InitInt("xrt.b.cell.cnt", SM_STALE_HIGH, batt_cell_count);
  for (i = 0; i < BATT_CELLS; i++)
    twizy_cell[i].InitMetrics(i);
  
  // init commands
  
  cmd_batt = cmd_xrt->RegisterCommand("batt", "Battery monitor", NULL, "", 0, 0, true);
  {
    cmd_batt->RegisterCommand("reset", "Reset alerts & watches", vehicle_twizy_batt, "", 0, 0, true);
    cmd_batt->RegisterCommand("status", "Status report", vehicle_twizy_batt, "[<pack>]", 0, 1, true);
    cmd_batt->RegisterCommand("volt", "Show voltages", vehicle_twizy_batt, "", 0, 0, true);
    cmd_batt->RegisterCommand("vdev", "Show voltage deviations", vehicle_twizy_batt, "", 0, 0, true);
    cmd_batt->RegisterCommand("temp", "Show temperatures", vehicle_twizy_batt, "", 0, 0, true);
    cmd_batt->RegisterCommand("tdev", "Show temperature deviations", vehicle_twizy_batt, "", 0, 0, true);
    cmd_batt->RegisterCommand("data-pack", "Output pack record", vehicle_twizy_batt, "[<pack>]", 0, 1, true);
    cmd_batt->RegisterCommand("data-cell", "Output cell record", vehicle_twizy_batt, "<cell>", 1, 1, true);
  }
  
}


/**
 * BatteryUpdate:
 *  - update pack layout
 *  - calculate voltage & temperature deviations
 *  - publish internal state to metrics
 */
void OvmsVehicleRenaultTwizy::BatteryUpdate()
{
  // update pack layout:
  
  if (twizy_cell[15].volt_act != 0 && twizy_cell[15].volt_act != 0x0fff)
    batt_cell_count = 16;
  if (twizy_cell[14].volt_act != 0 && twizy_cell[14].volt_act != 0x0fff)
    batt_cell_count = 15;
  else
    batt_cell_count = 14;
  *m_batt_cell_count = (int) batt_cell_count;
  
  if (twizy_cmod[7].temp_act != 0 && twizy_cmod[7].temp_act != 0xff)
    batt_cmod_count = 8;
  else
    batt_cmod_count = 7;
  *m_batt_cmod_count = (int) batt_cmod_count;
  
  
  // calculate voltage & temperature deviations:
  
  BatteryCheckDeviations();
  
  
  // publish internal state to metrics:
  
  *StdMetrics.ms_v_bat_soc = (float) twizy_soc / 100;
  *m_batt_soc_min = (float) twizy_soc_min / 100;
  *m_batt_soc_max = (float) twizy_soc_max / 100;
  
  *StdMetrics.ms_v_bat_soh = (float) twizy_soh;
  *StdMetrics.ms_v_bat_cac = (float) cfg_bat_cap_actual_prc / 100 * cfg_bat_cap_nominal_ah;
  // … add metrics for net capacity and cap_prc?
  
  *StdMetrics.ms_v_bat_temp = (float) twizy_batt[0].temp_act - 40;
  *StdMetrics.ms_v_bat_voltage = (float) twizy_batt[0].volt_act / 10;
  *StdMetrics.ms_v_bat_current = (float) twizy_current / 4;
  
  for (battery_pack &pack : twizy_batt)
    pack.UpdateMetrics();
  
  for (battery_cmod &cmod : twizy_cmod)
    cmod.UpdateMetrics();
  
  for (battery_cell &cell : twizy_cell)
    cell.UpdateMetrics();
  
}


/**
 * BatteryReset: reset deviations, alerts & watches
 */
void OvmsVehicleRenaultTwizy::BatteryReset()
{
  ESP_LOGD(TAG, "battmon reset");
  
  for (battery_cell &cell : twizy_cell)
  {
    cell.volt_max = cell.volt_act;
    cell.volt_min = cell.volt_act;
    cell.volt_maxdev = 0;
  }
  
  for (battery_cmod &cmod : twizy_cmod)
  {
    cmod.temp_max = cmod.temp_act;
    cmod.temp_min = cmod.temp_act;
    cmod.temp_maxdev = 0;
  }
  
  for (battery_pack &pack : twizy_batt)
  {
    pack.volt_min = pack.volt_act;
    pack.volt_max = pack.volt_act;
    pack.cell_volt_stddev_max = 0;
    pack.temp_min = pack.temp_act;
    pack.temp_max = pack.temp_act;
    pack.cmod_temp_stddev_max = 0;
    pack.temp_watches = 0;
    pack.temp_alerts = 0;
    pack.volt_watches = 0;
    pack.volt_alerts = 0;
    pack.last_temp_alerts = 0;
    pack.last_volt_alerts = 0;
  }
}


/**
 * BatteryCheckDeviations:
 *  - calculate voltage & temperature deviations
 *  - set watch/alert flags accordingly
 */

void OvmsVehicleRenaultTwizy::BatteryCheckDeviations(void)
{
  UINT i;
  UINT stddev, absdev;
  int dev;
  float m;
  UINT32 sum, sqrsum;
  
  
  // *********** Temperatures: ************
  
  // build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  
  for (i = 0; i < batt_cmod_count; i++)
  {
    // Validate:
    if ((twizy_cmod[i].temp_act == 0) || (twizy_cmod[i].temp_act >= 0x0f0))
      break;
    
    // Remember min:
    if ((twizy_cmod[i].temp_min == 0) || (twizy_cmod[i].temp_act < twizy_cmod[i].temp_min))
      twizy_cmod[i].temp_min = twizy_cmod[i].temp_act;
    
    // Remember max:
    if ((twizy_cmod[i].temp_max == 0) || (twizy_cmod[i].temp_act > twizy_cmod[i].temp_max))
      twizy_cmod[i].temp_max = twizy_cmod[i].temp_act;
    
    // build sums:
    sum += twizy_cmod[i].temp_act;
    sqrsum += SQR((UINT32) twizy_cmod[i].temp_act);
  }
  
  if (i == batt_cmod_count)
  {
    // All values valid, process:
    
    m = (float) sum / batt_cmod_count;
    
    twizy_batt[0].temp_act = m;
    
    // Battery pack usage cycle min/max:
    
    if ((twizy_batt[0].temp_min == 0) || (twizy_batt[0].temp_act < twizy_batt[0].temp_min))
      twizy_batt[0].temp_min = twizy_batt[0].temp_act;
    if ((twizy_batt[0].temp_max == 0) || (twizy_batt[0].temp_act > twizy_batt[0].temp_max))
      twizy_batt[0].temp_max = twizy_batt[0].temp_act;
    
    stddev = sqrtf( ((float)sqrsum/batt_cmod_count) - SQR((float)sum/batt_cmod_count) ) + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0
    
    // check max stddev:
    if (stddev > twizy_batt[0].cmod_temp_stddev_max)
    {
      twizy_batt[0].cmod_temp_stddev_max = stddev;
      
      // switch to overall stddev alert mode?
      // (resetting cmod flags to build new alert set)
      if (stddev >= BATT_STDDEV_TEMP_ALERT)
      {
        twizy_batt[0].temp_alerts.reset();
        twizy_batt[0].temp_alerts.set(BATT_STDDEV_TEMP_FLAG);
      }
      else if (stddev >= BATT_STDDEV_TEMP_WATCH)
      {
        twizy_batt[0].temp_watches.reset();
        twizy_batt[0].temp_watches.set(BATT_STDDEV_TEMP_FLAG);
      }
    }
    
    // check cmod deviations:
    for (i = 0; i < batt_cmod_count; i++)
    {
      // deviation:
      dev = (twizy_cmod[i].temp_act - m)
              + ((twizy_cmod[i].temp_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);
      
      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].temp_alerts[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_ALERT))
        twizy_batt[0].temp_alerts.set(i);
      else if (absdev >= BATT_DEV_TEMP_ALERT)
        twizy_batt[0].temp_alerts.set(i);
      else if ((twizy_batt[0].temp_watches[BATT_STDDEV_TEMP_FLAG]) && (absdev >= BATT_STDDEV_TEMP_WATCH))
        twizy_batt[0].temp_watches.set(i);
      else if (absdev > stddev)
        twizy_batt[0].temp_watches.set(i);
      
      // Remember max deviation:
      if (absdev > ABS(twizy_cmod[i].temp_maxdev))
        twizy_cmod[i].temp_maxdev = dev;
    }
    
  } // if( i == batt_cmod_count )
  
  
  // ********** Voltages: ************
  
  // Battery pack usage cycle min/max:
  
  if ((twizy_batt[0].volt_min == 0) || (twizy_batt[0].volt_act < twizy_batt[0].volt_min))
    twizy_batt[0].volt_min = twizy_batt[0].volt_act;
  if ((twizy_batt[0].volt_max == 0) || (twizy_batt[0].volt_act > twizy_batt[0].volt_max))
    twizy_batt[0].volt_max = twizy_batt[0].volt_act;
  
  // Cells: build mean value & standard deviation:
  sum = 0;
  sqrsum = 0;
  
  for (i = 0; i < batt_cell_count; i++)
  {
    // Validate:
    if ((twizy_cell[i].volt_act == 0) || (twizy_cell[i].volt_act >= 0x0f00))
      break;
    
    // Remember min:
    if ((twizy_cell[i].volt_min == 0) || (twizy_cell[i].volt_act < twizy_cell[i].volt_min))
      twizy_cell[i].volt_min = twizy_cell[i].volt_act;
    
    // Remember max:
    if ((twizy_cell[i].volt_max == 0) || (twizy_cell[i].volt_act > twizy_cell[i].volt_max))
      twizy_cell[i].volt_max = twizy_cell[i].volt_act;
    
    // build sums:
    sum += twizy_cell[i].volt_act;
    sqrsum += SQR((UINT32) twizy_cell[i].volt_act);
  }
  
  if (i == batt_cell_count)
  {
    // All values valid, process:
    
    m = (float) sum / batt_cell_count;
    
    stddev = sqrtf( ((float)sqrsum/batt_cell_count) - SQR((float)sum/batt_cell_count) ) + 0.5;
    if (stddev == 0)
      stddev = 1; // not enough precision to allow stddev 0
    
    // check max stddev:
    if (stddev > twizy_batt[0].cell_volt_stddev_max)
    {
      twizy_batt[0].cell_volt_stddev_max = stddev;
      
      // switch to overall stddev alert mode?
      // (resetting cell flags to build new alert set)
      if (stddev >= BATT_STDDEV_VOLT_ALERT)
      {
        twizy_batt[0].volt_alerts.reset();
        twizy_batt[0].volt_alerts.set(BATT_STDDEV_VOLT_FLAG);
      }
      else if (stddev >= BATT_STDDEV_VOLT_WATCH)
      {
        twizy_batt[0].volt_watches.reset();
        twizy_batt[0].volt_watches.set(BATT_STDDEV_VOLT_FLAG);
      }
    }
    
    // check cell deviations:
    for (i = 0; i < batt_cell_count; i++)
    {
      // deviation:
      dev = (twizy_cell[i].volt_act - m)
              + ((twizy_cell[i].volt_act >= m) ? 0.5 : -0.5);
      absdev = ABS(dev);
      
      // Set watch/alert flags:
      // (applying overall thresholds only in stddev alert mode)
      if ((twizy_batt[0].volt_alerts[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_ALERT))
        twizy_batt[0].volt_alerts.set(i);
      else if (absdev >= BATT_DEV_VOLT_ALERT)
        twizy_batt[0].volt_alerts.set(i);
      else if ((twizy_batt[0].volt_watches[BATT_STDDEV_VOLT_FLAG]) && (absdev >= BATT_STDDEV_VOLT_WATCH))
        twizy_batt[0].volt_watches.set(i);
      else if (absdev > stddev)
        twizy_batt[0].volt_watches.set(i);
      
      // Remember max deviation:
      if (absdev > ABS(twizy_cell[i].volt_maxdev))
        twizy_cell[i].volt_maxdev = dev;
    }
    
  } // if( i == batt_cell_count )
  
  
  // Battery monitor update/alert:
  if ((twizy_batt[0].volt_alerts != twizy_batt[0].last_volt_alerts)
    || (twizy_batt[0].temp_alerts != twizy_batt[0].last_temp_alerts))
  {
    RequestNotify(SEND_BatteryAlert | SEND_BatteryStats);
  }
  
  
}


void battery_pack::InitMetrics(int i)
{
  m_volt_min = MyMetrics.InitFloat(x_rt_b_pack_volt_min[i], SM_STALE_HIGH, 0, Volts);
  m_volt_max = MyMetrics.InitFloat(x_rt_b_pack_volt_max[i], SM_STALE_HIGH, 0, Volts);
  
  m_temp_min = MyMetrics.InitFloat(x_rt_b_pack_temp_min[i], SM_STALE_HIGH, 0, Celcius);
  m_temp_max = MyMetrics.InitFloat(x_rt_b_pack_temp_max[i], SM_STALE_HIGH, 0, Celcius);
  
  m_volt_watches = MyMetrics.InitBitset<32>(x_rt_b_pack_volt_watches[i], SM_STALE_HIGH);
  m_volt_alerts = MyMetrics.InitBitset<32>(x_rt_b_pack_volt_alerts[i], SM_STALE_HIGH);
  
  m_temp_watches = MyMetrics.InitBitset<32>(x_rt_b_pack_temp_watches[i], SM_STALE_HIGH);
  m_temp_alerts = MyMetrics.InitBitset<32>(x_rt_b_pack_temp_alerts[i], SM_STALE_HIGH);
  
  m_cell_volt_stddev_max = MyMetrics.InitFloat(x_rt_b_pack_volt_stddev_max[i], SM_STALE_HIGH, 0, Volts);
  m_cmod_temp_stddev_max = MyMetrics.InitFloat(x_rt_b_pack_temp_stddev_max[i], SM_STALE_HIGH, 0, Celcius);
}

void battery_pack::UpdateMetrics()
{
  *m_volt_min = (float) volt_min / 10;
  *m_volt_max = (float) volt_max / 10;
  
  *m_temp_min = (float) temp_min - 40;
  *m_temp_max = (float) temp_max - 40;
  
  *m_volt_watches = volt_watches;
  *m_volt_alerts = volt_alerts;
  
  *m_temp_watches = temp_watches;
  *m_temp_alerts = temp_alerts;

  *m_cell_volt_stddev_max = (float) cell_volt_stddev_max / 200;
  *m_cmod_temp_stddev_max = (float) cmod_temp_stddev_max;
}

bool battery_pack::IsModified(size_t m_modifier)
{
  bool modified =
    m_volt_min->IsModifiedAndClear(m_modifier) |
    m_volt_max->IsModifiedAndClear(m_modifier) |
    m_temp_min->IsModifiedAndClear(m_modifier) |
    m_temp_max->IsModifiedAndClear(m_modifier) |
    m_cell_volt_stddev_max->IsModifiedAndClear(m_modifier) |
    m_cmod_temp_stddev_max->IsModifiedAndClear(m_modifier);
  // Note: no checking of alerts/watches here, those are overall modifiers
  //  (see BatterySendDataUpdate())
  return modified;
}


void battery_cmod::InitMetrics(int i)
{
  m_temp_act = MyMetrics.InitFloat(x_rt_b_cmod_temp_act[i], SM_STALE_HIGH, 0, Celcius);
  m_temp_min = MyMetrics.InitFloat(x_rt_b_cmod_temp_min[i], SM_STALE_HIGH, 0, Celcius);
  m_temp_max = MyMetrics.InitFloat(x_rt_b_cmod_temp_max[i], SM_STALE_HIGH, 0, Celcius);
  m_temp_maxdev = MyMetrics.InitFloat(x_rt_b_cmod_temp_maxdev[i], SM_STALE_HIGH, 0, Celcius);
}

void battery_cmod::UpdateMetrics()
{
  *m_temp_act = (float) temp_act - 40;
  *m_temp_min = (float) temp_min - 40;
  *m_temp_max = (float) temp_max - 40;
  *m_temp_maxdev = (float) temp_maxdev;
}

bool battery_cmod::IsModified(size_t m_modifier)
{
  bool modified =
    m_temp_act->IsModifiedAndClear(m_modifier) |
    m_temp_min->IsModifiedAndClear(m_modifier) |
    m_temp_max->IsModifiedAndClear(m_modifier) |
    m_temp_maxdev->IsModifiedAndClear(m_modifier);
  return modified;
}


void battery_cell::InitMetrics(int i)
{
  m_volt_act = MyMetrics.InitFloat(x_rt_b_cell_volt_act[i], SM_STALE_HIGH, 0, Volts);
  m_volt_min = MyMetrics.InitFloat(x_rt_b_cell_volt_min[i], SM_STALE_HIGH, 0, Volts);
  m_volt_max = MyMetrics.InitFloat(x_rt_b_cell_volt_max[i], SM_STALE_HIGH, 0, Volts);
  m_volt_maxdev = MyMetrics.InitFloat(x_rt_b_cell_volt_maxdev[i], SM_STALE_HIGH, 0, Volts);
}

void battery_cell::UpdateMetrics()
{
  *m_volt_act = (float) volt_act / 200;
  *m_volt_min = (float) volt_min / 200;
  *m_volt_max = (float) volt_max / 200;
  *m_volt_maxdev = (float) volt_maxdev / 200;
}

bool battery_cell::IsModified(size_t m_modifier)
{
  bool modified =
    m_volt_act->IsModifiedAndClear(m_modifier) |
    m_volt_min->IsModifiedAndClear(m_modifier) |
    m_volt_max->IsModifiedAndClear(m_modifier) |
    m_volt_maxdev->IsModifiedAndClear(m_modifier);
  return modified;
}


/**
 * CommandBatt: batt reset|status|volt|vdev|temp|tdev|data-pack|data-cell
 */
OvmsVehicleRenaultTwizy::vehicle_command_t OvmsVehicleRenaultTwizy::CommandBatt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  const char *subcmd = cmd->GetName();
  
  ESP_LOGV(TAG, "command batt %s, verbosity=%d", subcmd, verbosity);
  
  if (strcmp(subcmd, "reset") == 0)
  {
    BatteryReset();
    writer->puts("Battery monitor reset.");
  }
  
  else if (strcmp(subcmd, "status") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("Error: pack number out of range [1-%d]\n", batt_pack_count);
      return Fail;
    }
    FormatBatteryStatus(verbosity, writer, pack-1);
  }
  
  else if (subcmd[0] == 'v')
  {
    // "volt"=absolute values, "vdev"=deviations
    FormatBatteryVolts(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }
  
  else if (subcmd[0] == 't')
  {
    // "temp"=absolute values, "tdev"=deviations
    FormatBatteryTemps(verbosity, writer, (subcmd[1]=='d') ? true : false);
  }
  
  else if (strcmp(subcmd, "data-pack") == 0)
  {
    int pack = (argc > 0) ? atoi(argv[0]) : 1;
    if (pack < 1 || pack > batt_pack_count) {
      writer->printf("Error: pack number out of range [1-%d]\n", batt_pack_count);
      return Fail;
    }
    FormatPackData(verbosity, writer, pack-1);
  }

  else if (strcmp(subcmd, "data-cell") == 0)
  {
    int cell = (argc > 0) ? atoi(argv[0]) : 1;
    if (cell < 1 || cell > batt_cell_count) {
      writer->printf("Error: cell number out of range [1-%d]\n", batt_cell_count);
      return Fail;
    }
    FormatCellData(verbosity, writer, cell-1);
  }
  

  return Success;
}


/**
 * FormatBatteryStatus: output status report (alerts & watches)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryStatus(int verbosity, OvmsWriter* writer, int pack)
{
  int capacity = verbosity;
  const char *em;
  int c, val;

  // Voltage deviations:
  capacity -= writer->printf("Volts: ");

  // standard deviation:
  val = CONV_CellVolt(twizy_batt[pack].cell_volt_stddev_max);
  if (twizy_batt[pack].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
    em = "!";
  else if (twizy_batt[pack].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dmV ", em, val);

  if ((twizy_batt[pack].volt_alerts.none()) && (twizy_batt[pack].volt_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cell_count; c++)
    {
      // check length:
      if (capacity < 12)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[pack].volt_alerts.test(c))
        em = "!";
      else if (twizy_batt[pack].volt_watches.test(c))
        em = "?";
      else
        continue;

      val = CONV_CellVoltS(twizy_cell[c].volt_maxdev);

      capacity -= writer->printf("%sC%d:%+dmV ", em, c+1, val);
    }
  }

  // check length:
  if (capacity < 20)
  {
    writer->puts("...");
    return;
  }

  // Temperature deviations:
  capacity -= writer->printf("Temps: ");

  // standard deviation:
  val = twizy_batt[pack].cmod_temp_stddev_max;
  if (twizy_batt[pack].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
    em = "!";
  else if (twizy_batt[pack].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
    em = "?";
  else
    em = "";
  capacity -= writer->printf("%sSD:%dC ", em, val);
  
  if ((twizy_batt[pack].temp_alerts.none()) && (twizy_batt[pack].temp_watches.none()))
  {
    capacity -= writer->printf("OK ");
  }
  else
  {
    for (c = 0; c < batt_cmod_count; c++)
    {
      // check length:
      if (capacity < 8)
      {
        writer->puts("...");
        return;
      }

      // Alert / Watch?
      if (twizy_batt[pack].temp_alerts.test(c))
        em = "!";
      else if (twizy_batt[pack].temp_watches.test(c))
        em = "?";
      else
        continue;

      val = twizy_cmod[c].temp_maxdev;

      capacity -= writer->printf("%sM%d:%+dC ", em, c+1, val);
    }
  }
  
  writer->puts("");
}


/**
 * FormatBatteryVolts: output voltage report (absolute / deviations)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryVolts(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;
  
  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    // output capacity reached?
    if (capacity < 13)
      break;
    
    if (show_deviations)
    {
      if (twizy_batt[p].volt_alerts.test(BATT_STDDEV_VOLT_FLAG))
        em = "!";
      else if (twizy_batt[p].volt_watches.test(BATT_STDDEV_VOLT_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dmV ", em, CONV_CellVolt(twizy_batt[p].cell_volt_stddev_max));
    }
    else
    {
      capacity -= writer->printf("P:%.2fV ", (float) CONV_PackVolt(twizy_batt[p].volt_act) / 100);
    }
  }

  // Output cell status:
  for (int c = 0; c < batt_cell_count; c++)
  {
    int p = 0; // fixed for now…
    
    // output capacity reached?
    if (capacity < 13)
      break;

    // Alert?
    if (twizy_batt[p].volt_alerts.test(c))
      em = "!";
    else if (twizy_batt[p].volt_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dmV ", em, c+1, CONV_CellVoltS(twizy_cell[c].volt_maxdev));
    else
      capacity -= writer->printf("%s%d:%.3fV ", em, c+1, (float) CONV_CellVolt(twizy_cell[c].volt_act) / 1000);
  }
  
  writer->puts("");
}


/**
 * FormatBatteryTemps: output temperature report (absolute / deviations)
 */
void OvmsVehicleRenaultTwizy::FormatBatteryTemps(int verbosity, OvmsWriter* writer, bool show_deviations)
{
  int capacity = verbosity;
  const char *em;
  
  // Output pack status:
  for (int p = 0; p < batt_pack_count; p++)
  {
    if (capacity < 17)
      break;
    
    if (show_deviations)
    {
      if (twizy_batt[p].temp_alerts.test(BATT_STDDEV_TEMP_FLAG))
        em = "!";
      else if (twizy_batt[p].temp_watches.test(BATT_STDDEV_TEMP_FLAG))
        em = "?";
      else
        em = "";
      capacity -= writer->printf("%sSD:%dC ", em, twizy_batt[0].cmod_temp_stddev_max);
    }
    else
    {
      capacity -= writer->printf("P:%dC (%dC..%dC) ",
        CONV_Temp(twizy_batt[p].temp_act),
        CONV_Temp(twizy_batt[p].temp_min),
        CONV_Temp(twizy_batt[p].temp_max));
    }
  }

  // Output cmod status:
  for (int c = 0; c < batt_cmod_count; c++)
  {
    int p = 0; // fixed for now…
    
    if (capacity < 8)
      break;
    
    // Alert?
    if (twizy_batt[p].temp_alerts.test(c))
      em = "!";
    else if (twizy_batt[p].temp_watches.test(c))
      em = "?";
    else
      em = "";

    if (show_deviations)
      capacity -= writer->printf("%s%d:%+dC ", em, c+1, twizy_cmod[c].temp_maxdev);
    else
      capacity -= writer->printf("%s%d:%dC ", em, c+1, CONV_Temp(twizy_cmod[c].temp_act));
  }

  writer->puts("");
}


/**
 * FormatPackData: output RT-BAT-P record
 */
void OvmsVehicleRenaultTwizy::FormatPackData(int verbosity, OvmsWriter* writer, int pack)
{
  if (verbosity < 200)
    return;
  
  int volt_alert, temp_alert;

  if (twizy_batt[pack].volt_alerts.any())
    volt_alert = 3;
  else if (twizy_batt[pack].volt_watches.any())
    volt_alert = 2;
  else
    volt_alert = 1;

  if (twizy_batt[pack].temp_alerts.any())
    temp_alert = 3;
  else if (twizy_batt[pack].temp_watches.any())
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output pack status:
  //  RT-BAT-P,0,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>
  //  ,<soc>,<soc_min>,<soc_max>
  //  ,<volt_act>,<volt_min>,<volt_max>
  //  ,<temp_act>,<temp_min>,<temp_max>
  //  ,<cell_volt_stddev_max>,<cmod_temp_stddev_max>
  //  ,<max_drive_pwr>,<max_recup_pwr>
  
  writer->printf(
    "RT-BAT-P,%d,86400"
    ",%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d,%d"
    ",%d,%d"
    ",%d,%d\n",
    pack+1,
    volt_alert, temp_alert,
    twizy_soc, twizy_soc_min, twizy_soc_max,
    twizy_batt[pack].volt_act,
    twizy_batt[pack].volt_min,
    twizy_batt[pack].volt_max,
    CONV_Temp(twizy_batt[pack].temp_act),
    CONV_Temp(twizy_batt[pack].temp_min),
    CONV_Temp(twizy_batt[pack].temp_max),
    CONV_CellVolt(twizy_batt[pack].cell_volt_stddev_max),
    (int) (twizy_batt[pack].cmod_temp_stddev_max + 0.5),
    (int) twizy_batt[pack].max_drive_pwr * 5,
    (int) twizy_batt[pack].max_recup_pwr * 5);
    
}


/**
 * FormatCellData: output RT-BAT-C record
 */
void OvmsVehicleRenaultTwizy::FormatCellData(int verbosity, OvmsWriter* writer, int cell)
{
  if (verbosity < 200)
    return;
  
  int pack = 0; // currently fixed, TODO for addon packs: determine pack index for cell
  int volt_alert, temp_alert;

  if (twizy_batt[pack].volt_alerts.test(cell))
    volt_alert = 3;
  else if (twizy_batt[pack].volt_watches.test(cell))
    volt_alert = 2;
  else
    volt_alert = 1;

  if (twizy_batt[pack].temp_alerts.test(cell >> 1))
    temp_alert = 3;
  else if (twizy_batt[pack].temp_watches.test(cell >> 1))
    temp_alert = 2;
  else
    temp_alert = 1;

  // Output cell status:
  //  RT-BAT-C,<cellnr>,86400
  //  ,<volt_alertstatus>,<temp_alertstatus>,
  //  ,<volt_act>,<volt_min>,<volt_max>,<volt_maxdev>
  //  ,<temp_act>,<temp_min>,<temp_max>,<temp_maxdev>

  writer->printf(
    "RT-BAT-C,%d,86400"
    ",%d,%d"
    ",%d,%d,%d,%d"
    ",%d,%d,%d,%d\n",
    cell+1,
    volt_alert, temp_alert,
    CONV_CellVolt(twizy_cell[cell].volt_act),
    CONV_CellVolt(twizy_cell[cell].volt_min),
    CONV_CellVolt(twizy_cell[cell].volt_max),
    CONV_CellVoltS(twizy_cell[cell].volt_maxdev),
    CONV_Temp(twizy_cmod[cell >> 1].temp_act),
    CONV_Temp(twizy_cmod[cell >> 1].temp_min),
    CONV_Temp(twizy_cmod[cell >> 1].temp_max),
    (int) (twizy_cmod[cell >> 1].temp_maxdev + 0.5));
    
}


/**
 * BatterySendDataUpdate: send data notifications for modified packs & cells
 */
void OvmsVehicleRenaultTwizy::BatterySendDataUpdate(bool force)
{
  bool overall_modified = force |
    m_batt_cell_count->IsModifiedAndClear(m_modifier) |
    m_batt_cmod_count->IsModifiedAndClear(m_modifier);
  
  for (int pack=0; pack < batt_pack_count; pack++) {
    
    // if any alert/watch is modified, update all cells:
    overall_modified |=
      twizy_batt[pack].m_volt_alerts->IsModifiedAndClear(m_modifier) |
      twizy_batt[pack].m_volt_watches->IsModifiedAndClear(m_modifier) |
      twizy_batt[pack].m_temp_alerts->IsModifiedAndClear(m_modifier) |
      twizy_batt[pack].m_temp_watches->IsModifiedAndClear(m_modifier);
    
    bool pack_modified = overall_modified |
      StdMetrics.ms_v_bat_soc->IsModifiedAndClear(m_modifier) |
      m_batt_soc_min->IsModifiedAndClear(m_modifier) |
      m_batt_soc_max->IsModifiedAndClear(m_modifier) |
      StdMetrics.ms_v_bat_temp->IsModifiedAndClear(m_modifier) |
      StdMetrics.ms_v_bat_voltage->IsModifiedAndClear(m_modifier) |
      twizy_batt[pack].IsModified(m_modifier);
      
    if (pack_modified)
      MyNotify.NotifyCommandf("data", "xrt batt data-pack %d", pack+1);
    
  }
  
  for (int cell=0; cell < batt_cell_count; cell++) {
    
    bool cell_modified = overall_modified |
      twizy_cell[cell].IsModified(m_modifier) |
      twizy_cmod[cell>>1].IsModified(m_modifier);
    
    if (cell_modified)
      MyNotify.NotifyCommandf("data", "xrt batt data-cell %d", cell+1);
    
  }
  
}


