/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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
static const char *TAG = "v-nissanleaf";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_nissanleaf.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

void remoteCommandTimer(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->RemoteCommandTimer();
  }

OvmsVehicleNissanLeaf::OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Nissan Leaf v3.0 vehicle module");

  m_gids = MyMetrics.InitInt("xnl.v.bat.gids", SM_STALE_HIGH, 0);
  m_hx = MyMetrics.InitFloat("xnl.v.bat.hx", SM_STALE_HIGH, 0);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  MyConfig.RegisterParam("xnl", "Nissan Leaf", true, true);
  ConfigChanged(NULL);

  m_remoteCommandTimer = xTimerCreate("Nissan Leaf Remote Command", 50 / portTICK_PERIOD_MS, pdTRUE, this, remoteCommandTimer);

  using std::placeholders::_1;
  using std::placeholders::_2;
  }

OvmsVehicleNissanLeaf::~OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Shutdown Nissan Leaf vehicle module");
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_charger_status()
// Takes care of setting all the charger state bit when the charger
// switches on or off. Separate from vehicle_nissanleaf_poll1() to make
// it clearer what is going on.
//

void vehicle_nissanleaf_charger_status(ChargerStatus status)
  {
  switch (status)
    {
    case CHARGER_STATUS_IDLE:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      break;
    case CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      break;
    case CHARGER_STATUS_QUICK_CHARGING:
    case CHARGER_STATUS_CHARGING:
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
        StandardMetrics.ms_v_charge_kwh->SetValue(0); // Reset charge kWh
        }
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");

      // TODO only use battery current for Quick Charging, for regular charging
      // we should return AC line current and voltage, not battery
      // TODO does the leaf know the AC line current and voltage?
      // TODO v3 supports negative values here, what happens if we send a negative charge current to a v2 client?
      if (StandardMetrics.ms_v_bat_current->AsFloat() < 0)
        {
        // battery current can go negative when climate control draws more than
        // is available from the charger. We're abusing the line current which
        // is unsigned, so don't underflow it
        //
        // TODO quick charging can draw current from the vehicle
        StandardMetrics.ms_v_charge_current->SetValue(0);
        }
      else
        {
        StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
        }
      StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
      break;
    case CHARGER_STATUS_FINISHED:
      // Charging finished
      StandardMetrics.ms_v_charge_current->SetValue(0);
      // TODO set this in ovms v2
      // TODO the charger probably knows the line voltage, when we find where it's
      // coded, don't zero it out when we're plugged in but not charging
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      break;
    }
  if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
    {
    StandardMetrics.ms_v_charge_current->SetValue(0);
    // TODO the charger probably knows the line voltage, when we find where it's
    // coded, don't zero it out when we're plugged in but not charging
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    }
  }

////////////////////////////////////////////////////////////////////////
// PollStart()
// Send the initial message to poll for data. Further data is requested
// in vehicle_nissanleaf_poll_continue() after the recept of each page.
//

