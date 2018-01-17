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
static const char *TAG = "command";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "log_buffers.h"

static FILE *ovms_log_file;
OvmsCommandApp MyCommandApp __attribute__ ((init_priority (1000)));

bool CompareCharPtr::operator()(const char* a, const char* b)
  {
  return strcmp(a, b) < 0;
  }

OvmsWriter::OvmsWriter()
  {
  std::string p = MyConfig.GetParamValue("password","module");
  if (p.empty())
    m_issecure = true;
  else
    m_issecure = false;
  m_insert = NULL;
  m_userData = NULL;
  m_monitoring = false;
  }

OvmsWriter::~OvmsWriter()
  {
  }

void OvmsWriter::Exit()
  {
  puts("This console cannot exit.");
  }

void OvmsWriter::RegisterInsertCallback(InsertCallback cb, void* ctx)
  {
  m_insert = cb;
  m_userData = ctx;
  }

bool OvmsWriter::IsSecure()
  {
  return m_issecure;
  }

void OvmsWriter::SetSecure(bool secure)
  {
  m_issecure = secure;
  }

OvmsCommand* OvmsCommandMap::FindUniquePrefix(const char* key)
  {
  int len = strlen(key);
  OvmsCommand* found = NULL;
  for (iterator it = begin(); it != end(); ++it)
    {
    if (strncmp(it->first, key, len) == 0)
      {
      if (len == strlen(it->first))
        {
        return it->second;
        }
      if (found)
        {
        return NULL;
        }
      else
        {
        found = it->second;
        }
      }
    }
  return found;
  }

OvmsCommand::OvmsCommand()
  {
  m_parent = NULL;
  }

OvmsCommand::OvmsCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                         const char *usage, int min, int max, bool secure)
  {
  m_name = name;
  m_title = title;
  m_execute = execute;
  m_usage_template= usage;
  m_min = min;
  m_max = max;
  m_parent = NULL;
  m_secure = secure;
  }

OvmsCommand::~OvmsCommand()
  {
  }

const char* OvmsCommand::GetName()
  {
  return m_name;
  }

const char* OvmsCommand::GetTitle()
  {
  return m_title;
  }

// Dynamic generation of "Usage:" messages.  Syntax of the usage template string:
// - Prefix is "Usage: " followed by names from ancestors (if any) and name of self
// - "$C" expands to children as child1|child2|child3
// - "[$C]" expands to optional children as [child1|child2|child3]
// - $G$ expands to the usage of the first child (typically used after $C)
// - $Gfoo$ expands to the usage of the child named "foo"
// - Parameters after command and subcommand tokens may be explicit like " <metric>"
// - Empty usage template "" defaults to "$C" for non-terminal OvmsCommand
const char* OvmsCommand::GetUsage(OvmsWriter* writer)
  {
  std::string usage = !m_usage_template ? "" : (!*m_usage_template && !m_execute) ? "$C" : m_usage_template;
  m_usage ="Usage: ";
  size_t pos = m_usage.size();
  for (OvmsCommand* parent = m_parent; parent && parent->m_parent; parent = parent->m_parent)
    {
    m_usage.insert(pos, " ");
    m_usage.insert(pos, parent->m_name);
    }
  m_usage += m_name;
  m_usage += " ";
  ExpandUsage(usage, writer);
  return m_usage.c_str();
  }

void OvmsCommand::ExpandUsage(std::string usage, OvmsWriter* writer)
  {
  size_t pos;
  if ((pos = usage.find_first_of("$C")) != std::string::npos)
    {
    m_usage += usage.substr(0, pos);
    pos += 2;
    size_t z = m_usage.size();
    for (OvmsCommandMap::iterator it = m_children.begin(); ; )
      {
      if (!it->second->m_secure || writer->m_issecure)
        m_usage += it->first;
      if (++it == m_children.end())
        break;
      if (!it->second->m_secure || writer->m_issecure)
        m_usage += "|";
      }
    if (m_usage.size() == z)
      {
      m_usage = "All subcommands require 'enable' mode";
      pos = usage.size();       // Don't append any more
      }
    }
  else pos = 0;
  size_t pos2;
  if ((pos2 = usage.find_first_of("$G", pos)) != std::string::npos)
    {
    m_usage += usage.substr(pos, pos2-pos);
    pos2 += 2;
    size_t pos3;
    OvmsCommandMap::iterator it = m_children.end();
    if ((pos3 = usage.find_first_of("$", pos2)) != std::string::npos)
      {
      if (pos3 == pos2)
        it = m_children.begin();
      else
        it = m_children.find(usage.substr(pos2, pos3-pos2).c_str());
      pos = pos3 + 1;
      if (it != m_children.end() && (!it->second->m_secure || writer->m_issecure))
        {
        OvmsCommand* child = it->second;
        child->ExpandUsage(child->m_usage_template, writer);
        m_usage += child->m_usage;
        }
      }
    else
      pos = pos2;
    if (it == m_children.end())
      m_usage += "ERROR IN USAGE TEMPLATE";
    }
  m_usage += usage.substr(pos);
  }

