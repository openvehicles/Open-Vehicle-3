/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    0.1.0  Initial stub
;			- Most basic functionality is ported from V2.
;
;		 0.1.1  03-Dec-2017 - Geir Øyvind Vælidalo
;			- Added xks cells-command which prints out battery voltages
;
;		 0.1.2  03-Dec-2017 - Geir Øyvind Vælidalo
;			- Moved more ks-variables to metrics.
;
;		 0.1.3  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Added Low voltage DC-DC converter metrics
;
;		 0.1.4  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Added pilot duty cycle and proper charger temp from OBC.
;
;		 0.1.5  04-Dec-2017 - Geir Øyvind Vælidalo
;			- Fetch proper RPM from VMCU.
;
;		 0.1.6  06-Dec-2017 - Geir Øyvind Vælidalo
;			- Added some verbosity-handling in CELLS and TRIP. Plus other minor changes.
;
;		 0.1.7  07-Dec-2017 - Geir Øyvind Vælidalo
;			- All doors added (except trunk).
;			- Daylight led driving lights, low beam and highbeam added.
;
;		 0.1.8  08-Dec-2017 - Geir Øyvind Vælidalo
;			- Added charge speed in km/h, adjusted for temperature.
;
;		 0.1.9  09-Dec-2017 - Geir Øyvind Vælidalo
;			- Added reading of door lock.
;
;		 0.2.0  12-Dec-2017 - Geir Øyvind Vælidalo
;			- x.ks.BatteryCapacity-parameter renamed to x.ks.acp_cap_kwh to mimic Renault Twizy naming.
;
;		 0.2.1  12-Dec-2017 - Geir Øyvind Vælidalo
;			- Kia Soul EV requires OVMS GPS, since we don't know how to access the cars own GPS at the moment.
;
;		 0.2.2  15-Dec-2017 - Geir Øyvind Vælidalo
;			- Minor bugfixes after quick in-car test.
;
;		 0.2.3  17-Dec-2017 - Geir Øyvind Vælidalo
;			- Open trunk, lock/unlock doors and open chargeport + minor changes.
;
;		 0.2.4  18-Dec-2017 - Geir Øyvind Vælidalo
;			- Added xks sjb - command + RearDefogger, Leftindicator, right indicator.
;
;		 0.2.5  22-Dec-2017 - Geir Øyvind Vælidalo
;			- "Trunk"-button of keyfob opens up the chargeport
;			- Added temporary, developer-commands: sjb and bcm
;			- Added methods for controlling ACC-relay, IGN1-relay, IGN2-relay and start-relay.
;
;		 0.2.6  22-Dec-2017 - Geir Øyvind Vælidalo
;			- Electric park break service-command
;
;		 0.2.7  22-Dec-2017 - Geir Øyvind Vælidalo
;			- New config: xks.remote_charge_port - enables the possibility to open charge port from keyfob
;			- Renamed config-root from x.ks to xks
;
;		 0.2.8  22-Dec-2017 - Geir Øyvind Vælidalo
;			- Renamed xks to ks - Config, metric list and command.
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
;;
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

//TODO
//		- ProcessCommand and Set and GetFeature - See twizy-code.
//		- parkbreakservice is not working
//		- IGN1-, IGN2-, ACC-, Start-relay is working but turns on just for a second or so.
//		- Rear defogger works, but only as long as TesterPresent is sent.
//

#include "ovms_log.h"
static const char *TAG = "v-kiasoulev";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_kiasoulev.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"

#define VERSION "0.2.8"

static const OvmsVehicle::poll_pid_t vehicle_kiasoulev_polls[] =
  {
    { 0x7e2, 0, 	   VEHICLE_POLL_TYPE_OBDIIVEHICLE,  0x02, {  30,  30,  30 } }, 	// VIN
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x01, {  30,  10,  10 } }, 	// BMC Diag page 01
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, {  30,  30,  10 } }, 	// BMC Diag page 02
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x03, {  30,  30,  10 } }, 	// BMC Diag page 03
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x04, {  30,  30,  10 } }, 	// BMC Diag page 04
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x05, {  30,  30,  10 } },	// BMC Diag page 05
    { 0x794, 0x79c, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, {  30,  30,  10 } }, 	// OBC - On board charger
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x00, {  30,  10,  10 } }, 	// VMCU Shift-stick
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x02, {  30,  10,   0 } }, 	// VMCU Motor temp++
    { 0x7df, 0x7de, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x06, {  30,  10,   0 } }, 	// TMPS
    { 0x7c5, 0x7cd, VEHICLE_POLL_TYPE_OBDIIGROUP,  	0x01, {  30,  10,   0 } }, 	// LDC - Low voltage DC-DC
    { 0, 0, 0, 0, { 0, 0, 0 } }
  };

/**
 * Constructor for Kia Soul EV
 */