void OvmsVehicleNissanLeaf::PollStart(void)
  {
  // Request Group 1
  uint8_t data[] = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  nl_poll_state = ZERO;
  m_can1->WriteStandard(0x79b, 8, data);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_poll_continue()
// Process a 0x7bb polling response and request the next page
//

void OvmsVehicleNissanLeaf::PollContinue(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  if (nl_poll_state == IDLE)
    {
    // we're not expecting anything, maybe something else is polling?
    return;
    }
  if ((d[0] & 0x0f) != nl_poll_state)
    {
    // not the page we were expecting, abort
    nl_poll_state = IDLE;
    return;
    }

  switch (nl_poll_state)
    {
    case IDLE:
      // this isn't possible due to the idle check above
      abort();
    case ZERO:
    case ONE:
    case TWO:
    case THREE:
      // TODO this might not be idomatic C++ but I want to keep the delta to
      // the v2 code small until the porting is finished
      nl_poll_state = static_cast<PollState>(static_cast<int>(nl_poll_state) + 1);
      break;
    case FOUR:
      {
        uint16_t hx;
        hx = d[2];
        hx = hx << 8;
        hx = hx | d[3];
        m_hx->SetValue(hx / 100.0);

        nl_poll_state = static_cast<PollState>(static_cast<int>(nl_poll_state) + 1);
        break;
      }
    case FIVE:
      {
        uint32_t ah10000;
        ah10000 = d[2];
        ah10000 = ah10000 << 8;
        ah10000 = ah10000 | d[3];
        ah10000 = ah10000 << 8;
        ah10000 = ah10000 | d[4];
        float ah = ah10000 / 10000.0;
        StandardMetrics.ms_v_bat_cac->SetValue(ah);

        // there may be a way to get the SoH directly from the BMS, but for now
        // divide by a configurable battery size
        float newCarAh = MyConfig.GetParamValueFloat("xnl", "newCarAh", GEN_1_NEW_CAR_AH);
        StandardMetrics.ms_v_bat_soh->SetValue(ah / newCarAh * 100);

        nl_poll_state = IDLE;
        break;
      }
    }
  if (nl_poll_state != IDLE)
    {
    // request the next page of data
    uint8_t next[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    m_can1->WriteStandard(0x79b, 8, next);
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x1db:
    {
      // sent by the LBC, measured inside the battery box
      // current is 11 bit twos complement big endian starting at bit 0
      int16_t nl_battery_current = ((int16_t) d[0] << 3) | (d[1] & 0xe0) >> 5;
      if (nl_battery_current & 0x0400)
        {
        // negative so extend the sign bit
        nl_battery_current |= 0xf800;
        }
      StandardMetrics.ms_v_bat_current->SetValue(nl_battery_current / 2.0f);

      // voltage is 10 bits unsigned big endian starting at bit 16
      int16_t nl_battery_voltage = ((uint16_t) d[2] << 2) | (d[3] & 0xc0) >> 6;
      StandardMetrics.ms_v_bat_voltage->SetValue(nl_battery_voltage / 2.0f);
    }
      break;
    case 0x284:
    {
      // certain CAN bus activity counts as state "awake"
      StandardMetrics.ms_v_env_awake->SetValue(true);

      uint16_t car_speed16 = d[4];
      car_speed16 = car_speed16 << 8;
      car_speed16 = car_speed16 | d[5];
      // this ratio determined by comparing with the dashboard speedometer
      // it is approximately correct and converts to km/h on my car with km/h speedo
      StandardMetrics.ms_v_pos_speed->SetValue(car_speed16 / 92);
    }
      break;
    case 0x390:
      // Gen 2 Charger
      //
      // When the data is valid, can_databuffer[6] is the J1772 maximum
      // available current if we're plugged in, and 0 when we're not.
      // can_databuffer[3] seems to govern when it's valid, and is probably a
      // bit field, but I don't have a full decoding
      //
      // specifically the last few frames before shutdown to wait for the charge
      // timer contain zero in can_databuffer[6] so we ignore them and use the
      // valid data in the earlier frames
      //
      // During plug in with charge timer activated, byte 3 & 6:
      //
      // 0x390 messages start
      // 0x00 0x21
      // 0x60 0x21
      // 0x60 0x00
      // 0xa0 0x00
      // 0xb0 0x00
      // 0xb2 0x15 -- byte 6 now contains valid j1772 pilot amps
      // 0xb3 0x15
      // 0xb1 0x00
      // 0x71 0x00
      // 0x69 0x00
      // 0x61 0x00
      // 0x390 messages stop
      //
      // byte 3 is 0xb3 during charge, and byte 6 contains the valid pilot amps
      //
      // so far, except briefly during startup, when byte 3 is 0x00 or 0x03,
      // byte 6 is 0x00, correctly indicating we're unplugged, so we use that
      // for unplugged detection.
      //
      if (d[3] == 0xb3 ||
        d[3] == 0x00 ||
        d[3] == 0x03)
        {
        // can_databuffer[6] is the J1772 pilot current, 0.5A per bit
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("type1");
        uint8_t current_limit = (d[6] + 1) / 2;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        StandardMetrics.ms_v_charge_pilot->SetValue(current_limit != 0);
        StandardMetrics.ms_v_door_chargeport->SetValue(current_limit != 0);
        }
      switch (d[5])
        {
        case 0x80:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x83:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x88:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x98:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        }
      break;
    case 0x54b:
    {
      bool hvac_candidate;
      // this might be a bit field? So far these 6 values indicate HVAC on
      hvac_candidate = 
        d[1] == 0x0a || // Gen 1 Remote
        d[1] == 0x48 || // Manual Heating or Fan Only
        d[1] == 0x4b || // Gen 2 Remote Heating
        d[1] == 0x71 || // Gen 2 Remote Cooling
        d[1] == 0x76 || // Auto
        d[1] == 0x78;   // Manual A/C on
      StandardMetrics.ms_v_env_hvac->SetValue(hvac_candidate);
      /* These may only reflect the centre console LEDs, which
       * means they don't change when remote CC is operating.
       * They should be replaced when we find better signals that
       * indicate when heating or cooling is actually occuring.
       */
      StandardMetrics.ms_v_env_cooling->SetValue(d[1] & 0x10);
      StandardMetrics.ms_v_env_heating->SetValue(d[1] & 0x02);
    }
      break;
    case 0x54c:
      /* Ambient temperature.  This one has half-degree C resolution,
       * and seems to stay within a degree or two of the "eyebrow" temp display.
       * App label: AMBIENT
       */
      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_temp->SetValue(d[6] / 2.0 - 40);
        }
      break;
    case 0x54f:
      /* Climate control's measurement of temperature inside the car.
       * Subtracting 14 is a bit of a guess worked out by observing how
       * auto climate control reacts when this reaches the target setting.
       */
      if (d[0] != 20)
        {
        StandardMetrics.ms_v_env_cabintemp->SetValue(d[0] / 2.0 - 14);
        }
      break;
    case 0x5bc:
    {
      uint16_t nl_gids = ((uint16_t) d[0] << 2) | ((d[1] & 0xc0) >> 6);
      if (nl_gids == 1023)
        {
        // ignore invalid data seen during startup
        break;
        }
      uint16_t max_gids = MyConfig.GetParamValueInt("xnl", "maxGids", GEN_1_NEW_CAR_GIDS);
      float km_per_kwh = MyConfig.GetParamValueFloat("xnl", "kmPerKWh", GEN_1_KM_PER_KWH);
      float wh_per_gid = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);

      m_gids->SetValue(nl_gids);
      StandardMetrics.ms_v_bat_soc->SetValue((nl_gids * 100.0) / max_gids);
      StandardMetrics.ms_v_bat_range_ideal->SetValue((nl_gids * wh_per_gid * km_per_kwh) / 1000);
    }
      break;
    case 0x5bf:
      if (d[4] == 0xb0)
        {
        // Quick Charging
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("chademo");
        StandardMetrics.ms_v_charge_climit->SetValue(120);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        }
      else
        {
        // Maybe J1772 is connected
        // can_databuffer[2] is the J1772 maximum available current, 0 if we're not plugged in
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("type1");
        uint8_t current_limit = d[2] / 5;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        StandardMetrics.ms_v_charge_pilot->SetValue(current_limit != 0);
        StandardMetrics.ms_v_door_chargeport->SetValue(current_limit != 0);
        }

      switch (d[4])
        {
        case 0x28:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0xb0: // Quick Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x60: // L2 Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        }
      break;
    case 0x5c0:
      /* Another "ambient" temperature, but this one reacts to outside changes
       * quite slowly.  Seems likely it is battery pack temperature, as the rest
       * of the packet is about charging.
       * Effectively has only 7-bit precision, as the bottom bit is always 0.
       */
      if ( (d[0]>>6) == 1 )
        {
        StandardMetrics.ms_v_bat_temp->SetValue(d[2] / 2 - 40);
        }
      break;
    case 0x7bb:
      PollContinue(p_frame);
      break;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x180:
      if (d[5] != 0xff)
        {
        StandardMetrics.ms_v_env_throttle->SetValue(d[5] / 2.00);
        }
      break;
    case 0x292:
      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_footbrake->SetValue(d[6] / 1.39);
        }
      break;
    case 0x385:
      // not sure if this order is correct
      if (d[2]) StandardMetrics.ms_v_tpms_fl_p->SetValue(d[2] / 4.0, PSI);
      if (d[3]) StandardMetrics.ms_v_tpms_fr_p->SetValue(d[3] / 4.0, PSI);
      if (d[4]) StandardMetrics.ms_v_tpms_rl_p->SetValue(d[4] / 4.0, PSI);
      if (d[5]) StandardMetrics.ms_v_tpms_rr_p->SetValue(d[5] / 4.0, PSI);
      break;
    case 0x421:
      switch ( (d[0] >> 3) & 7 )
        {
        case 0: // Parking
        case 1: // Park
        case 3: // Neutral
        case 5: // undefined
        case 6: // undefined
          StandardMetrics.ms_v_env_gear->SetValue(0);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          break;
        case 4: // Drive
        case 7: // Drive/B (ECO on some models)
          StandardMetrics.ms_v_env_gear->SetValue(1);
          break;
        }
      break;
    case 0x510:
      /* This seems to be outside temperature with half-degree C accuracy.
       * It reacts a bit more rapidly than what we get from the battery.
       * App label: PEM
       */
      if (d[7] != 0xff)
        {
        StandardMetrics.ms_v_inv_temp->SetValue(d[7] / 2.0 - 40);
        }
      break;
    case 0x5c5:
      // This is the parking brake (which is foot-operated on some models).
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0] & 4);
      StandardMetrics.ms_v_pos_odometer->SetValue(d[1] << 16 | d[2] << 8 | d[3]);
      break;
    case 0x60d:
      StandardMetrics.ms_v_door_trunk->SetValue(d[0] & 0x80);
      StandardMetrics.ms_v_door_rr->SetValue(d[0] & 0x40);
      StandardMetrics.ms_v_door_rl->SetValue(d[0] & 0x20);
      StandardMetrics.ms_v_door_fr->SetValue(d[0] & 0x10);
      StandardMetrics.ms_v_door_fl->SetValue(d[0] & 0x08);
      StandardMetrics.ms_v_env_headlights->SetValue((d[0] & 0x02) | // dip beam
                                                    (d[1] & 0x08)); // main beam
      /* d[1] bits 1 and 2 indicate Start button states:
       *   No brake:   off -> accessory -> on -> off
       *   With brake: [off, accessory, on] -> ready to drive -> off
       * Using "ready to drive" state to set ms_v_env_on seems sensible,
       * though maybe "on, not ready to drive" should also count.
       */
      switch ((d[1]>>1) & 3)
        {
        case 0: // off
        case 1: // accessory
        case 2: // on (not ready to drive)
          StandardMetrics.ms_v_env_on->SetValue(false);
          break;
        case 3: // ready to drive
          StandardMetrics.ms_v_env_on->SetValue(true);
          break;
        }
      // The two lock bits are 0x10 driver door and 0x08 other doors.
      // We should only say "locked" if both are locked.
      StandardMetrics.ms_v_env_locked->SetValue( (d[2] & 0x18) == 0x18);
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
// Handles pre and post 2016 model year cars based on xnl.modelyear config
//
// Does nothing if @command is out of range