OvmsCommand* OvmsCommand::RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                          const char *usage, int min, int max, bool secure)
  {
  OvmsCommand* cmd = new OvmsCommand(name, title, execute, usage, min, max, secure);
  m_children[name] = cmd;
  cmd->m_parent = this;
  //printf("Registered '%s' under '%s'\n",name,m_title);
  return cmd;
  }

char ** OvmsCommand::Complete(OvmsWriter* writer, int argc, const char * const * argv)
  {
  if (argc <= 1)
    {
    return writer->GetCompletion(m_children, argc > 0 ? argv[0] : "");
    }
  OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
  if (!cmd)
    {
    return writer->GetCompletion(m_children, NULL);
    }
  return cmd->Complete(writer, argc-1, ++argv);
  }

void OvmsCommand::Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv)
  {
//  if (argc>0)
//    {
//    printf("Execute(%s/%d) verbosity=%d argc=%d first=%s\n",m_title, m_children.size(), verbosity,argc,argv[0]);
//    }
//  else
//    {
//    printf("Execute(%s/%d) verbosity=%d (no args)\n",m_title, m_children.size(), verbosity);
//    }

  if (m_execute && (m_children.empty() || argc == 0))
    {
    //puts("Executing directly...");
    if (argc < m_min || argc > m_max || (argc > 0 && strcmp(argv[argc-1],"?")==0))
      {
      writer->puts(GetUsage(writer));
      return;
      }
    if ((!m_secure)||(m_secure && writer->m_issecure))
      m_execute(verbosity,writer,this,argc,argv);
    else
      writer->puts("Error: Secure command requires 'enable' mode");
    return;
    }
  else
    {
    //puts("Looking for a matching command");
    if (argc <= 0)
      {
      writer->puts("Subcommand required");
      writer->puts(GetUsage(writer));
      return;
      }
    if (strcmp(argv[0],"?")==0)
      {
      // Show available commands
      int avail = 0;
      for (OvmsCommandMap::iterator it=m_children.begin(); it!=m_children.end(); ++it)
        {
        if (it->second->IsSecure() && !writer->m_issecure)
          continue;
        const char* k = it->first;
        const char* v = it->second->GetTitle();
        writer->printf("%-20.20s %s\n",k,v);
        ++avail;
        }
      if (!avail)
        writer->printf("All subcommands require 'enable' mode\n");
      return;
      }
    OvmsCommand* cmd = m_children.FindUniquePrefix(argv[0]);
    if (!cmd)
      {
      writer->puts("Unrecognised command");
      if (!m_usage.empty())
        writer->puts(GetUsage(writer));
      return;
      }
    if (argc>1)
      {
      cmd->Execute(verbosity,writer,argc-1,++argv);
      }
    else
      {
      cmd->Execute(verbosity,writer,0,NULL);
      }
    }
  }

OvmsCommand* OvmsCommand::GetParent()
  {
  return m_parent;
  }

OvmsCommand* OvmsCommand::FindCommand(const char* name)
  {
  return m_children.FindUniquePrefix(name);
  }

void help(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("This isn't really much help, is it?");
  }

bool echoInsert(OvmsWriter* writer, void* ctx, char ch)
  {
  if (ch == '\n')
    return false;
  writer->write(&ch, 1);
  return true;
  }

void echo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Type characters to be echoed, end with newline.");
  writer->RegisterInsertCallback(echoInsert, NULL);
  }

void Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->Exit();
  }

void log_level(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* tag = "*";
  if (argc > 0)
    tag = argv[0];
  const char* title = cmd->GetTitle();
  esp_log_level_t level_num = (esp_log_level_t)(*(title+strlen(title)-2) - '0');
  esp_log_level_set(tag, level_num);
  writer->printf("Logging level for %s set to %s\n",tag,cmd->GetName());
  }