OvmsVehicleKiaSoulEv::OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Kia Soul EV v3.0 vehicle module");

  memset( m_vin, 0, sizeof(m_vin));
  memset( ks_battery_cell_voltage ,0, sizeof(ks_battery_cell_voltage));
  memset( ks_battery_module_temp, 0, sizeof(ks_battery_module_temp));
  memset( ks_tpms_id, 0, sizeof(ks_tpms_id));

  ks_obc_volt = 230;
  ks_battery_current = 0;

  ks_battery_cum_charge_current = 0;
  ks_battery_cum_discharge_current = 0;
  ks_battery_cum_charge = 0;
  ks_battery_cum_discharge = 0;
  ks_battery_cum_op_time = 0;

  ks_charge_bits.ChargingChademo = false;
  ks_charge_bits.ChargingJ1772 = false;
  ks_charge_bits.FanStatus = 0;

  ks_heatsink_temperature = 0;
  ks_battery_fan_feedback = 0;
  ks_aux_bat_ok = true;

  ks_send_can.id = 0;
  ks_send_can.status = 0;

  ks_openChargePort = false;

  memset( ks_send_can.byte, 0, sizeof(ks_send_can.byte));

  ks_maxrange = CFG_DEFAULT_MAXRANGE;

  // C-Bus
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  // M-Bus
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

  MyConfig.RegisterParam("ks", "Kia Soul EV", true, true);
  ConfigChanged(NULL);

  // init metrics:
  m_version = MyMetrics.InitString("ks.version", 0, VERSION " " __DATE__ " " __TIME__);
  m_b_cell_volt_max = MyMetrics.InitFloat("ks.b.cell.volt.max", 10, 0, Volts);
  m_b_cell_volt_min = MyMetrics.InitFloat("ks.b.cell.volt.min", 10, 0, Volts);
  m_b_cell_volt_max_no = MyMetrics.InitInt("ks.b.cell.volt.max.no", 10, 0);
  m_b_cell_volt_min_no = MyMetrics.InitInt("ks.b.cell.volt.min.no", 10, 0);
  m_b_cell_det_max = MyMetrics.InitFloat("ks.b.cell.det.max", 0, 0, Percentage);
  m_b_cell_det_min = MyMetrics.InitFloat("ks.b.cell.det.min", 0, 0, Percentage);
  m_b_cell_det_max_no = MyMetrics.InitInt("ks.b.cell.det.max.no", 10, 0);
  m_b_cell_det_min_no = MyMetrics.InitInt("ks.b.cell.det.min.no", 10, 0);
  m_c_power = MyMetrics.InitFloat("ks.c.power", 10, 0, kW);
  m_c_speed = MyMetrics.InitFloat("ks.c.speed", 10, 0, Kph);
  m_b_min_temperature = MyMetrics.InitInt("ks.b.min.temp", 10, 0, Celcius);
  m_b_max_temperature = MyMetrics.InitInt("ks.b.max.temp", 10, 0, Celcius);
  m_b_inlet_temperature = MyMetrics.InitInt("ks.b.inlet.temp", 10, 0, Celcius);
  m_b_heat_1_temperature = MyMetrics.InitInt("ks.b.heat1.temp", 10, 0, Celcius);
  m_b_heat_2_temperature = MyMetrics.InitInt("ks.b.heat2.temp", 10, 0, Celcius);

  m_ldc_out_voltage = MyMetrics.InitFloat("ks.ldc.out.volt", 10, 12, Volts);
  m_ldc_in_voltage = MyMetrics.InitFloat("ks.ldc.in.volt", 10, 12, Volts);
  m_ldc_out_current = MyMetrics.InitFloat("ks.ldc.out.amps", 10, 0, Amps);
  m_ldc_temperature = MyMetrics.InitFloat("ks.ldc.temp", 10, 0, Celcius);

  m_obc_pilot_duty = MyMetrics.InitFloat("ks.obc.pilot.duty", 10, 0, Percentage);

  m_v_env_lowbeam = MyMetrics.InitBool("ks.e.lowbeam", 10, 0);
  m_v_env_highbeam = MyMetrics.InitBool("ks.e.highbeam", 10, 0);

  m_b_cell_det_max->SetValue(0);
  m_b_cell_det_min->SetValue(0);

  StdMetrics.ms_v_bat_12v_voltage->SetValue(12.5, Volts);
  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);

  // init commands:
  cmd_xks = MyCommandApp.RegisterCommand("ks","Kia Soul EV",NULL,"",0,0,true);
  cmd_xks->RegisterCommand("trip","Show trip info", xks_trip, 0,0, false);
  cmd_xks->RegisterCommand("tpms","Tire pressure monitor", xks_tpms, 0,0, false);
  cmd_xks->RegisterCommand("cells","Cell voltages", xks_cells, 0,0, false);
  cmd_xks->RegisterCommand("aux","Aux battery", xks_aux, 0,0, false);
  cmd_xks->RegisterCommand("IGN1","IGN1 relay", xks_ign1, "<on/off><password>",1,1, false);
  cmd_xks->RegisterCommand("IGN2","IGN2 relay", xks_ign2, "<on/off><password>",1,1, false);
  cmd_xks->RegisterCommand("ACC","ACC relay", xks_acc_relay, "<on/off><password>",1,1, false);
  cmd_xks->RegisterCommand("START","Start relay", xks_start_relay, "<on/off><password>",1,1, false);

  MyCommandApp.RegisterCommand("trunk","Open trunk", CommandOpenTrunk, "<password>",1,1, false);
  MyCommandApp.RegisterCommand("chargeport","Open chargeport", CommandOpenChargePort, "<password>",1,1, false);
  MyCommandApp.RegisterCommand("parkbreakservice","Enable break pad service", CommandParkBreakService, "<on/off/off2>",1,1, false);

  // For test purposes
  MyCommandApp.RegisterCommand("sjb","Send command to SJB ECU", xks_sjb, "<b1><b2><b3>", 3,3, false);
  MyCommandApp.RegisterCommand("bcm","Send command to BCM ECU", xks_bcm, "<b1><b2><b3>", 3,3, false);

  PollSetPidList(m_can1,vehicle_kiasoulev_polls);
  PollSetState(0);

  // require GPS:
  MyEvents.SignalEvent("vehicle.require.gps", NULL);
  MyEvents.SignalEvent("vehicle.require.gpstime", NULL);
  }

/**
 * Destructor
 */
OvmsVehicleKiaSoulEv::~OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Shutdown Kia Soul EV vehicle module");

  // release GPS:
  MyEvents.SignalEvent("vehicle.release.gps", NULL);
  MyEvents.SignalEvent("vehicle.release.gpstime", NULL);
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleKiaSoulEv::ConfigChanged(OvmsConfigParam* param)
	{
  ESP_LOGD(TAG, "Kia Soul EV reload configuration");

  // Instances:
  // ks
  //	  cap_act_kwh			Battery capacity in wH (Default: 270000)
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  //  maxrange         	Maximum ideal range at 20 °C [km] (Default: 160)
  //  remote_charge_port					Use "trunk" button on keyfob to open charge port (Default: 1=enabled)
  ks_battery_capacity = (float)MyConfig.GetParamValueInt("ks", "cap_act_kwh", CGF_DEFAULT_BATTERY_CAPACITY);
  ks_key_fob_open_charge_port = (bool)MyConfig.GetParamValueBool("ks", "remote_charge_port", true);

  ks_maxrange = MyConfig.GetParamValueInt("ks", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (ks_maxrange <= 0)
    ks_maxrange = CFG_DEFAULT_MAXRANGE;

  *StdMetrics.ms_v_charge_limit_soc = (float) MyConfig.GetParamValueInt("ks", "suffsoc");
  *StdMetrics.ms_v_charge_limit_range = (float) MyConfig.GetParamValueInt("ks", "suffrange");
	}

/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleKiaSoulEv::vehicle_kiasoulev_car_on(bool isOn)
  {

  if (isOn && !StdMetrics.ms_v_env_on->AsBool())
    {
		// Car is ON
		StdMetrics.ms_v_env_on->SetValue(isOn);
		StdMetrics.ms_v_env_awake->SetValue(isOn);

		// Start trip, save current state
		ks_trip_start_odo = POS_ODO;	// ODO at Start of trip
		ks_start_cdc = CUM_DISCHARGE; // Register Cumulated discharge
		ks_start_cc = CUM_CHARGE; 		// Register Cumulated charge
		StdMetrics.ms_v_env_charging12v->SetValue( false );
    PollSetState(1);
    }
  else if(!isOn && StdMetrics.ms_v_env_on->AsBool())
    {
    // Car is OFF
  		StdMetrics.ms_v_env_on->SetValue( isOn );
  		StdMetrics.ms_v_env_awake->SetValue( isOn );
  		StdMetrics.ms_v_pos_speed->SetValue( 0 );
  	  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- ks_trip_start_odo );
  		StdMetrics.ms_v_env_charging12v->SetValue( false );
    }
  }

/**
 * Handles incoming CAN-frames on bus 1, the C-bus
 */
