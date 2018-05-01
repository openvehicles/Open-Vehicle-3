/*
;    Project:       Open Vehicle Monitor System
;    Date:          29th December 2017
;
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kiasoulev.h"

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

	soul->	SendTesterPresent(ABS_EBP_UNIT,1);

	if( strcmp(argv[0],"on")==0 )
		{
	  //soul->Send_EBP_Command(0x02, 0x01, KS_90_DIAGNOSTIC_SESSION);
		if( soul->SetSessionMode(ABS_EBP_UNIT, KS_90_DIAGNOSTIC_SESSION ))
			{
			soul->SendCanMessage(ABS_EBP_UNIT, 3, VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_LOC_ID, 0x02, 0x01, 0, 0, 0, 0 );
			}
		}
	else if( strcmp(argv[0],"off")==0 )
		{
	  soul->Send_EBP_Command(0x02, 0x03, KS_90_DIAGNOSTIC_SESSION);
		}
	else if( strcmp(argv[0],"off2")==0 ) //Disengange 7d5h	8	03 30 01 01 00 00 00 00
		{
	  soul->Send_EBP_Command(0x01, 0x01, KS_90_DIAGNOSTIC_SESSION);
		}

	for(int i=0; i<100; i++)
		{
		soul->	SendTesterPresent(ABS_EBP_UNIT,2);
		vTaskDelay( 20 / portTICK_PERIOD_MS );
		}
	soul->SetSessionMode(ABS_EBP_UNIT, DEFAULT_SESSION);
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

void xks_set_head_light_delay(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->SetHeadLightDelay(strcmp(argv[0],"on")==0);
	}

void xks_set_one_touch_turn_signal(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->SetOneThouchTurnSignal(strtol(argv[0],NULL,10));
	}

void xks_set_auto_door_unlock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->SetAutoDoorUnlock(strtol(argv[0],NULL,10));
	}

void xks_set_auto_door_lock(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
	{
  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();
	soul->SetAutoDoorLock(strtol(argv[0],NULL,10));
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
 * Print out the VIN information.
 */
void xks_vin(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyVehicleFactory.m_currentvehicle==NULL)
    {
    writer->puts("Error: No vehicle module selected");
    return;
    }

  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("VIN\n");
	writer->printf("Vin: %s \n", soul->m_vin);
	if(soul->m_vin[2]=='A' || soul->m_vin[2]=='D')
		{
		writer->printf("MPV/SUV/RV (Outside USA, Canada, Mexico)\n");
		}
	else if(soul->m_vin[2]=='C')
		{
		writer->printf("Commercial Vehicle\n");
		}
	else if(soul->m_vin[2]=='D')
		{
		writer->printf("MPV/SUV/RV (USA, Canada, Mexico)\n");
		}
	else if(soul->m_vin[2]=='H')
		{
		writer->printf("Van\n");
		}

	writer->printf("Vehicle Line: ");
	if(soul->m_vin[3]=='J')
		{
		writer->printf("Soul\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[3]);
		}

	writer->printf("Model: ");
	if(soul->m_vin[4]=='M')
		{
		writer->printf("Low grade\n");
		}
	else if(soul->m_vin[4]=='N')
		{
		writer->printf("Middle-low grade\n");
		}
	else if(soul->m_vin[4]=='P')
		{
		writer->printf("Middle grade\n");
		}
	else if(soul->m_vin[4]=='R')
		{
		writer->printf("Middle-high grade\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[4]);
		}

	writer->printf("Model year: %04d\n", 2014 + soul->m_vin[9]-'E');

	writer->printf("Motor type: ");
	if(soul->m_vin[7]=='E')
		{
		writer->printf("Battery [LiPB 350 V, 75 Ah] + Motor [3-phase AC 80 KW]\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[7]);
		}

	writer->printf("Production plant: ");
	if(soul->m_vin[10]=='5')
		{
		writer->printf("Hwaseong (Korea)\n");
		}
	else if(soul->m_vin[10]=='6')
		{
		writer->printf("Soha-ri (Korea)\n");
		}
	else if(soul->m_vin[10]=='7')
		{
		writer->printf("Gwangju (Korea)\n");
		}
	else if(soul->m_vin[10]=='T')
		{
		writer->printf("Seosan (Korea)\n");
		}
	else
		{
		writer->printf("Unknown %c\n", soul->m_vin[10]);
		}

	writer->printf("Sequence number: %c%c%c%c%c%c\n", soul->m_vin[11],soul->m_vin[12],soul->m_vin[13],soul->m_vin[14],soul->m_vin[15],soul->m_vin[16]);
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

	const char* minimum = soul->m_b_cell_volt_min->AsUnitString("-", Volts, 2).c_str();
	const char* maximum = soul->m_b_cell_volt_max->AsUnitString("-", Volts, 2).c_str();
	const char* total = StdMetrics.ms_v_bat_voltage->AsUnitString("-", Volts, 2).c_str();
	const char* minDet = soul->m_b_cell_det_min->AsUnitString("-", Percentage, 2).c_str();
	const char* maxDet = soul->m_b_cell_det_max->AsUnitString("-", Percentage, 2).c_str();

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

  OvmsVehicleKiaSoulEv* soul = (OvmsVehicleKiaSoulEv*) MyVehicleFactory.ActiveVehicle();

	writer->printf("TPMS\n");
	// Front left
	const char* fl_pressure = StdMetrics.ms_v_tpms_fl_p->AsUnitString("-", kPa, 1).c_str();
	const char* fl_temp = StdMetrics.ms_v_tpms_fl_t->AsUnitString("-", Celcius, 1).c_str();
	// Front right
	const char* fr_pressure = StdMetrics.ms_v_tpms_fr_p->AsUnitString("-", kPa, 1).c_str();
	const char* fr_temp = StdMetrics.ms_v_tpms_fr_t->AsUnitString("-", Celcius, 1).c_str();
	// Rear left
	const char* rl_pressure = StdMetrics.ms_v_tpms_rl_p->AsUnitString("-", kPa, 1).c_str();
	const char* rl_temp = StdMetrics.ms_v_tpms_rl_t->AsUnitString("-", Celcius, 1).c_str();
	// Rear right
	const char* rr_pressure = StdMetrics.ms_v_tpms_rr_p->AsUnitString("-", kPa, 1).c_str();
	const char* rr_temp = StdMetrics.ms_v_tpms_rr_t->AsUnitString("-", Celcius, 1).c_str();

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
  const char* discharge = StdMetrics.ms_v_bat_energy_used->AsUnitString("-", kWh, 1).c_str();
  // Recuperation
  const char* recuparation = StdMetrics.ms_v_bat_energy_recd->AsUnitString("-", kWh, 1).c_str();
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

