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
static const char *TAG = "notify";

#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "ovms.h"
#include "ovms_notify.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "buffered_shell.h"
#include "string.h"

using namespace std;

////////////////////////////////////////////////////////////////////////
// Console commands...

OvmsNotify       MyNotify       __attribute__ ((init_priority (1820)));

void notify_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(),"on")==0)
    MyNotify.m_trace = true;
  else
    MyNotify.m_trace = false;

  writer->printf("Notification tracing is now %s\n",cmd->GetName());
  }

void notify_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Notification system has %d readers registered\n",
    MyNotify.CountReaders());
  for (OvmsNotifyCallbackMap_t::iterator itc=MyNotify.m_readers.begin(); itc!=MyNotify.m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    writer->printf("  %s: verbosity=%d\n", mc->m_caller, mc->m_verbosity);
    }

  if (MyNotify.m_types.size() > 0)
    {
    writer->puts("Notify types:");
    for (OvmsNotifyTypeMap_t::iterator itm=MyNotify.m_types.begin(); itm!=MyNotify.m_types.end(); ++itm)
      {
      OvmsNotifyType* mt = itm->second;
      writer->printf("  %s: %d entries\n",
        mt->m_name, mt->m_entries.size());
      for (NotifyEntryMap_t::iterator ite=mt->m_entries.begin(); ite!=mt->m_entries.end(); ++ite)
        {
        OvmsNotifyEntry* e = ite->second;
        writer->printf("    %d: [%d pending] %s\n",
          ite->first, e->m_readers.count(), e->GetValue().c_str());
        }
      }
    }
  }

void notify_raise(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Raise %s notification for %s/%s as %s\n",
    cmd->GetName(), argv[0], argv[1], argv[2]);

  if (strcmp(cmd->GetName(),"text")==0)
    MyNotify.NotifyString(argv[0],argv[1],argv[2]);
  else
    MyNotify.NotifyCommand(argv[0],argv[1],argv[2]);
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntry is the virtual object for notification entries.
// These are added to a particular OvmsNotifyType (as a member of a list)
// and readers are notified. Once a reader has processed an entry, the
// MarkRead function is called. The framework can test IsAllRead() to
// see if all readers have processed the entry and housekeep cleanup
// appropriately.

OvmsNotifyEntry::OvmsNotifyEntry(const char* subtype)
  {
  m_readers.reset();
  m_id = 0;
  m_created = monotonictime;
  m_subtype = subtype;
  }

OvmsNotifyEntry::~OvmsNotifyEntry()
  {
  }

bool OvmsNotifyEntry::IsRead(size_t reader)
  {
  return !m_readers[reader];
  }

bool OvmsNotifyEntry::IsAllRead()
  {
  return (m_readers.count() == 0);
  }

const extram::string OvmsNotifyEntry::GetValue()
  {
  return extram::string("");
  }

