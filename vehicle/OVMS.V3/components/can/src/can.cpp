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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#include "ovms_log.h"
static const char *TAG = "can";

#include "can.h"
#include "canlog.h"
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include "ovms_command.h"

can MyCan __attribute__ ((init_priority (4500)));;

void can_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  int baud = atoi(argv[0]);

  CAN_mode_t smode = CAN_MODE_LISTEN;
  if (strcmp(mode, "active")==0) smode = CAN_MODE_ACTIVE;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  switch (baud)
    {
    case 100000:
      sbus->Start(smode,CAN_SPEED_100KBPS);
      break;
    case 125000:
      sbus->Start(smode,CAN_SPEED_125KBPS);
      break;
    case 250000:
      sbus->Start(smode,CAN_SPEED_250KBPS);
      break;
    case 500000:
      sbus->Start(smode,CAN_SPEED_500KBPS);
      break;
    case 1000000:
      sbus->Start(smode,CAN_SPEED_1000KBPS);
      break;
    default:
      writer->puts("Error: Unrecognised speed (100000, 125000, 250000, 500000, 1000000 are accepted)");
      return;
    }
  writer->printf("Can bus %s started in mode %s at speed %dbps\n",
                 bus, mode, baud);
  }

void can_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  sbus->Stop();
  writer->printf("Can bus %s stapped\n",bus);
  }

void can_tx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  CAN_frame_format_t smode = CAN_frame_std;
  if (strcmp(mode, "extended")==0) smode = CAN_frame_ext;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (sbus->GetPowerMode() != On)
    {
    writer->puts("Error: Can bus is not powered on");
    return;
    }

  CAN_frame_t frame;
  frame.origin = NULL;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  frame.MsgID = (int)strtol(argv[0],NULL,16);
  for(int k=0;k<(argc-1);k++)
    {
    frame.data.u8[k] = strtol(argv[k+1],NULL,16);
    }
  sbus->Write(&frame);
  }

void can_rx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  CAN_frame_format_t smode = CAN_frame_std;
  if (strcmp(mode, "extended")==0) smode = CAN_frame_ext;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (sbus->GetPowerMode() != On)
    {
    writer->puts("Error: Can bus is not powered on");
    return;
    }

  CAN_frame_t frame;
  frame.origin = sbus;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  frame.MsgID = (int)strtol(argv[0],NULL,16);
  for(int k=0;k<(argc-1);k++)
    {
    frame.data.u8[k] = strtol(argv[k+1],NULL,16);
    }
  MyCan.IncomingFrame(&frame);
  }