void OvmsVehicleKiaSoulEv::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
  	  case 0x018:
  	   {
       // Doors status Byte 0, 4 & 5:
  	   StdMetrics.ms_v_door_chargeport->SetValue((d[0] & 0x01) > 0); 	//Byte 0 - Bit 0
  	   StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x10) > 0);					//Byte 0 - Bit 4
  	   StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x80) > 0);					//Byte 0 - Bit 7
  	   StdMetrics.ms_v_door_rl->SetValue((d[4] & 0x08) > 0);					//Byte 4 - Bit 3
  	   StdMetrics.ms_v_door_rr->SetValue((d[4] & 0x02) > 0); 					//Byte 4 - Bit 1

  	   StdMetrics.ms_v_door_trunk->SetValue((d[5] & 0x80) > 0); 			//Byte 5 - Bit 7

  	   // Light status Byte 2,4 & 5
  	   m_v_env_lowbeam->SetValue( (d[2] & 0x01)>0 );		//Byte 2 - Bit 0 - Low beam
  	   m_v_env_highbeam->SetValue( (d[2] & 0x02)>0 );		//Byte 2 - Bit 1 - High beam
  	   StdMetrics.ms_v_env_headlights->SetValue( (d[4] & 0x01)>0 );		//Byte 4 - Bit 0 - Day lights

  	   //byte 5 - Bit 6 - Left indicator light
  	   //byte 5 - Bit 5 - Right indicator light

  	   // Seat belt status Byte 7
  	   }
     break;

  	  /*case 0x110:
  	   {
  	   StdMetrics.ms_v_env_headlights->SetValue( (d[3] & 0x80)>0 );
  	   }
     break;
*/
  	  case 0x120: //Locks
  	    {
			if( d[3] & 0x20)
				{
				StdMetrics.ms_v_env_locked->SetValue(false); //Todo This only keeps track of the lock signal from keyfob
				}
			else if( d[3] & 0x10 )
				{
				StdMetrics.ms_v_env_locked->SetValue(true); //Todo This only keeps track of the lock signal from keyfob
				}
			if( d[3] & 0x40 && ks_key_fob_open_charge_port )
				{
				ks_openChargePort = true;
				}
  	    }
  	    break;

      case 0x200:
        {
          // Estimated range:
          //TODO   float ekstraRange = d[0]/10.0;  //Ekstra range when heating is off.
          uint16_t estRange = d[2] + ((d[1] & 1)<<9);
          if (estRange > 0){
        	    StdMetrics.ms_v_bat_range_est->SetValue((float)(estRange<<1), Kilometers );
          }
        }
        break;

      case 0x433:
        {
        // Parking brake status
        StdMetrics.ms_v_env_handbrake->SetValue((d[2] & 0x08) > 0); //Why did I have 0x10 here??
        }
        break;

      //case 0x4b0:
      //  {
        // Motor RPM based on wheel rotation
        //int rpm = (d[0]+(d[1]<<8)) * 8.206;
        //StdMetrics.ms_v_mot_rpm->SetValue( rpm );
      //  }
      //  break;

      case 0x4f0:
        {
          // Odometer:
          if (/*length = 8 &&*/ (d[7] || d[6] || d[5])) { //TODO can_datalength was needed in V2
            StdMetrics.ms_v_pos_odometer->SetValue(
            		(float)((d[7]<<16) + (d[6]<<8) + d[5])/10.0,
								Kilometers);
          }
        }
        break;

      case 0x4f2:
      		{
      		// Speed:
      		StdMetrics.ms_v_pos_speed->SetValue( d[1] >> 1, Kph ); // kph

      		// 4f2 is one of the few can-messages that are sent while car is both on and off.
      		// Byte 2 and 7 are some sort of counters which runs while the car is on.
      		if (d[2] > 0 || d[7] > 0)
      			{
      			vehicle_kiasoulev_car_on(true);
      			}
      		else if (d[2] == 0 && d[7] == 0 && StdMetrics.ms_v_pos_speed->AsFloat(Kph) == 0 && StdMetrics.ms_v_env_handbrake->AsBool())
      			{
           // Boths 2 and 7 and speed is 0, so we assumes the car is off.
      	     vehicle_kiasoulev_car_on(false);
      			}
        }
        break;

      case 0x542:
        {
        // SOC:
        if (d[0] > 0)
        	  StdMetrics.ms_v_bat_soc->SetValue( d[0] >> 1 ); //In V2 car_SOC
        }
        break;

      case 0x581:
        {
        // Car is CHARGING:
        StdMetrics.ms_v_bat_power->SetValue((float)(((uint16_t) d[7] << 8) | d[6])/256.0, kW);
        }
        break;

      case 0x594:
        {
          // BMS SOC:
          uint16_t val = (uint16_t) d[5] * 5 + d[6];
          if (val > 0){
            ks_bms_soc = (uint8_t) (val / 10);
          }
        }
        break;

      case 0x653:
        {
        // AMBIENT TEMPERATURE:
        StdMetrics.ms_v_env_temp->SetValue( TO_CELCIUS(d[5]/2.0), Celcius);
        }
        break;

      case 0x779:
      	 {
   		 ESP_LOGD(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      	 }
      	 break;
    }

  	// Check if response is from synchronous can message
	if(p_frame->MsgID==(ks_send_can.id+0x08) && ks_send_can.status==0xff)
		{
		//Store message bytes so that the async method can continue
		ks_send_can.status=3;

		ks_send_can.byte[0]=d[0];
		ks_send_can.byte[1]=d[1];
		ks_send_can.byte[2]=d[2];
		ks_send_can.byte[3]=d[3];
		ks_send_can.byte[4]=d[4];
		ks_send_can.byte[5]=d[5];
		ks_send_can.byte[6]=d[6];
		ks_send_can.byte[7]=d[7];
		}
  }

/**
 * Handle incoming CAN-frames on bus 2, the M-bus
 */
void OvmsVehicleKiaSoulEv::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

/**
 * Handle incoming messages from TPMS poll.
 */
void OvmsVehicleKiaSoulEv::IncomingTPMS(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	uint8_t bVal;
	uint32_t lVal;

	switch (pid)
		{
		case 0x06:
			if (m_poll_ml_frame == 0)
				{
				lVal = CAN_UINT32(4);
				SET_TPMS_ID(0, lVal);
				}
			else if (m_poll_ml_frame == 1)
				{
				bVal = CAN_BYTE(0);
				if (bVal > 0) StdMetrics.ms_v_tpms_fl_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_fl_t->SetValue( TO_CELCIUS(CAN_BYTE(1)), Celcius);
				lVal = (ks_tpms_id[1] & 0x000000ff) | (CAN_UINT32(4) & 0xffffff00);
				SET_TPMS_ID(1, lVal);
				}
			else if (m_poll_ml_frame == 2)
				{
				lVal = (uint32_t) CAN_BYTE(0) | (ks_tpms_id[1] & 0xffffff00);
				SET_TPMS_ID(1, lVal);
				bVal = CAN_BYTE(1);
				if (bVal > 0) StdMetrics.ms_v_tpms_fr_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_fr_t->SetValue( TO_CELCIUS(CAN_BYTE(2)), Celcius);
				lVal = (ks_tpms_id[2] & 0x0000ffff) | (CAN_UINT32(5) & 0xffff0000);
				SET_TPMS_ID(2, lVal);

				}
			else if (m_poll_ml_frame == 3)
				{
				lVal = ((uint32_t) CAN_UINT(0)) | (ks_tpms_id[2] & 0xffff0000);
				SET_TPMS_ID(2, lVal);
				bVal = CAN_BYTE(2);
				if (bVal > 0) StdMetrics.ms_v_tpms_rl_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_rl_t->SetValue( TO_CELCIUS(CAN_BYTE(3)), Celcius);
				lVal = (ks_tpms_id[3] & 0x00ffffff) | ((uint32_t) CAN_BYTE(6) << 24);
				SET_TPMS_ID(3, lVal);

			} else if (m_poll_ml_frame == 4) {
				lVal = (CAN_UINT24(0)) | (ks_tpms_id[3] & 0xff000000);
				SET_TPMS_ID(3, lVal);
				bVal = CAN_BYTE(3);
				if (bVal > 0) StdMetrics.ms_v_tpms_rr_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_rr_t->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
			}
			break;
	}
	}

/**
 * Handle incoming messages from On Board Charger-poll
 */