const char* OvmsNotifyEntry::GetSubType()
  {
  return m_subtype;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryString is the notification entry for a constant
// string type.

OvmsNotifyEntryString::OvmsNotifyEntryString(const char* subtype, const char* value)
  : OvmsNotifyEntry(subtype)
  {
  m_value = extram::string(value);
  }

OvmsNotifyEntryString::~OvmsNotifyEntryString()
  {
  }

const extram::string OvmsNotifyEntryString::GetValue()
  {
  return m_value;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryCommand is the notification entry for a command
// callback type.

OvmsNotifyEntryCommand::OvmsNotifyEntryCommand(const char* subtype, int verbosity, const char* cmd)
  : OvmsNotifyEntry(subtype)
  {
  m_cmd = new char[strlen(cmd)+1];
  strcpy(m_cmd,cmd);

  BufferedShell* bs = new BufferedShell(false, verbosity);
  // command notifications can only be raised by the system or "notify raise" in enabled mode,
  // so we can assume this is a secure shell:
  bs->SetSecure(true);
  bs->ProcessChars(m_cmd, strlen(m_cmd));
  bs->ProcessChar('\n');
  bs->Dump(m_value);
  delete bs;
  }

OvmsNotifyEntryCommand::~OvmsNotifyEntryCommand()
  {
  if (m_cmd)
    {
    delete [] m_cmd;
    m_cmd = NULL;
    }
  }

const extram::string OvmsNotifyEntryCommand::GetValue()
  {
  return m_value;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyType is the container for an ordered list of
// OvmsNotifyEntry objects (being the notification data queued)

OvmsNotifyType::OvmsNotifyType(const char* name)
  {
  m_name = name;
  m_nextid = 1;
  }

OvmsNotifyType::~OvmsNotifyType()
  {
  }

uint32_t OvmsNotifyType::QueueEntry(OvmsNotifyEntry* entry)
  {
  uint32_t id = m_nextid++;

  entry->m_id = id;
  m_entries[id] = entry;

  std::string event("notify.");
  event.append(m_name);
  MyEvents.SignalEvent(event, (void*)entry);

  // Dispatch the callbacks...
  MyNotify.NotifyReaders(this, entry);

  // Check if we can cleanup...
  Cleanup(entry);

  return id;
  }

uint32_t OvmsNotifyType::AllocateNextID()
  {
  return m_nextid++;
  }

void OvmsNotifyType::ClearReader(size_t reader)
  {
  if (m_entries.size() > 0)
    {
    for (NotifyEntryMap_t::iterator ite=m_entries.begin(); ite!=m_entries.end(); )
      {
      OvmsNotifyEntry* e = ite->second;
      ++ite;
      e->m_readers.reset(reader);
      Cleanup(e);
      }
    }
  }

OvmsNotifyEntry* OvmsNotifyType::FirstUnreadEntry(size_t reader, uint32_t floor)
  {
  for (NotifyEntryMap_t::iterator ite=m_entries.begin(); ite!=m_entries.end(); ++ite)
    {
    OvmsNotifyEntry* e = ite->second;
    if ((!e->IsRead(reader))&&(e->m_id > floor))
      return e;
    }
  return NULL;
  }

OvmsNotifyEntry* OvmsNotifyType::FindEntry(uint32_t id)
  {
  auto k = m_entries.find(id);
  if (k == m_entries.end())
    return NULL;
  else
    return k->second;
  }

void OvmsNotifyType::MarkRead(size_t reader, OvmsNotifyEntry* entry)
  {
  entry->m_readers.reset(reader);
  Cleanup(entry);
  }

void OvmsNotifyType::Cleanup(OvmsNotifyEntry* entry)
  {
  if (entry->IsAllRead())
    {
    // We can cleanup...
    auto k = m_entries.find(entry->m_id);
    if (k != m_entries.end())
       {
       m_entries.erase(k);
       }
    if (MyNotify.m_trace)
      ESP_LOGI(TAG,"Cleanup type %s id %d",m_name,entry->m_id);
    delete entry;
    }
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyCallbackEntry contains the callback function for a
// particular reader

OvmsNotifyCallbackEntry::OvmsNotifyCallbackEntry(const char* caller, size_t reader, int verbosity, OvmsNotifyCallback_t callback, bool filtered)
  {
  m_caller = caller;
  m_reader = reader;
  m_verbosity = verbosity;
  m_callback = callback;
  m_filtered = filtered;
  }

OvmsNotifyCallbackEntry::~OvmsNotifyCallbackEntry()
  {
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyCallbackEntry contains the callback function for a
//

OvmsNotify::OvmsNotify()
  {
  ESP_LOGI(TAG, "Initialising NOTIFICATIONS (1820)");

  m_nextreader = 1;

#ifdef CONFIG_OVMS_DEV_DEBUGNOTIFICATIONS
  m_trace = true;
#else
  m_trace = false;
#endif // #ifdef CONFIG_OVMS_DEV_DEBUGNOTIFICATIONS

  MyConfig.RegisterParam("notify", "Notification filters", true, true);

  // Register our commands
  OvmsCommand* cmd_notify = MyCommandApp.RegisterCommand("notify","NOTIFICATION framework",NULL, "", 1, 0, true);
  cmd_notify->RegisterCommand("status","Show notification status",notify_status,"", 0, 0, true);
  OvmsCommand* cmd_notifyraise = cmd_notify->RegisterCommand("raise","NOTIFICATION raise framework", NULL, "", 0, 0, true);
  cmd_notifyraise->RegisterCommand("text","Raise a textual notification",notify_raise,"<type><subtype><message>", 3, 3, true);
  cmd_notifyraise->RegisterCommand("command","Raise a command callback notification",notify_raise,"<type><subtype><command>", 3, 3, true);
  OvmsCommand* cmd_notifytrace = cmd_notify->RegisterCommand("trace","NOTIFICATION trace framework", NULL, "", 0, 0, true);
  cmd_notifytrace->RegisterCommand("on","Turn notification tracing ON",notify_trace,"", 0, 0, true);
  cmd_notifytrace->RegisterCommand("off","Turn notification tracing OFF",notify_trace,"", 0, 0, true);

  RegisterType("info");
  RegisterType("error");
  RegisterType("alert");
  RegisterType("data");
  }

OvmsNotify::~OvmsNotify()
  {
  }

size_t OvmsNotify::RegisterReader(const char* caller, int verbosity, OvmsNotifyCallback_t callback, bool filtered)
  {
  size_t reader = m_nextreader++;

  m_readers[caller] = new OvmsNotifyCallbackEntry(caller, reader, verbosity, callback, filtered);

  return reader;
  }

void OvmsNotify::ClearReader(const char* caller)
  {
  auto k = m_readers.find(caller);
  if (k != m_readers.end())
    {
    for (OvmsNotifyTypeMap_t::iterator itt=m_types.begin(); itt!=m_types.end(); ++itt)
      {
      OvmsNotifyType* t = itt->second;
      t->ClearReader(k->second->m_reader);
      }
    OvmsNotifyCallbackEntry* ec = k->second;
    m_readers.erase(k);
    delete ec;
    }
  }

size_t OvmsNotify::CountReaders()
  {
  return m_nextreader-1;
  }

OvmsNotifyType* OvmsNotify::GetType(const char* type)
  {
  auto k = m_types.find(type);
  if (k == m_types.end())
    return NULL;
  else
    return k->second;
  }

void OvmsNotify::NotifyReaders(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (mc->m_filtered)
      {
      // Check if we need to filter this
      std::string filter = MyConfig.GetParamValue("notify", entry->GetSubType());
      if (!filter.empty())
        {
        if (filter.find(mc->m_caller) == string::npos)
          {
          entry->m_readers.reset(mc->m_reader);
          continue; // This is filtered out
          }
        }
      }
    bool result = mc->m_callback(type,entry);
    if (result) entry->m_readers.reset(mc->m_reader);
    }
  }

void OvmsNotify::RegisterType(const char* type)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    mt = new OvmsNotifyType(type);
    m_types[type] = mt;
    ESP_LOGI(TAG,"Registered notification type %s",type);
    }
  }

uint32_t OvmsNotify::NotifyString(const char* type, const char* subtype, const char* value)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, value);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise text %s: %s", type, value);

  if (m_readers.size() == 0)
    {
    ESP_LOGD(TAG, "Abort: no readers");
    return 0;
    }

  // create message:
  OvmsNotifyEntry* msg = (OvmsNotifyEntry*) new OvmsNotifyEntryString(subtype, value);

  // add all currently active readers accepting the message length:
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (strlen(value) <= mc->m_verbosity)
      msg->m_readers.set(mc->m_reader);
    }

  ESP_LOGD(TAG, "Created entry with length %d has %d readers pending", strlen(value), msg->m_readers.count());

  return mt->QueueEntry(msg);
  }