// Shell command:
//    can log <type> [path] [filter1] [filter2] [filter3]
//    can log off
//    can log status
//
// Filter: [bus:][id][-id]
// Examples:
//    can log trace                     → enable syslog tracing for all buses, all ids
//    can log trace 1 3:780-7ff         → enable syslog for bus 1 and id range 780-7ff on bus 3
//    can log crtd /sd/cap1 2:100-1ff   → capture id range 100-1ff of bus 2 in crtd file /sd/cap1
//
// Path and filters can be changed on the fly.
//
void can_log(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* type = cmd->GetName();
  bool is_trace = (strcmp(type, "trace") == 0);
  canlog* cl = MyCan.GetLogger();

  if (strcmp(type, "status") == 0)
    {
    if (cl)
      writer->printf("CAN logging active: %s\nStatistics: %s\n", cl->GetInfo().c_str(), cl->GetStats().c_str());
    else
      writer->puts("CAN logging inactive.");
    return;
    }

  if (strcmp(type, "off") == 0)
    {
    if (!cl)
      {
      writer->puts("CAN logging inactive.");
      return;
      }
    writer->printf("Closing log: %s\n", cl->GetInfo().c_str());
    MyCan.UnsetLogger();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    cl->Close();
    writer->printf("Statistics: %s\n", cl->GetStats().c_str());
    delete cl;
    writer->puts("CAN logging stopped.");
    return;
    }

  if (cl && strcmp(type, cl->GetType()) != 0)
    {
    writer->printf("Error: logger of type '%s' still running, please stop first\n", cl->GetType());
    return;
    }

  // parse args:

  const char* path = (cl && !is_trace) ? cl->GetPath().c_str() : "";
  canlog_filter_t filter[CANLOG_MAX_FILTERS] = {};
  int fcnt = 0;

  for (int i=0; i<argc; i++)
    {
    if (argv[i][0] == '/' && !is_trace)
      {
      path = argv[i];
      }
    else if (fcnt < CANLOG_MAX_FILTERS)
      {
      char* s = (char*) argv[i];
      if (s[1] == 0)
        {
        filter[fcnt].bus = s[0];
        filter[fcnt].id_from = 0;
        filter[fcnt].id_to = UINT32_MAX;
        }
      else
        {
        if (s[1] == ':')
          {
          filter[fcnt].bus = s[0];
          s += 2;
          }
        filter[fcnt].id_from = strtol(s, &s, 16);
        if (*s)
          filter[fcnt].id_to = strtol(s+1, NULL, 16); // id range
        else
          filter[fcnt].id_to = filter[fcnt].id_from; // single id
        if (filter[fcnt].id_to < filter[fcnt].id_from)
          {
          uint32_t tmp = filter[fcnt].id_to;
          filter[fcnt].id_to = filter[fcnt].id_from;
          filter[fcnt].id_from = tmp;
          }
        }
      fcnt++;
      }
    }

  if (!is_trace && !path[0])
    {
    writer->puts("Error: no path specified");
    return;
    }

  // start / reconfigure logger:

  if (!cl)
    {
    cl = canlog::Instantiate(type);
    if (!cl)
      {
      writer->printf("Error: cannot create logger of type '%s'\n", type);
      return;
      }
    }

  if (!is_trace && cl->IsOpen() && cl->GetPath() != path)
    {
    writer->printf("Closing path '%s'\n", cl->GetPath().c_str());
    MyCan.UnsetLogger();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    writer->printf("Statistics: %s\n", cl->GetStats().c_str());
    cl->Close();
    }

  cl->SetFilter(fcnt, filter);

  if (!is_trace && !cl->IsOpen() && !cl->Open(path))
    {
    writer->printf("Error: cannot open log path '%s'\n", path);
    delete cl;
    return;
    }

  MyCan.SetLogger(cl);
  writer->printf("CAN logging active: %s\n", cl->GetInfo().c_str());
  if (is_trace)
    writer->puts("Note: info logging is done at log level debug, frame logging at verbose");
  }


void can_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  sbus->LogStatus(CAN_LogStatus_Statistics);

  writer->printf("CAN:       %s\n",sbus->GetName());
  writer->printf("Mode:      %s\n",(sbus->m_mode==CAN_MODE_OFF)?"Off":
                                   ((sbus->m_mode==CAN_MODE_LISTEN)?"Listen":"Active"));
  writer->printf("Speed:     %d\n",(sbus->m_speed)*1000);
  writer->printf("Interrupts:%20d\n",sbus->m_status.interrupts);
  writer->printf("Rx pkt:    %20d\n",sbus->m_status.packets_rx);
  writer->printf("Rx err:    %20d\n",sbus->m_status.errors_rx);
  writer->printf("Rx ovrflw: %20d\n",sbus->m_status.rxbuf_overflow);
  writer->printf("Tx pkt:    %20d\n",sbus->m_status.packets_tx);
  writer->printf("Tx delays: %20d\n",sbus->m_status.txbuf_delay);
  writer->printf("Tx err:    %20d\n",sbus->m_status.errors_tx);
  writer->printf("Tx ovrflw: %20d\n",sbus->m_status.txbuf_overflow);
  writer->printf("Err flags: %#x\n",sbus->m_status.error_flags);
  }