void OvmsVehicleNissanLeaf::SendCommand(RemoteCommand command)
  {
  unsigned char data[4];
  uint8_t length;
  canbus *tcuBus;

  if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2016)
    {
    ESP_LOGI(TAG, "New TCU on CAR Bus");
    length = 4;
    tcuBus = m_can2;
    }
  else
    {
    ESP_LOGI(TAG, "OLD TCU on EV Bus");
    length = 1;
    tcuBus = m_can1;
    }

  switch (command)
    {
    case ENABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Enable Climate Control");
      data[0] = 0x4e;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    case DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Disable Climate Control");
      data[0] = 0x56;
      data[1] = 0x00;
      data[2] = 0x01;
      data[3] = 0x00;
      break;
    case START_CHARGING:
      ESP_LOGI(TAG, "Start Charging");
      data[0] = 0x66;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    default:
      // shouldn't be possible, but lets not send random data on the bus
      return;
    }
    tcuBus->WriteStandard(0x56e, length, data);
  }

////////////////////////////////////////////////////////////////////////
// implements the repeated sending of remote commands and releases
// EV SYSTEM ACTIVATION REQUEST after an appropriate amount of time

void OvmsVehicleNissanLeaf::RemoteCommandTimer()
  {
  ESP_LOGI(TAG, "RemoteCommandTimer %d", nl_remote_command_ticker);
  if (nl_remote_command_ticker > 0)
    {
    nl_remote_command_ticker--;
    if (nl_remote_command_ticker % 2 == 1)
      {
      SendCommand(nl_remote_command);
      }

    // nl_remote_command_ticker is set to REMOTE_COMMAND_REPEAT_COUNT in
    // RemoteCommandHandler() and we decrement it every 10th of a
    // second, hence the following if statement evaluates to true
    // ACTIVATION_REQUEST_TIME tenths after we start
    // TODO re-implement to support Gen 1 leaf
    //if (nl_remote_command_ticker == REMOTE_COMMAND_REPEAT_COUNT - ACTIVATION_REQUEST_TIME)
    //  {
    //  // release EV SYSTEM ACTIVATION REQUEST
    //  output_gpo3(FALSE);
    //  }
    }
    else
    {
      xTimerStop(m_remoteCommandTimer, 0);
    }
  }