void log_file(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    if (ovms_log_file)
      {
      fclose(ovms_log_file);
      ovms_log_file = NULL;
      }
    writer->puts("Closed log file");
    return;
    }

  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  if (ovms_log_file)
    {
    writer->puts("Closing old log file");
    fclose(ovms_log_file);
    ovms_log_file = NULL;
    }

  ovms_log_file = fopen(argv[0], "a+");
  if (ovms_log_file == NULL)
    {
    writer->puts("Error: VFS file cannot be opened for append");
    return;
    }
  }

static OvmsCommand* monitor;
static OvmsCommand* monitor_yes;

void log_monitor(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool state;
  if (cmd == monitor)
    state = !writer->IsMonitoring();
  else
    state = (cmd == monitor_yes);
  writer->printf("Monitoring log messages %s\n", state ? "enabled" : "disabled");
  writer->SetMonitoring(state);
  }

typedef struct
  {
  std::string password;
  int tries;
  } PasswordContext;

bool enableInsert(OvmsWriter* writer, void* v, char ch)
  {
  PasswordContext* pc = (PasswordContext*)v;
  if (ch == '\n')
    {
    std::string p = MyConfig.GetParamValue("password","module");
    if (p.compare(pc->password) == 0)
      {
      writer->SetSecure(true);
      writer->printf("\nSecure mode");
      delete pc;
      return false;
      }
    if (++pc->tries == 3)
      {
      writer->printf("\nError: %d incorrect password attempts", pc->tries);
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      delete pc;
      return false;
      }
    writer->printf("\nSorry, try again.\nPassword:");
    pc->password.erase();
    return true;
    }
  if (ch == 'C'-0100)
    {
    delete pc;
    return false;
    }
  if (ch != '\r')
    pc->password.append(1, ch);
  return true;
  }

void enable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string p = MyConfig.GetParamValue("password","module");
  if ((p.empty())||((argc==1)&&(p.compare(argv[0])==0)))
    {
    writer->SetSecure(true);
    writer->puts("Secure mode");
    }
  else if (argc == 1)
    {
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    writer->puts("Error: Invalid password");
    }
  else
    {
    PasswordContext* pc = new PasswordContext;
    pc->tries = 0;
    writer->printf("Password:");
    writer->RegisterInsertCallback(enableInsert, pc);
    }
  }

void disable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->SetSecure(false);
  }

OvmsCommandApp::OvmsCommandApp()
  {
  ESP_LOGI(TAG, "Initialising COMMAND (1000)");

  ovms_log_file = NULL;
  m_root.RegisterCommand("help", "Ask for help", help, "", 0, 0);
  m_root.RegisterCommand("exit", "End console session", Exit , "", 0, 0);
  OvmsCommand* cmd_log = MyCommandApp.RegisterCommand("log","LOG framework",NULL, "", 0, 0, true);
  cmd_log->RegisterCommand("file", "Start logging to specified file", log_file , "<vfspath>", 0, 1, true);
  OvmsCommand* level_cmd = cmd_log->RegisterCommand("level", "Set logging level", NULL, "$C [<tag>]");
  level_cmd->RegisterCommand("verbose", "Log at the VERBOSE level (5)", log_level , "[<tag>]", 0, 1, true);
  level_cmd->RegisterCommand("debug", "Log at the DEBUG level (4)", log_level , "[<tag>]", 0, 1, true);
  level_cmd->RegisterCommand("info", "Log at the INFO level (3)", log_level , "[<tag>]", 0, 1, true);
  level_cmd->RegisterCommand("warn", "Log at the WARN level (2)", log_level , "[<tag>]", 0, 1);
  level_cmd->RegisterCommand("error", "Log at the ERROR level (1)", log_level , "[<tag>]", 0, 1);
  level_cmd->RegisterCommand("none", "No logging (0)", log_level , "[<tag>]", 0, 1);
  monitor = cmd_log->RegisterCommand("monitor", "Monitor log on this console", log_monitor , "[$C]", 0, 1, true);
  monitor_yes = monitor->RegisterCommand("yes", "Monitor log", log_monitor , "", 0, 0, true);
  monitor->RegisterCommand("no", "Don't monitor log", log_monitor , "", 0, 0, true);
  m_root.RegisterCommand("enable","Enter secure mode", enable, "[<password>]", 0, 1);
  m_root.RegisterCommand("disable","Leave secure mode", disable, "", 0, 0, true);
  m_root.RegisterCommand("echo", "Test getchar", echo, "", 0, 0);
  }