static void CAN_rxtask(void *pvParameters)
  {
  can *me = (can*)pvParameters;
  CAN_msg_t msg;

  while(1)
    {
    if (xQueueReceive(me->m_rxqueue,&msg, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      switch(msg.type)
        {
        case CAN_frame:
          me->IncomingFrame(&msg.body.frame);
          break;
        case CAN_rxcallback:
          while (msg.body.bus->RxCallback(&msg.body.frame))
            {
            me->IncomingFrame(&msg.body.frame);
            }
          break;
        case CAN_txcallback:
          msg.body.bus->TxCallback();
          break;
        case CAN_logerror:
          msg.body.bus->LogStatus(CAN_LogStatus_Error);
          break;
        default:
          break;
        }
      }
    }
  }

can::can()
  {
  ESP_LOGI(TAG, "Initialising CAN (4500)");

  OvmsCommand* cmd_can = MyCommandApp.RegisterCommand("can","CAN framework",NULL, "", 0, 0, true);

  for (int k=1;k<4;k++)
    {
    static const char* name[4] = {"can1", "can2", "can3"};
    OvmsCommand* cmd_canx = cmd_can->RegisterCommand(name[k-1],"CANx framework",NULL, "", 0, 0, true);
    OvmsCommand* cmd_canstart = cmd_canx->RegisterCommand("start","CAN start framework", NULL, "", 0, 0, true);
    cmd_canstart->RegisterCommand("listen","Start CAN bus in listen mode",can_start,"<baud>", 1, 1, true);
    cmd_canstart->RegisterCommand("active","Start CAN bus in active mode",can_start,"<baud>", 1, 1, true);
    cmd_canx->RegisterCommand("stop","Stop CAN bus",can_stop, "", 0, 0, true);
    OvmsCommand* cmd_cantx = cmd_canx->RegisterCommand("tx","CAN tx framework", NULL, "", 0, 0, true);
    cmd_cantx->RegisterCommand("standard","Transmit standard CAN frame",can_tx,"<id> <data...>", 1, 9, true);
    cmd_cantx->RegisterCommand("extended","Transmit extended CAN frame",can_tx,"<id> <data...>", 1, 9, true);
    OvmsCommand* cmd_canrx = cmd_canx->RegisterCommand("rx","CAN rx framework", NULL, "", 0, 0, true);
    cmd_canrx->RegisterCommand("standard","Simulate reception of standard CAN frame",can_rx,"<id> <data...>", 1, 9, true);
    cmd_canrx->RegisterCommand("extended","Simulate reception of extended CAN frame",can_rx,"<id> <data...>", 1, 9, true);
    cmd_canx->RegisterCommand("status","Show CAN status",can_status,"", 0, 0, true);
    }

  OvmsCommand* cmd_canlog = cmd_can->RegisterCommand("log", "CAN logging framework", NULL, "", 0, 0, true);
  cmd_canlog->RegisterCommand("trace", "Logging to syslog", can_log,
    "[filter1] [filter2] [filter3]\n"
    "Filter: <bus> / <id>[-<id>] / <bus>:<id>[-<id>]\n"
    "Example: 2:2a0-37f", 0, 3, true);
  const char* const* logtype = canlog::GetTypeList();
  while (*logtype)
    {
    if (strcmp(*logtype, "trace") != 0)
      {
      cmd_canlog->RegisterCommand(*logtype, "...format logging", can_log,
        "<path> [filter1] [filter2] [filter3]\n"
        "Filter: <bus> / <id>[-<id>] / <bus>:<id>[-<id>]\n"
        "Example: 2:2a0-37f", 1, 4, true);
      }
    logtype++;
    }
  cmd_canlog->RegisterCommand("off", "Stop logging", can_log, "", 0, 0, true);
  cmd_canlog->RegisterCommand("status", "Logging status", can_log, "", 0, 0, true);

  m_rxqueue = xQueueCreate(20,sizeof(CAN_msg_t));
  xTaskCreatePinnedToCore(CAN_rxtask, "OVMS CanRx", 1024, (void*)this, 10, &m_rxtask, 0);
  m_logger = NULL;
  }

can::~can()
  {
  }

void can::IncomingFrame(CAN_frame_t* p_frame)
  {
  p_frame->origin->m_status.packets_rx++;

  for (auto n : m_listeners)
    {
    xQueueSend(n,p_frame,0);
    }

  p_frame->origin->LogFrame(CAN_LogFrame_RX, p_frame);
  }

void can::RegisterListener(QueueHandle_t queue)
  {
  m_listeners.push_back(queue);
  }

void can::DeregisterListener(QueueHandle_t queue)
  {
  auto it = std::find(m_listeners.begin(), m_listeners.end(), queue);
  if (it != m_listeners.end())
    {
    m_listeners.erase(it);
    }
  }



canbus::canbus(const char* name)
  : pcp(name)
  {
  m_txqueue = xQueueCreate(20, sizeof(CAN_frame_t));
  m_mode = CAN_MODE_OFF;
  m_speed = CAN_SPEED_1000KBPS;
  memset(&m_status, 0, sizeof(m_status));
  m_status_chksum = 0;
  }

canbus::~canbus()
  {
  vQueueDelete(m_txqueue);
  }

esp_err_t canbus::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  // clear statistics:
  memset(&m_status, 0, sizeof(m_status));
  m_status_chksum = 0;
  return ESP_FAIL;
  }