uint32_t OvmsNotify::NotifyCommand(const char* type, const char* subtype, const char* cmd)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, cmd);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise command %s: %s", type, cmd);

  if (m_readers.size() == 0)
    {
    ESP_LOGD(TAG, "Abort: no readers");
    return 0;
    }

  // Strategy:
  //  to minimize RAM usage and command calls we try to reuse higher verbosity messages
  //  if their result length fits for lower verbosity readers as well.

  std::map<int, OvmsNotifyEntryCommand*> verbosity_msgs;
  std::map<int, OvmsNotifyEntryCommand*>::iterator itm;
  std::map<int, OvmsNotifyEntryCommand*>::reverse_iterator ritm;
  OvmsNotifyCallbackMap_t::iterator itc;
  OvmsNotifyEntryCommand *msg;
  size_t msglen;

  // get verbosity levels needed by readers:
  for (itc=m_readers.begin(); itc!=m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    verbosity_msgs[mc->m_verbosity] = NULL;
    }

  // fetch verbosity levels beginning at highest verbosity:
  msg = NULL;
  msglen = 0;
  for (ritm=verbosity_msgs.rbegin(); ritm!=verbosity_msgs.rend(); ritm++)
    {
    int verbosity = ritm->first;

    if (msg && msglen <= verbosity)
      {
      // reuse last verbosity level message:
      verbosity_msgs[verbosity] = msg;
      }
    else
      {
      msg = verbosity_msgs[verbosity];
      if (!msg)
        {
        // create verbosity level message:
        msg = new OvmsNotifyEntryCommand(subtype, verbosity, cmd);
        msglen = msg->GetValue().length();
        verbosity_msgs[verbosity] = msg;
        }
      }
    }

  // add readers:
  for (itc=m_readers.begin(); itc!=m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    msg = verbosity_msgs[mc->m_verbosity];
    msg->m_readers.set(mc->m_reader);
    }

  // queue all verbosity level messages beginning at lowest verbosity (fastest delivery):
  msg = NULL;
  uint32_t queue_id = 0;
  for (itm=verbosity_msgs.begin(); itm!=verbosity_msgs.end(); itm++)
    {
    if (itm->second == msg)
      continue; // already queued

    msg = itm->second;
    ESP_LOGD(TAG, "Created entry for verbosity %d has %d readers pending", itm->first, msg->m_readers.count());
    queue_id = mt->QueueEntry(msg);
    }

  return queue_id;
  }


/**
 * NotifyStringf: printf style API
 */
uint32_t OvmsNotify::NotifyStringf(const char* type, const char* subtype, const char* fmt, ...)
  {
  char *buffer = NULL;
  uint32_t res = 0;
  va_list args;
  va_start(args, fmt);
  int len = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (len >= 0)
    {
    res = NotifyString(type, subtype, buffer);
    free(buffer);
    }
  return res;
  }


/**
 * NotifyCommandf: printf style API
 */
uint32_t OvmsNotify::NotifyCommandf(const char* type, const char* subtype, const char* fmt, ...)
  {
  char *buffer = NULL;
  uint32_t res = 0;
  va_list args;
  va_start(args, fmt);
  int len = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (len >= 0)
    {
    res = NotifyCommand(type, subtype, buffer);
    free(buffer);
    }
  return res;
  }