OvmsCommandApp::~OvmsCommandApp()
  {
  }

OvmsCommand* OvmsCommandApp::RegisterCommand(const char* name, const char* title, void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                                             const char *usage, int min, int max, bool secure)
  {
  return m_root.RegisterCommand(name, title, execute, usage, min, max, secure);
  }

OvmsCommand* OvmsCommandApp::FindCommand(const char* name)
  {
  return m_root.FindCommand(name);
  }

void OvmsCommandApp::RegisterConsole(OvmsWriter* writer)
  {
  m_consoles.insert(writer);
  }

void OvmsCommandApp::DeregisterConsole(OvmsWriter* writer)
  {
  m_consoles.erase(writer);
  }

int OvmsCommandApp::Log(const char* fmt, ...)
  {
  va_list args;
  va_start(args, fmt);
  size_t ret = Log(fmt, args);
  va_end(args);
  return ret;
  }

int OvmsCommandApp::Log(const char* fmt, va_list args)
  {
  LogBuffers* lb;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  PartialLogs::iterator it = m_partials.find(task);
  if (it == m_partials.end())
    lb = new LogBuffers();
  else
    {
    lb = it->second;
    m_partials.erase(task);
    }
  int ret = LogBuffer(lb, fmt, args);
  lb->set(m_consoles.size());
  for (ConsoleSet::iterator it = m_consoles.begin(); it != m_consoles.end(); ++it)
    {
    (*it)->Log(lb);
    }
  return ret;
  }

int OvmsCommandApp::LogPartial(const char* fmt, ...)
  {
  LogBuffers* lb;
  TaskHandle_t task = xTaskGetCurrentTaskHandle();
  PartialLogs::iterator it = m_partials.find(task);
  if (it == m_partials.end())
    {
    lb = new LogBuffers();
    m_partials[task] = lb;
    }
  else
    {
    lb = it->second;
    }
  va_list args;
  va_start(args, fmt);
  int ret = LogBuffer(lb, fmt, args);
  va_end(args);
  return ret;
  }

int OvmsCommandApp::LogBuffer(LogBuffers* lb, const char* fmt, va_list args)
  {
  char *buffer;
  int ret = vasprintf(&buffer, fmt, args);

  // replace CR/LF except last by "|", but don't leave '|' at the end
  char* s;
  for (s=buffer; *s; s++)
    {
    if ((*s=='\r' || *s=='\n') && *(s+1))
      *s = '|';
    }
  for (--s; s > buffer; --s)
    {
    if (*s=='\r' || *s=='\n')
      continue;
    if (*s != '|')
      break;
    *s++ = '\n';
    *s-- = '\0';
    }

  if (ovms_log_file)
    {
    // Log to the log file as well...
    fwrite(buffer,1,strlen(buffer),ovms_log_file);
    fflush(ovms_log_file);
    fsync(fileno(ovms_log_file));
    }
  lb->append(buffer);
  return ret;
  }

int OvmsCommandApp::HexDump(const char* tag, const char* prefix, const char* data, size_t length, size_t colsize /*=16*/)
  {
  char buffer[colsize*4 + 4]; // space for 16x3 + 2 + 16 + 1(\0)
  const char *s = data;
  int rlength = (int)length;

  while (rlength>0)
    {
    char *p = buffer;
    const char *os = s;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        sprintf(p,"%2.2x ",*s);
        s++;
        }
      else
        {
        sprintf(p,"   ");
        }
      p+=3;
      }
    sprintf(p,"| ");
    p += 2;
    s = os;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        if (isprint((int)*s))
          { *p = *s; }
        else
          { *p = '.'; }
        s++;
        }
      else
        {
        *p = ' ';
        }
      p++;
    }
    *p = 0;
    ESP_LOGV(tag, "%s: %s", prefix, buffer);
    rlength -= colsize;
    }

  return length;
  }

char ** OvmsCommandApp::Complete(OvmsWriter* writer, int argc, const char * const * argv)
  {
  return m_root.Complete(writer, argc, argv);
  }

void OvmsCommandApp::Execute(int verbosity, OvmsWriter* writer, int argc, const char * const * argv)
  {
  if (argc == 0)
    {
    writer->puts("Error: Empty command unrecognised");
    }
  else
    {
    m_root.Execute(verbosity, writer, argc, argv);
    }
  }