esp_err_t canbus::Stop()
  {
  return ESP_FAIL;
  }

bool canbus::RxCallback(CAN_frame_t* frame)
  {
  return false;
  }

void canbus::TxCallback()
  {
  }

/**
 * canbus::Write -- main TX API
 *    - returns ESP_OK, ESP_QUEUED or ESP_FAIL
 *      … ESP_OK = frame delivered to CAN transceiver (not necessarily sent!)
 *      … ESP_FAIL = TX queue is full (TX overflow)
 *    - actual TX implementation in driver override
 *    - here: counting & logging of frames sent (called by driver after a successful TX)
 */
esp_err_t canbus::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  m_status.packets_tx++;
  LogFrame(CAN_LogFrame_TX, p_frame);
  return ESP_OK;
  }

/**
 * canbus::QueueWrite -- add a frame to the TX queue for later delivery
 *    - internal method, called by driver if no TX buffer is available
 */
esp_err_t canbus::QueueWrite(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueSend(m_txqueue, p_frame, maxqueuewait) == pdTRUE)
    {
    m_status.txbuf_delay++;
    LogFrame(CAN_LogFrame_TX_Queue, p_frame);
    return ESP_QUEUED;
    }
  else
    {
    m_status.txbuf_overflow++;
    LogFrame(CAN_LogFrame_TX_Fail, p_frame);
    return ESP_FAIL;
    }
  }

/**
 * canbus::WriteExtended -- application TX utility
 */
esp_err_t canbus::WriteExtended(uint32_t id, uint8_t length, uint8_t *data, TickType_t maxqueuewait /*=0*/)
  {
  if (length > 8)
    {
    abort();
    }

  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = this;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = length;
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = id;
  memcpy(frame.data.u8, data, length);
  return this->Write(&frame, maxqueuewait);
  }

/**
 * canbus::WriteStandard -- application TX utility
 */
esp_err_t canbus::WriteStandard(uint16_t id, uint8_t length, uint8_t *data, TickType_t maxqueuewait /*=0*/)
  {
  if (length > 8)
    {
    abort();
    }

  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = this;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = length;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = id;
  memcpy(frame.data.u8, data, length);
  return this->Write(&frame, maxqueuewait);
  }

/**
 * CAN_frame_t::Write -- main TX API
 *    - returns ESP_OK, ESP_QUEUED or ESP_FAIL
 *      … ESP_OK = frame delivered to CAN transceiver (not necessarily sent!)
 *      … ESP_FAIL = TX queue is full (TX overflow)
 */
esp_err_t CAN_frame_t::Write(canbus* bus /*=NULL*/, TickType_t maxqueuewait /*=0*/)
  {
  if (!bus)
    bus = origin;
  return bus ? bus->Write(this, maxqueuewait) : ESP_FAIL;
  }


/**
 * Tracing/logging
 */

void can::LogFrame(canbus* bus, CAN_LogEntry_t type, const CAN_frame_t* frame)
  {
  if (m_logger)
    m_logger->LogFrame(bus, type, frame);
  }

void can::LogStatus(canbus* bus, CAN_LogEntry_t type, const CAN_status_t* status)
  {
  if (m_logger)
    m_logger->LogStatus(bus, type, status);
  }

void can::LogInfo(canbus* bus, CAN_LogEntry_t type, const char* text)
  {
  if (m_logger)
    m_logger->LogInfo(bus, type, text);
  }


void canbus::LogFrame(CAN_LogEntry_t type, const CAN_frame_t* frame)
  {
  MyCan.LogFrame(this, type, frame);
  }

void canbus::LogStatus(CAN_LogEntry_t type)
  {
  if (!MyCan.GetLogger() || (type==CAN_LogStatus_Error && !StatusChanged()))
    return;
  MyCan.LogStatus(this, type, &m_status);
  }

void canbus::LogInfo(CAN_LogEntry_t type, const char* text)
  {
  MyCan.LogInfo(this, type, text);
  }


bool canbus::StatusChanged()
  {
  // simple checksum to prevent log flooding:
  uint32_t chksum = m_status.packets_rx + m_status.packets_tx + m_status.errors_rx + m_status.errors_tx
    + m_status.rxbuf_overflow + m_status.txbuf_overflow + m_status.error_flags + m_status.txbuf_delay;
  if (chksum != m_status_chksum)
    {
    m_status_chksum = chksum;
    return true;
    }
  return false;
  }