void OvmsVehicleNissanLeaf::Ticker1(uint32_t ticker)
  {
  if (nl_cc_off_ticker > 0) nl_cc_off_ticker--;

  // FIXME
  // detecting that on is stale and therefor should turn off probably shouldn't
  // be done like this
  // perhaps there should be a car on-off state tracker and event generator in
  // the core framework?
  // perhaps interested code should be able to subscribe to "onChange" and
  // "onStale" events for each metric?
  if (StandardMetrics.ms_v_env_awake->IsStale())
    {
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }

  if (nl_cc_off_ticker < (REMOTE_CC_TIME_GRID - REMOTE_CC_TIME_BATTERY)
    && nl_cc_off_ticker > 1
    && !StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
    // we're not on grid power so switch off early
    nl_cc_off_ticker = 1;
    }
  if (nl_cc_off_ticker > 1 && StandardMetrics.ms_v_env_on->AsBool())
    {
    // car has turned on during climate control, switch climate control off
    nl_cc_off_ticker = 1;
    }
  if (nl_cc_off_ticker == 1)
    {
    SendCommand(DISABLE_CLIMATE_CONTROL);
    }
  }

void OvmsVehicleNissanLeaf::Ticker60(uint32_t ticker)
  {
  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    // we only poll while the car is on -- polling at other times causes a
    // relay to click
    PollStart();
    }
  }