void OvmsVehicleKiaSoulEv::IncomingOBC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x02:
			if (m_poll_ml_frame == 1)
				{
				ks_obc_volt = (float) CAN_UINT(2) / 10.0;
				//} else if (vehicle_poll_ml_frame == 2) {
				//ks_obc_ampere = ((UINT) can_databuffer[4 + CAN_ADJ] << 8)
				//        | (UINT) can_databuffer[5 + CAN_ADJ];
				}
			else if (m_poll_ml_frame == 2)
				{
				m_obc_pilot_duty->SetValue( (float) CAN_BYTE(6) / 3.0 );
				}
			else if (m_poll_ml_frame == 3)
				{
				StdMetrics.ms_v_charge_temp->SetValue( (float) (CAN_BYTE(0)+CAN_BYTE(1)+CAN_BYTE(2))/3, Celcius );
				}
			break;
		}
	}

/**
 * Handle incoming messages from VMCU-poll
 */
void OvmsVehicleKiaSoulEv::IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	UINT base;
	uint8_t bVal;

	switch (pid)
		{
		case 0x00:
			if (m_poll_ml_frame == 1)
				{
				ks_shift_bits.value = CAN_BYTE(3);
				}
			break;

		case 0x02:
			// VIN (multi-line response):
			// VIN length is 20 on Kia => skip first frame (3 bytes):
			if (m_poll_ml_frame > 0 && type == VEHICLE_POLL_TYPE_OBDIIVEHICLE)
				{
				base = m_poll_ml_offset - length - 3;
				for (bVal = 0; (bVal < length) && ((base + bVal)<(sizeof (m_vin) - 1)); bVal++)
					m_vin[base + bVal] = CAN_BYTE(bVal);
				if (m_poll_ml_remain == 0) m_vin[base + bVal] = 0;
				}
			if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
				{
				if (m_poll_ml_frame == 1)
					{
					StdMetrics.ms_v_mot_rpm->SetValue( (CAN_BYTE(5)<<8) | CAN_BYTE(6) );
					}
				else if (m_poll_ml_frame == 3)
					{
					StdMetrics.ms_v_mot_temp->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
					StdMetrics.ms_v_inv_temp->SetValue( TO_CELCIUS(CAN_BYTE(5)), Celcius );
					}
				}
			break;
		}
	}

/**
 * Handle incoming messages from BMC-poll
 */
void OvmsVehicleKiaSoulEv::IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	UINT base;
	uint8_t bVal;

	switch (pid)
		{
		case 0x01:
			// diag page 01: skip first frame (no data)
			if (m_poll_ml_frame > 0) {
				if (m_poll_ml_frame == 1) // 02 21 01 - 21
					{
					m_c_power->SetValue( (float)CAN_UINT(1)/100.0, kW);
					//ks_battery_avail_discharge = (UINT8)(((UINT) can_databuffer[5 + CAN_ADJ]
					//        | ((UINT) can_databuffer[4 + CAN_ADJ] << 8))>>2);
					bVal = CAN_BYTE(5);
					StdMetrics.ms_v_charge_pilot->SetValue((bVal & 0x80) > 0);
					ks_charge_bits.ChargingChademo = ((bVal & 0x40) > 0);
					ks_charge_bits.ChargingJ1772 = ((bVal & 0x20) > 0);
					ks_battery_current = (ks_battery_current & 0x00FF)
									| ((UINT) CAN_BYTE(6) << 8);
					}
				else if (m_poll_ml_frame == 2) // 02 21 01 - 22
					{
					ks_battery_current = (ks_battery_current & 0xFF00) | (UINT) CAN_BYTE(0);
					StdMetrics.ms_v_bat_current->SetValue((float)ks_battery_current/10.0, Amps);
					StdMetrics.ms_v_bat_voltage->SetValue((float)CAN_UINT(1)/10.0, Volts);
					//StdMetrics.ms_v_bat_power->SetValue(
					//		StdMetrics.ms_v_bat_voltage->AsFloat(Volts) * StdMetrics.ms_v_bat_current->AsFloat(Amps) / 1000.0 , kW);
					ks_battery_module_temp[0] = CAN_BYTE(3);
					ks_battery_module_temp[1] = CAN_BYTE(4);
					ks_battery_module_temp[2] = CAN_BYTE(5);
					ks_battery_module_temp[3] = CAN_BYTE(6);

					}
				else if (m_poll_ml_frame == 3) // 02 21 01 - 23
					{
					ks_battery_module_temp[4] = CAN_BYTE(0);
					ks_battery_module_temp[5] = CAN_BYTE(1);
					ks_battery_module_temp[6] = CAN_BYTE(2);
					ks_battery_module_temp[7] = CAN_BYTE(3);
					//TODO What about the 30kWh-version?

					m_b_cell_volt_max->SetValue((float)CAN_BYTE(5)/50.0, Volts);
					m_b_cell_volt_max_no->SetValue(CAN_BYTE(6));

					}
				else if (m_poll_ml_frame == 4) // 02 21 01 - 24
					{
					m_b_cell_volt_min->SetValue((float)CAN_BYTE(0)/50.0, Volts);
					m_b_cell_volt_min_no->SetValue(CAN_BYTE(1));
					ks_battery_fan_feedback = CAN_BYTE(2);
					ks_charge_bits.FanStatus = CAN_BYTE(3) & 0xF;
					StdMetrics.ms_v_bat_12v_voltage->SetValue ((float)CAN_BYTE(4)/10.0 , Volts);
					ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0x0000FFFF) | ((uint32_t) CAN_UINT(5) << 16);

					}
				else if (m_poll_ml_frame == 5) // 02 21 01 - 25
					{
					ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0xFFFF0000) | ((uint32_t) CAN_UINT(0));
					ks_battery_cum_discharge_current = CAN_UINT32(2);
					ks_battery_cum_charge = (ks_battery_cum_charge & 0x00FFFFFF) | ((uint32_t) CAN_BYTE(6)) << 24;
					}
				else if (m_poll_ml_frame == 6) // 02 21 01 - 26
					{
					ks_battery_cum_charge = (ks_battery_cum_charge & 0xFF000000) | ((uint32_t) CAN_UINT24(0));
					ks_battery_cum_discharge = CAN_UINT32(3);
					}
				else if (m_poll_ml_frame == 7) // 02 21 01 - 27
					{
					ks_battery_cum_op_time = CAN_UINT32(0) / 3600;
					}
				}
			break;

		case 0x02: //Cell voltages
		case 0x03:
		case 0x04:
			// diag page 02-04: skip first frame (no data)
			base = ((pid-2)<<5) + m_poll_ml_offset - (length - 3);
			for (bVal = 0; bVal < length && ((base + bVal)<sizeof (ks_battery_cell_voltage)); bVal++)
				ks_battery_cell_voltage[base + bVal] = CAN_BYTE(bVal);
			break;

		case 0x05:
			if (m_poll_ml_frame == 1)
				{
				//TODO Untested.
				base = ((pid-2)<<5) + m_poll_ml_offset - (length - 3);
				for (bVal = 0; bVal < length && ((base + bVal)<sizeof (ks_battery_cell_voltage)); bVal++)
					ks_battery_cell_voltage[base + bVal] = CAN_BYTE(bVal);

				m_b_inlet_temperature->SetValue( CAN_BYTE(5) );
				m_b_min_temperature->SetValue( CAN_BYTE(6) );
				}
			else if (m_poll_ml_frame == 2)
				{
				m_b_max_temperature->SetValue( CAN_BYTE(0) );
				}
			else if (m_poll_ml_frame == 3)
				{
				//ks_air_bag_hwire_duty = can_databuffer[5 + CAN_ADJ];
				m_b_heat_1_temperature->SetValue( CAN_BYTE(5) );
				m_b_heat_2_temperature->SetValue( CAN_BYTE(6) );
				}
			else if (m_poll_ml_frame == 4)
				{
				m_b_cell_det_max->SetValue( (float)CAN_UINT(0)/10.0 );
				m_b_cell_det_max_no->SetValue( CAN_BYTE(2) );
				m_b_cell_det_min->SetValue( (float)CAN_UINT(3)/10.0 );
				m_b_cell_det_min_no->SetValue( CAN_BYTE(5) );
				}
			break;
		}
	}

