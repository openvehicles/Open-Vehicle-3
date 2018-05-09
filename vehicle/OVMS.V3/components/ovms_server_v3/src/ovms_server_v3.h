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

#ifndef __OVMS_SERVER_V3_H__
#define __OVMS_SERVER_V3_H__

#include "ovms_server.h"
#include "ovms_netmanager.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include "ovms_config.h"
#include "ovms_mutex.h"

class OvmsServerV3 : public OvmsServer
  {
  public:
    OvmsServerV3(const char* name);
    ~OvmsServerV3();

  public:
    void MetricModified(OvmsMetric* metric);
    bool IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry);
    void EventListener(std::string event, void* data);
    void ConfigChanged(OvmsConfigParam* param);
    void NetUp(std::string event, void* data);
    void NetDown(std::string event, void* data);
    void NetReconfigured(std::string event, void* data);
    void NetmanInit(std::string event, void* data);
    void NetmanStop(std::string event, void* data);
    void Ticker1(std::string event, void* data);

  public:
    enum State
      {
      Undefined = 0,
      WaitNetwork,
      ConnectWait,
      Connecting,
      Authenticating,
      Connected,
      Disconnected,
      WaitReconnect
      };
    State m_state;
    std::string m_status;
    void SetStatus(const char* status, bool fault=false, State newstate=Undefined);

  public:
    std::string m_vehicleid;
    std::string m_server;
    std::string m_user;
    std::string m_password;
    std::string m_port;
    struct mg_connection *m_mgconn;
    OvmsMutex m_mgconn_mutex;
    int m_connretry;
    bool m_sendall;
    int m_msgid;
    int m_lasttx;
    int m_lasttx_stream;
    int m_peers;
    int m_streaming;
    int m_updatetime_idle;
    int m_updatetime_connected;

  public:
    virtual void SetPowerMode(PowerMode powermode);
    void Connect();
    void Disconnect();
    void TransmitAllMetrics();
    void TransmitModifiedMetrics();
  };

class OvmsServerV3Init
  {
  public:
    OvmsServerV3Init();
    void AutoInit();
  };

extern OvmsServerV3Init MyOvmsServerV3Init;

#endif //#ifndef __OVMS_SERVER_V3_H__