////////////////////////////////////////////////////////////////////////
// Wake up the car & send Climate Control or Remote Charge message to VCU,
// replaces Nissan's CARWINGS and TCU module, see
// http://www.mynissanleaf.com/viewtopic.php?f=44&t=4131&hilit=open+CAN+discussion&start=416
//
// On Generation 1 Cars, TCU pin 11's "EV system activation request signal" is
// driven to 12V to wake up the VCU. This function drives RC3 high to
// activate the "EV system activation request signal". Without a circuit
// connecting RC3 to the activation signal wire, remote climate control will
// only work during charging and for obvious reasons remote charging won't
// work at all.
//
// On Generation 2 Cars, a CAN bus message is sent to wake up the VCU. This
// function sends that message even to Generation 1 cars which doesn't seem to
// cause any problems.
//

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::RemoteCommandHandler(RemoteCommand command)
  {
  ESP_LOGI(TAG, "RemoteCommandHandler");

  // Use GPIO to wake up GEN 1 Leaf with EV SYSTEM ACTIVATION REQUEST
  // TODO re-implement to support Gen 1 leaf
  //output_gpo3(TRUE);

  // The Gen 2 Wakeup frame works on some Gen1, and doesn't cause a problem
  // so we send it on all models. We have to take care to send it on the
  // correct can bus in newer cars.
  ESP_LOGI(TAG, "Sending Gen 2 Wakeup Frame");
  int modelyear = MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR);
  canbus* tcuBus = modelyear >= 2016 ? m_can2 : m_can1;
  unsigned char data = 0;
  tcuBus->WriteStandard(0x68c, 1, &data);

  // The GEN 2 Nissan TCU module sends the command repeatedly, so we start
  // m_remoteCommandTimer (which calls RemoteCommandTimer()) to do this
  // EV SYSTEM ACTIVATION REQUEST is released in the timer too
  nl_remote_command = command;
  nl_remote_command_ticker = REMOTE_COMMAND_REPEAT_COUNT;
  xTimerStart(m_remoteCommandTimer, 0);

  if (command == ENABLE_CLIMATE_CONTROL)
    {
    nl_cc_off_ticker = REMOTE_CC_TIME_GRID;
    }

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandHomelink(int button)
  {
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0)
    {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
    }
  if (button == 1)
    {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
    }
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandClimateControl(bool climatecontrolon)
  {
  ESP_LOGI(TAG, "CommandClimateControl");
  return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandStartCharge()
  {
  ESP_LOGI(TAG, "CommandStartCharge");
  return RemoteCommandHandler(START_CHARGING);
  }

class OvmsVehicleNissanLeafInit
  {
  public: OvmsVehicleNissanLeafInit();
  } MyOvmsVehicleNissanLeafInit  __attribute__ ((init_priority (9000)));

OvmsVehicleNissanLeafInit::OvmsVehicleNissanLeafInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Nissan Leaf (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleNissanLeaf>("NL","Nissan Leaf");
  }