/**
 * Handle incoming messages from LDC-poll
 */
/**
 * Hanlde incoming messages from LDC-poll
 */
void OvmsVehicleKiaSoulEv::IncomingLDC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x01:
			// 12V system
			ks_ldc_enabled = (CAN_BYTE(0) & 6) != 0;
			m_ldc_out_voltage->SetValue( CAN_BYTE(1) / 10.0 );
			m_ldc_in_voltage->SetValue( CAN_BYTE(3) * 2 );
			m_ldc_out_voltage->SetValue( CAN_BYTE(2) );
			m_ldc_temperature->SetValue( CAN_BYTE(4) - 100 );
			break;
		}
	}

/**
 * Incoming poll reply messages
 */
void OvmsVehicleKiaSoulEv::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
	switch (m_poll_moduleid_low)
		{
		// ****** TPMS ******
		case 0x7de:
			IncomingTPMS(bus, type, pid, data, length, mlremain);
			break;

		// ****** OBC ******
		case 0x79c:
			IncomingOBC(bus, type, pid, data, length, mlremain);
			break;

		// ******* VMCU ******
		case 0x7ea:
			IncomingVMCU(bus, type, pid, data, length, mlremain);
			break;

		// ***** BMC ****
		case 0x7ec:
			IncomingBMC(bus, type, pid, data, length, mlremain);
			break;

		// ***** LDC ****
		case 0x7cd:
			IncomingLDC(bus, type, pid, data, length, mlremain);
			break;

		default:
			ESP_LOGD(TAG, "Unknown module: %03x", m_poll_moduleid_low);
			break;
	  }
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleKiaSoulEv::Ticker1(uint32_t ticker)
	{
	// Open charge port if user demands it
	if( ks_openChargePort )
		{
		OpenChargePort(""); //TODO
		ks_openChargePort = false;
		}


	UpdateMaxRangeAndSOH();

	//Set VIN
	//TODO *StdMetrics.ms_v_vin = (string) m_vin;
	StandardMetrics.ms_v_vin->SetValue(m_vin);

	if (FULL_RANGE > 0) //  If we have the battery full range, we can calculate the ideal range too
		{
	  	StdMetrics.ms_v_bat_range_ideal->SetValue( FULL_RANGE * BAT_SOC / 100.0, Kilometers);
	  	}

	// Get battery temperature
	StdMetrics.ms_v_bat_temp->SetValue(((float) ks_battery_module_temp[0] + (float) ks_battery_module_temp[1] +
	          (float) ks_battery_module_temp[2] + (float) ks_battery_module_temp[3] +
	          (float) ks_battery_module_temp[4] + (float) ks_battery_module_temp[5] +
	          (float) ks_battery_module_temp[6] + (float) ks_battery_module_temp[7]) / 8, Celcius);

	// Update trip
	if (StdMetrics.ms_v_env_on->AsBool())
		{
		StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - ks_trip_start_odo , Kilometers);
		StdMetrics.ms_v_bat_energy_used->SetValue( (CUM_DISCHARGE - ks_start_cdc) - (CUM_CHARGE - ks_start_cc), kWh );
		StdMetrics.ms_v_bat_energy_recd->SetValue( CUM_CHARGE - ks_start_cc, kWh );
	  }

	//Keep charging metrics up to date
	if (ks_charge_bits.ChargingJ1772)  				// **** J1772 - Type 1  charging ****
		{
		SetChargeMetrics(ks_obc_volt, StdMetrics.ms_v_bat_power->AsFloat(0, kW) * 1000 / ks_obc_volt, 6600 / ks_obc_volt, false);
	  }
	else if (ks_charge_bits.ChargingChademo)  // **** ChaDeMo charging ****
		{
		SetChargeMetrics(StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), -ks_battery_current / 10.0, m_c_power->AsFloat(0,kW) * 10 / StdMetrics.ms_v_bat_voltage->AsFloat(400,Volts), true);
	  }

	//
	// Check for charging status changes:
	bool isCharging = StdMetrics.ms_v_charge_pilot->AsBool()
			  						&& (ks_charge_bits.ChargingChademo || ks_charge_bits.ChargingJ1772)
									&& (CHARGE_CURRENT > 0);

	if (isCharging)
		{

		if (!StdMetrics.ms_v_charge_inprogress->AsBool() )
	  		{
		  ESP_LOGI(TAG, "Charging starting");
	    // ******* Charging started: **********
	    StdMetrics.ms_v_charge_duration_full->SetValue( 1440, Minutes ); // Lets assume 24H to full.
  			SET_CHARGE_STATE("charging");
			StdMetrics.ms_v_charge_kwh->SetValue( 0, kWh );  // kWh charged
			ks_cum_charge_start = CUM_CHARGE; // Battery charge base point
			StdMetrics.ms_v_charge_inprogress->SetValue( true );
			StdMetrics.ms_v_env_charging12v->SetValue( true);

			PollSetState(2);

      // Send charge alert:
      RequestNotify(SEND_ChargeState);
			//TODO ks_sms_bits.NotifyCharge = 1; // Send SMS when charging is fully initiated
	    }
	  else
	  		{
		  ESP_LOGI(TAG, "Charging...");
	    // ******* Charging continues: *******
	    if (((BAT_SOC > 0) && (LIMIT_SOC > 0) && (BAT_SOC >= LIMIT_SOC) && (ks_last_soc < LIMIT_SOC))
	    			|| ((EST_RANGE > 0) && (LIMIT_RANGE > 0)
	    					&& (IDEAL_RANGE >= LIMIT_RANGE )
								&& (ks_last_ideal_range < LIMIT_RANGE )))
	    		{
	      // ...enter state 2=topping off when we've reach the needed range / SOC:
	  			SET_CHARGE_STATE("topoff");
	      }
	    else if (BAT_SOC >= 95) // ...else set "topping off" from 94% SOC:
	    		{
  				SET_CHARGE_STATE("topoff");
  	      // Send charge alert:
  	      RequestNotify(SEND_ChargeState);
	    		}
	  		}

	  // Check if we have what is needed to calculate remaining minutes
	  if (CHARGE_VOLTAGE > 0 && CHARGE_CURRENT > 0)
	  		{
	    	//Calculate remaining charge time
			float chargeTarget_full 	= ks_battery_capacity;
			float chargeTarget_soc 		= ks_battery_capacity;
			float chargeTarget_range 	= ks_battery_capacity;

			if (LIMIT_SOC > 0) //If SOC limit is set, lets calculate target battery capacity
				{
				chargeTarget_soc = ks_battery_capacity * LIMIT_SOC / 100.0;
				}
			else if (LIMIT_RANGE > 0)  //If range limit is set, lets calculate target battery capacity
				{
				chargeTarget_range = LIMIT_RANGE * ks_battery_capacity / FULL_RANGE;
				}

			if (ks_charge_bits.ChargingChademo)
				{ //ChaDeMo charging means that we will reach maximum 83%.
				chargeTarget_full = MIN(chargeTarget_full, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
				chargeTarget_soc = MIN(chargeTarget_soc, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
				chargeTarget_range = MIN(chargeTarget_range, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
				//TODO calculate the needed capacity above 83% as 32A
				}

			// Calculate time to full, SOC-limit and range-limit.
			StdMetrics.ms_v_charge_duration_full->SetValue( calcMinutesRemaining(chargeTarget_full), Minutes);
			StdMetrics.ms_v_charge_duration_soc->SetValue( calcMinutesRemaining(chargeTarget_soc), Minutes);
			StdMetrics.ms_v_charge_duration_range->SetValue( calcMinutesRemaining(chargeTarget_range), Minutes);

			//TODO if (ks_sms_bits.NotifyCharge == 1) //Send Charge SMS after we have initialized the voltage and current settings
			//  ks_sms_bits.NotifyCharge = 0;
      // Send charge alert:
      RequestNotify(SEND_ChargeState);
	    }
	  else
	  		{
  			SET_CHARGE_STATE("heating");
      // Send charge alert:
      RequestNotify(SEND_ChargeState);
	  		}
	  StdMetrics.ms_v_charge_kwh->SetValue(CUM_CHARGE - ks_cum_charge_start, kWh); // kWh charged
	  ks_last_soc = BAT_SOC;
	  ks_last_ideal_range = IDEAL_RANGE;

		}
	else if (!isCharging && StdMetrics.ms_v_charge_inprogress->AsBool())
		{
	  ESP_LOGI(TAG, "Charging done...");

	  // ** Charge completed or interrupted: **
		StdMetrics.ms_v_charge_current->SetValue( 0 );
	  StdMetrics.ms_v_charge_climit->SetValue( 0 );
	  if (BAT_SOC == 100)
	  		{
	  		SET_CHARGE_STATE("done");
	  		}
	  else
	  		{
			SET_CHARGE_STATE("stopped");
			}
		StdMetrics.ms_v_charge_substate->SetValue("onrequest");
	  StdMetrics.ms_v_charge_kwh->SetValue( CUM_CHARGE - ks_cum_charge_start, kWh );  // kWh charged

	  ks_cum_charge_start = 0;
	  StdMetrics.ms_v_charge_inprogress->SetValue( false );
		StdMetrics.ms_v_env_charging12v->SetValue( false );
		m_c_speed->SetValue(0);

    // Send charge alert:
    RequestNotify(SEND_ChargeState);
		}

	if(!isCharging && !StdMetrics.ms_v_env_on->AsBool())
		{
		PollSetState(0);
		}

	//Check aux battery and send alert
	if( StdMetrics.ms_v_bat_12v_voltage->AsFloat()<12.2 && ks_aux_bat_ok)
		{
		ks_aux_bat_ok = false;
		RequestNotify(SEND_AuxBattery_Low);
	  }
	else if ( StdMetrics.ms_v_bat_12v_voltage->AsFloat()>12.4 )
		{
		ks_aux_bat_ok = true;
		}

	DoNotify();
	}

/**
 *  Sets the charge metrics
 */
void OvmsVehicleKiaSoulEv::SetChargeMetrics(float voltage, float current, float climit, bool chademo)
	{
	StdMetrics.ms_v_charge_voltage->SetValue( voltage, Volts );
	StdMetrics.ms_v_charge_current->SetValue( current, Amps );
	StdMetrics.ms_v_charge_mode->SetValue( chademo ? "performance" : "standard");
	StdMetrics.ms_v_charge_climit->SetValue( climit, Amps);
	StdMetrics.ms_v_charge_type->SetValue( chademo ? "chademo" : "type1");
	StdMetrics.ms_v_charge_substate->SetValue("onrequest");

	//"Typical" consumption based on battery temperature and ambient temperature.
	float temp = (StdMetrics.ms_v_bat_temp->AsFloat(Celcius) + StdMetrics.ms_v_env_temp->AsFloat(Celcius))/2;
	float consumption = 15+(20-temp)*3.0/8.0; //kWh/100km
	m_c_speed->SetValue( (voltage * current) / (consumption*10), Kph);
	}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
uint16_t OvmsVehicleKiaSoulEv::calcMinutesRemaining(float target)
  		{
	  return MIN( 1440, (uint16_t) (((target - (ks_battery_capacity * BAT_SOC) / 100.0)*60.0) /
              (CHARGE_VOLTAGE * CHARGE_CURRENT)));
  		}

/**
 * Updates the maximum real world range at current temperature.
 * Also updates the State of Health
 */
void OvmsVehicleKiaSoulEv::UpdateMaxRangeAndSOH(void)
	{
	//Update State of Health using following assumption: 10% buffer
	StdMetrics.ms_v_bat_soh->SetValue( 110 - ( m_b_cell_det_max->AsFloat(0) + m_b_cell_det_min->AsFloat(0)) / 2 );
	StdMetrics.ms_v_bat_cac->SetValue( (ks_battery_capacity * BAT_SOH * BAT_SOC/10000.0) / 400, AmpHours);

	float maxRange = ks_maxrange * BAT_SOH / 100.0;
	float amb_temp = StdMetrics.ms_v_env_temp->AsFloat(20, Celcius);
	float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(20, Celcius);

	// Temperature compensation:
	//   - Assumes standard maxRange specified at 20 degrees C
	//   - Range halved at -20C.
	if (maxRange != 0)
		{
		maxRange = (maxRange * (100.0 - (int) (ABS(20.0 - (amb_temp+bat_temp)/2)* 1.25))) / 100.0;
		}
	StdMetrics.ms_v_bat_range_full->SetValue(maxRange, Kilometers);
	}

/**
 * Send a can message and wait for the response before continuing.
 * Time out after approx 0.5 second.
 */
bool OvmsVehicleKiaSoulEv::SendCanMessage_sync(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	uint16_t timeout;

	ks_send_can.id = id;
	ks_send_can.status = 0xff;

	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGD(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

	//Now wait for the response
	timeout = 50; // 50*50 = 2500 ms
	do
		{
		/* Block for 50ms. */
		vTaskDelay( xDelay );
		} while (ks_send_can.status == 0xff && --timeout>0);

	//Did we get the response?
	if(timeout != 0 )
		{
		if( ks_send_can.byte[1]==serviceId+0x40 )
			{
			return true;
			}
		else
			{
			ks_send_can.status = 0x2;
			}
		}
	else
		{
		ks_send_can.status = 0x1; //Timeout
		}
	return false;
 }

void OvmsVehicleKiaSoulEv::SendCanMessage(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
	{
	uint8_t data[] = {count, serviceId, b1, b2, b3, b4, b5, b6};
	m_can1->WriteStandard(id, 8, data);
	ESP_LOGD(TAG, "%03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
 }

/**
 * Send a can message to ECU to notify that tester is present
 */
void OvmsVehicleKiaSoulEv::SendTesterPresent(uint16_t id, uint8_t length)
	{
  return SendCanMessage(id, length,VEHICLE_POLL_TYPE_OBDII_TESTER_PRESENT, 0,0,0,0,0,0);
	}

/**
 * Send a can message to set ECU in a specific session mode
 */
bool OvmsVehicleKiaSoulEv::SetSessionMode(uint16_t id, uint8_t mode)
	{
  return SendCanMessage_sync(id, 2,VEHICLE_POLL_TYPE_OBDIISESSION, mode,0,0,0,0,0);
	}

/**
 * Put the car in proper session mode and then send the command
 */
bool OvmsVehicleKiaSoulEv::SendCommandInSessionMode(uint16_t id, uint8_t count, uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6 )
	{
	bool result = false;
	SendTesterPresent(id,2);
	vTaskDelay( xDelay );
	if( SetSessionMode(id, EXTENDED_DIAGNOSTIC_SESSION))
		{
		vTaskDelay( xDelay );
		SendTesterPresent(id,2);
		vTaskDelay( xDelay );
		result = SendCanMessage_sync(id, count, serviceId, b1, b2, b3, b4, b5, b6 );
		}
	SetSessionMode(id, DEFAULT_SESSION);
	return result;
	}

/**
 * Send command to Smart Junction Box
 * 771 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaSoulEv::Send_SJB_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(SMART_JUNCTION_BOX, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0 );
	}

/**
 * Send command to Body Control Module
 * 7a0 04 2F [b1] [b2] [b3]
 */
bool OvmsVehicleKiaSoulEv::Send_BCM_Command( uint8_t b1, uint8_t b2, uint8_t b3)
	{
	return SendCommandInSessionMode(BODY_CONTROL_MODULE, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b1, b2, b3, 0,0,0 );
	}

/**
 * Send command to Smart Key Unit
 * 7a5 [b1] 2F [b2] [b3] [b4] [b5] [b6] [b7]
 */
bool OvmsVehicleKiaSoulEv::Send_SMK_Command( uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4, uint8_t b5, uint8_t b6, uint8_t b7)
	{
	return SendCommandInSessionMode(SMART_KEY_UNIT, b1,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, b2, b3, b4, b5, b6, b7);
	}

/**
 * Send command to ABS & EBP Module
 * 7d5 03 30 [b1] [b2]
 */
bool OvmsVehicleKiaSoulEv::Send_EBP_Command( uint8_t b1, uint8_t b2)
	{
	return SendCommandInSessionMode(ABS_EBP_UNIT, 3,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID, b1, b2, 0,0,0,0 );
	}

/**
 * Open or lock the doors
 * 771 04 2F BC 1[0:1] 03
 */
bool OvmsVehicleKiaSoulEv::SetDoorLock(bool open, const char* password)
	{
  if( ks_shift_bits.Park )
  		{
    if( IsPasswordOk(password) )
    		{
    		return Send_SJB_Command(0xbc, open?0x11:0x10, 0x03);
    		}
  		}
		return false;
	}

/**
 * Open trunk door
 * 771 04 2F BC 09 03
 */
bool OvmsVehicleKiaSoulEv::OpenTrunk(const char* password)
	{
  if( ks_shift_bits.Park )
  		{
		if( IsPasswordOk(password) )
			{
  			return Send_SJB_Command(0xbc, 0x09, 0x03);
  			}
  		}
		return false;
	}

/**
 * Open charge port
 * 7a0 04 2F B0 61 03
 */
bool OvmsVehicleKiaSoulEv::OpenChargePort(const char* password)
	{
  if( ks_shift_bits.Park )
  		{
		if( IsPasswordOk(password) )
			{
			return Send_BCM_Command(0xb0, 0x61, 0x03);
  			}
  		}
		return false;
	}

/**
 * Turn on and off left indicator light
 * 771 04 2f bc 15 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::LeftIndicator(bool on)
	{
  if( ks_shift_bits.Park )
  		{
		return Send_SJB_Command(0xbc, 0x15, on?0x03:0x00);
  		}
		return false;
	}

/**
 * Turn on and off right indicator light
 * 771 04 2f bc 16 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::RightIndicator(bool on)
	{
  if( ks_shift_bits.Park )
  		{
		return Send_SJB_Command(0xbc, 0x16, on?0x03:0x00);
  		}
		return false;
	}

/**
 * Turn on and off rear defogger
 * Needs to send tester present or something... Turns of immediately.
 * 771 04 2f bc 0c 0[3:0]
 */
bool OvmsVehicleKiaSoulEv::RearDefogger(bool on)
	{
  if( ks_shift_bits.Park )
  		{
		return Send_SJB_Command(0xbc, 0x0c, on?0x03:0x00);
  		}
		return false;
	}

/**
 * ACC - relay
 */
bool OvmsVehicleKiaSoulEv::ACCRelay(bool on, const char* password)
	{
	if(IsPasswordOk(password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x08, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x08, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * IGN1 - relay
 */
bool OvmsVehicleKiaSoulEv::IGN1Relay(bool on, const char* password)
	{
	if(IsPasswordOk(password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x09, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x09, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * IGN2 - relay
 */
bool OvmsVehicleKiaSoulEv::IGN2Relay(bool on, const char* password)
	{
	if(IsPasswordOk(password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x0a, 0x03, 0x0a, 0x0a, 0x05);
				else return Send_SMK_Command(4, 0xb1, 0x0a, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * Start - relay
 */
bool OvmsVehicleKiaSoulEv::StartRelay(bool on, const char* password)
	{
	if(IsPasswordOk(password))
		{
		if( ks_shift_bits.Park )
				{
				if( on ) return Send_SMK_Command(7, 0xb1, 0x0b, 0x03, 0x02, 0x02, 0x01);
				else return Send_SMK_Command(4, 0xb1, 0x0b, 0, 0, 0, 0);
				}
		}
	return false;
	}

/**
 * Check if the password is ok
 */
bool OvmsVehicleKiaSoulEv::IsPasswordOk(const char *password)
  {
	return true;
	//TODO
  //std::string p = MyConfig.GetParamValue("password","module");
  //return ((p.empty())||(p.compare(password)==0));
  }


OvmsVehicle::vehicle_command_t OvmsVehicleKiaSoulEv::CommandLock(const char* pin)
  {
  return SetDoorLock(false,pin) ? Success:Fail;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleKiaSoulEv::CommandUnlock(const char* pin)
  {
  return SetDoorLock(true,pin) ? Success:Fail;
  }

/**
 * Command to open trunk
 */
void CommandOpenTrunk(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->OpenTrunk(argv[0]);
	}

/**
 * Command to open the charge port
 */
void CommandOpenChargePort(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->OpenChargePort(argv[0]);
	}

/**
 * Command to enable service mode on park breaks
 */
void CommandParkBreakService(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	if( strcmp(argv[0],"on")==0 )
		{
	  soul->Send_EBP_Command(0x02, 0x01);
		}
	else if( strcmp(argv[0],"off")==0 )
		{
	  soul->Send_EBP_Command(0x02, 0x03);
		}
	else if( strcmp(argv[0],"off2")==0 )
		{
	  soul->Send_EBP_Command(0x01, 0x01);
		}
	}

/**
 * Command to enable IGN1
 */
void xks_ign1(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->IGN1Relay( strcmp(argv[0],"on")==0, argv[1] );
	}

/**
 * Command to enable IGN2
 */
void xks_ign2(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->IGN2Relay( strcmp(argv[0],"on")==0, argv[1] );
	}


/**
 * Command to enable ACC relay
 */
void xks_acc_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->ACCRelay( strcmp(argv[0],"on")==0, argv[1] );
	}

/**
 * Command to enable start relay
 */
void xks_start_relay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->StartRelay( strcmp(argv[0],"on")==0, argv[1] );
	}

void xks_sjb(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->Send_SJB_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
	}

void xks_bcm(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->Send_BCM_Command(strtol(argv[0],NULL,16), strtol(argv[1],NULL,16), strtol(argv[2],NULL,16));
	}

/**
 * Print out the aux battery voltage.
 */
void xks_aux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  metric_unit_t rangeUnit = Native; // TODO: use user config if set

	const char* auxBatt = StdMetrics.ms_v_bat_12v_voltage->AsUnitString("-", rangeUnit, 2).c_str();

	writer->printf("AUX BATTERY\n");
	if (*auxBatt != '-') writer->printf("Aux battery voltage %s\n", auxBatt);
	}

/**
 * Print out the cell voltages.
 */
void xks_cells(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();

  metric_unit_t rangeUnit = Native; // TODO: use user config if set

	const char* minimum = soul->m_b_cell_volt_min->AsUnitString("-", rangeUnit, 2).c_str();
	const char* maximum = soul->m_b_cell_volt_max->AsUnitString("-", rangeUnit, 2).c_str();
	const char* total = StdMetrics.ms_v_bat_voltage->AsUnitString("-", rangeUnit, 2).c_str();
	const char* minDet = soul->m_b_cell_det_min->AsUnitString("-", rangeUnit, 2).c_str();
	const char* maxDet = soul->m_b_cell_det_max->AsUnitString("-", rangeUnit, 2).c_str();

	writer->printf("CELLS\n");
	if (*minimum != '-') writer->printf("Min %s #%d\n", minimum, soul->m_b_cell_volt_min_no->AsInt(0));
	if (*maximum != '-') writer->printf("Max %s #%d\n", maximum, soul->m_b_cell_volt_max_no->AsInt(0));
	if (*total != '-') writer->printf("Total %s\n", total);
	if (*minDet != '-') writer->printf("Min Det %s #%d\n", minDet, soul->m_b_cell_det_min_no->AsInt(0));
	if (*maxDet != '-') writer->printf("Max Det %s #%d\n", maxDet, soul->m_b_cell_det_max_no->AsInt(0));

	if(verbosity>788)
		{
		for (uint8_t i=0; i < sizeof (soul->ks_battery_cell_voltage); i++)
			{
			if( i % 10 == 0) writer->printf("\n%02d:",i+1);
			writer->printf("%.*fV ", 2, (float)soul->ks_battery_cell_voltage[i]/50.0);
			}
		writer->printf("\n");
		}
	else
		{
		uint8_t i, lines=(verbosity-80)/11;
		// Count each voltage and print out number of cells with that voltage.
		// Handles up to as many lines as verbosity allows. Hopefully it will be enough
		for( i=0;i<225; i++)
			{
			uint8_t cnt=0;
			for (uint8_t a=0; a < sizeof (soul->ks_battery_cell_voltage) && lines>0; a++)
				{
				if( soul->ks_battery_cell_voltage[a]==i) cnt++;
				}
			if(cnt>0)
				{
				writer->printf("%02d x %.*fV\n", cnt, 2, (float)i/50.0);
				lines--;
				}
			}
		}
	}

/**
 * Print out information of the tpms.
 */
void xks_tpms(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  metric_unit_t rangeUnit = Native; // TODO: use user config if set

  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("TPMS\n");
	// Front left
	const char* fl_pressure = StdMetrics.ms_v_tpms_fl_p->AsUnitString("-", rangeUnit, 1).c_str();
	const char* fl_temp = StdMetrics.ms_v_tpms_fl_t->AsUnitString("-", rangeUnit, 1).c_str();
	// Front right
	const char* fr_pressure = StdMetrics.ms_v_tpms_fr_p->AsUnitString("-", rangeUnit, 1).c_str();
	const char* fr_temp = StdMetrics.ms_v_tpms_fr_t->AsUnitString("-", rangeUnit, 1).c_str();
	// Rear left
	const char* rl_pressure = StdMetrics.ms_v_tpms_rl_p->AsUnitString("-", rangeUnit, 1).c_str();
	const char* rl_temp = StdMetrics.ms_v_tpms_rl_t->AsUnitString("-", rangeUnit, 1).c_str();
	// Rear right
	const char* rr_pressure = StdMetrics.ms_v_tpms_rr_p->AsUnitString("-", rangeUnit, 1).c_str();
	const char* rr_temp = StdMetrics.ms_v_tpms_rr_t->AsUnitString("-", rangeUnit, 1).c_str();

	if (*fl_pressure != '-')
    writer->printf("1 ID:%lu %s %s\n", soul->ks_tpms_id[0], fl_pressure, fl_temp);

  if (*fr_pressure != '-')
    writer->printf("2 ID:%lu %s %s\n",soul->ks_tpms_id[1], fr_pressure, fr_temp);

  if (*rl_pressure != '-')
    writer->printf("3 ID:%lu %s %s\n",soul->ks_tpms_id[2], rl_pressure, rl_temp);

  if (*rr_pressure != '-')
    writer->printf("4 ID:%lu %s %s\n",soul->ks_tpms_id[3], rr_pressure, rr_temp);
  }

/**
 * Print out information of the current trip.
 */
void xks_trip(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  metric_unit_t rangeUnit = Native; // TODO: use user config if set

  writer->printf("TRIP\n");

	// Trip distance
	const char* distance = StdMetrics.ms_v_pos_trip->AsUnitString("-", rangeUnit, 1).c_str();
  // Consumption
  float consumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) * 100 / StdMetrics.ms_v_pos_trip->AsFloat(Kilometers);
  float consumption2 = StdMetrics.ms_v_pos_trip->AsFloat(Kilometers) / StdMetrics.ms_v_bat_energy_used->AsFloat(kWh);
    // Discharge
  const char* discharge = StdMetrics.ms_v_bat_energy_used->AsUnitString("-", rangeUnit, 1).c_str();
  // Recuperation
  const char* recuparation = StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", rangeUnit, 1).c_str();
  // Total consumption
  float totalConsumption = StdMetrics.ms_v_bat_energy_used->AsFloat(kWh) + StdMetrics.ms_v_bat_energy_recd->AsFloat(kWh);
  // ODO
  const char* ODO = StdMetrics.ms_v_pos_odometer->AsUnitString("-", rangeUnit, 1).c_str();

  if (*distance != '-')
    writer->printf("Dist %s\n", distance);

  writer->printf("Con %.*fkWh/100km\n", 2, consumption);
  writer->printf("Con %.*fkm/kWh\n", 2, consumption2);

  if (*discharge != '-')
    writer->printf("Dis %s\n", discharge);

  if (*recuparation != '-')
    writer->printf("Rec %s\n", recuparation);

  writer->printf("Total %.*fkWh\n", 2, totalConsumption);

  if (*ODO != '-')
    writer->printf("ODO %s\n", ODO);
  }

/**
 * RequestNotify: send notifications / alerts / data updates
 */
void OvmsVehicleKiaSoulEv::RequestNotify(unsigned int which)
	{
  ks_notifications |= which;
	}

void OvmsVehicleKiaSoulEv::DoNotify()
	{
  unsigned int which = ks_notifications;

  if (which & SEND_ChargeState)
  		{
    MyNotify.NotifyCommand("info", "stat");
    ks_notifications &= ~SEND_ChargeState;
  		}

  if (which & SEND_AuxBattery_Low)
  		{
    MyNotify.NotifyCommand("alert", "xks aux");
    ks_notifications &= ~SEND_AuxBattery_Low;
  		}
	}


class OvmsVehicleKiaSoulEvInit
  {
  public: OvmsVehicleKiaSoulEvInit();
  } MyOvmsVehicleKiaSoulEvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleKiaSoulEvInit::OvmsVehicleKiaSoulEvInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Kia Soul EV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleKiaSoulEv>("KS","Kia Soul EV");
  }

