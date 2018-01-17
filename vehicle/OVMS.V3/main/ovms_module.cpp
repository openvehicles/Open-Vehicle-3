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
static const char *TAG = "ovms-module";

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/FreeRTOSConfig.h"
#include "esp_heap_caps.h"
#include "esp_heap_debug.h"
#include <esp_system.h>
#include "ovms_module.h"
#include "ovms_command.h"

#define MAX_TASKS 30
#define DUMPSIZE 1000
#define NUMTASKS 32
#define NAMELEN 16
#define TASKLIST 10
#define NOT_FOUND (TaskHandle_t)0xFFFFFFFF

#ifndef CONFIG_HEAP_TASK_TRACKING
#define NOGO 1
#endif
#if configUSE_TRACE_FACILITY==0
#define NOGO 1
#endif
#ifdef CONFIG_FREERTOS_ASSERT_ON_UNTESTED_FUNCTION
#define NOGO 1
#endif

#ifdef NOGO
static void must(OvmsWriter* writer)
  {
  writer->printf("To use these debugging tools, must set CONFIG_HEAP_TASK_TRACKING=y\n");
  writer->printf("and have updated openvehicles/esp-idf\n");
  }

static void module_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

static void module_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

static void module_check(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  must(writer);
  }

void AddTaskToMap(TaskHandle_t task) {}

#else

class HeapTotals;

static TaskHandle_t* tasklist = NULL;
static TaskStatus_t* taskstatus = NULL;
static heap_dump_block_t* before = NULL;
static heap_dump_block_t* after = NULL;
static size_t numbefore = 0, numafter = 0;
static HeapTotals* changes = NULL;


class Name
  {
  public:
    inline Name() {}
    inline Name(const char *name)
      {
      for (int i = 0; i < NAMELEN/4; ++i)
        words[i] = 0;
      strncpy(bytes, name, NAMELEN-1);
      }
    inline Name(const Name& name)
      {
      for (int i = 0; i < NAMELEN/4; ++i)
        words[i] = name.words[i];
      bytes[NAMELEN-1] = '\0';
      }
    inline bool operator==(Name& a)
      {
      for (int i = 0; i < NAMELEN/4 - 1; ++i)
        if (a.words[i] != words[i]) return false;
      if (a.words[NAMELEN/4-1] != (words[NAMELEN/4-1] & 0x7FFFFFFF)) return false;
      return true;
      }
  public:
    union
      {
      char bytes[NAMELEN];
      int words[NAMELEN/4];
      };
  };


class TaskMap
  {
  private:
    TaskMap() : count(0) {}
    typedef struct
      {
      TaskHandle_t id;
      Name name;
      } TaskPair;

  public:
    inline static TaskMap* instance()
      {
      if (!taskmap)
        {
        void* p = heap_caps_malloc(sizeof(TaskMap), MALLOC_CAP_32BIT);
        if (!p)
          return NULL;
        taskmap = new(p) TaskMap;
        }
      return taskmap;
      }
    bool insert(TaskHandle_t taskid, const char* name)
      {
      int i;
      for (i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          break;
        }
      if (i == count)
        {
        if (count == NUMTASKS)
          return false;
        ++count;
        }
      map[i].id = taskid;
      map[i].name = Name(name);
      return true;
      }
    bool find(TaskHandle_t taskid, Name& name)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          {
          name = map[i].name;
          name.bytes[NAMELEN-1] = '\0';
          return true;
          }
        }
      sprintf(name.bytes, "%08X", (unsigned int)taskid);
      return false;
      }
    TaskHandle_t find(Name& name)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].name == name)
          return map[i].id;
        }
      char* end;
      TaskHandle_t task = (TaskHandle_t)strtoul(name.bytes, &end, 16);
      if (*end == '\0')
        return task;
      return NOT_FOUND;
      }
    UBaseType_t populate()
      {
      for (int i = 0; i < count; ++i)
        {
        map[i].name.words[3] |= 0x80000000;
        }
      UBaseType_t n = uxTaskGetSystemState(taskstatus, MAX_TASKS, NULL);
      for (UBaseType_t i = 0; i < n; ++i)
        {
        insert(taskstatus[i].xHandle, taskstatus[i].pcTaskName);
        }
      return n;
      }
    bool zero(TaskHandle_t taskid)
      {
      for (int i = 0; i < count; ++i)
        {
        if (map[i].id == taskid)
          {
          if (map[i].name.words[3] > 0)
            return false;
          for (++i ; i < count; ++i)
            {
            map[i-1] = map[i];
            }
          --count;
          return true;
          }
        }
      return false;
      }
    void dump()
      {
      for (int i = 0; i < count; ++i)
        {
        Name name = map[i].name;
        ::printf("taskmap %d %p %s\n", i, map[i].id, name.bytes);
        }
      }

  private:
    static TaskMap* taskmap;
    int count;
    TaskPair map[NUMTASKS];
  };
TaskMap* TaskMap::taskmap = NULL;


class FreeHeap
  {
  public:
    inline FreeHeap()
      {
      m_free_8bit = heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
      m_free_32bit = heap_caps_get_free_size(MALLOC_CAP_32BIT|MALLOC_CAP_INTERNAL) - m_free_8bit;
      m_free_spi = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
      if (!m_total_8bit)
        {
        m_total_8bit = TotalSize(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
        m_total_32bit = TotalSize(MALLOC_CAP_32BIT|MALLOC_CAP_INTERNAL) - FreeHeap::m_total_8bit;
        m_total_spi = TotalSize(MALLOC_CAP_SPIRAM);
        }
      }
    size_t Free8bit() { return m_free_8bit; }
    size_t Free32bit() { return m_free_32bit; }
    size_t FreeSPI() { return m_free_spi; }
    size_t Total8bit() { return m_total_8bit; }
    size_t Total32bit() { return m_total_32bit; }
    size_t TotalSPI() { return m_total_spi; }

  private:
    size_t TotalSize(uint32_t caps)
    {
    multi_heap_info_t info;
    heap_caps_get_info(&info, caps);
    size_t total = info.total_free_bytes + info.total_allocated_bytes +
      info.allocated_blocks*20 + info.free_blocks*4;
    return total;
    }

  private:
    size_t m_free_8bit;
    size_t m_free_32bit;
    size_t m_free_spi;
    static size_t m_total_8bit;
    static size_t m_total_32bit;
    static size_t m_total_spi;
  };
size_t FreeHeap::m_total_8bit = 0;
size_t FreeHeap::m_total_32bit = 0;
size_t FreeHeap::m_total_spi = 0;


class HeapTask
  {
  public:
    HeapTask()
      {
      totals.task = 0;
      for (int i = 0; i < NUM_USED_TYPES; ++i)
        totals.size[i] = 0;
      }
    heap_dump_totals_t totals;
  };


class HeapTotals
  {
  public:
    HeapTotals() : count(0) {}
    int begin() { return 0; }
    int end() { return count; }
    heap_dump_totals_t* array() { return &after[0].totals; }
    size_t* size() { return (size_t*)&count; }
    TaskHandle_t Task(int task) { return after[task].totals.task; }
    int Before(int task, int type) { return before[task].totals.size[type]; }
    int After(int task, int type) { return after[task].totals.size[type]; }
    void clear()
      {
      for (int i = 0; i < count; ++i)
        for (int j = 0; j < NUM_USED_TYPES; ++j)
          after[i].totals.size[j] = 0;
      }
    void transfer()
      {
      for (int i = 0; i < count; ++i)
        for (int j = 0; j < NUM_USED_TYPES; ++j)
          {
          before[i].totals.task = after[i].totals.task;
          before[i].totals.size[j] = after[i].totals.size[j];
          }
      }
    int find(TaskHandle_t task)
      {
      for (int i = 0; i < count; ++i)
        if (task == after[i].totals.task)
          return i;
      return -1;
      }
    void append(HeapTask& t)
      {
      if (count < NUMTASKS)
        {
        after[count] = t;
        ++count;
        }
      }

  private:
    HeapTask before[NUMTASKS];
    HeapTask after[NUMTASKS];
    int count;
  };


void AddTaskToMap(TaskHandle_t task)
  {
  TaskMap::instance()->insert(task, pcTaskGetTaskName(task));
  }


static void print_blocks(OvmsWriter* writer, TaskHandle_t task)
  {
  int count = 0, total = 0;
  bool separate = false;
  Name name;
  TaskMap* tm = TaskMap::instance();
  for (int i = 0; i < numbefore; ++i)
    {
    if (before[i].task != task)
      continue;
    int j = 0;
    for ( ; j < numafter; ++j)
      if (before[i].address == after[j].address && before[i].size == after[j].size)
        break;
    if (j == numafter)
      {
      tm->find(before[i].task, name);
      writer->printf("- t=%s s=%4d a=%p\n", name.bytes, before[i].size, before[i].address);
      ++count;
      }
    }
  if (count)
    separate = true;
  total += count;
  count = 0;
  for (int i = 0; i < numafter; ++i)
    {
    if (after[i].task != task)
      continue;
    int j = 0;
    for ( ; j < numbefore; ++j)
      if (after[i].address == before[j].address && after[i].size == before[j].size)
        break;
    if (j < numbefore)
      {
      int* p = (int*)after[i].address;
      if (separate)
        {
        writer->printf("----------------------------\n");
        separate = false;
        }
      tm->find(after[i].task, name);
      writer->printf("  t=%s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X\n",
        name.bytes, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
      ++count;
      }
    }
  if (count)
    separate = true;
  total += count;
  for (int i = 0; i < numafter; ++i)
    {
    if (after[i].task != task)
      continue;
    int j = 0;
    for ( ; j < numbefore; ++j)
      if (after[i].address == before[j].address && after[i].size == before[j].size)
        break;
    if (j == numbefore)
      {
      int* p = (int*)after[i].address;
      if (separate)
        {
        writer->printf("----------------------------\n");
        separate = false;
        }
      tm->find(after[i].task, name);
      writer->printf("+ t=%s s=%4d a=%p  %08X %08X %08X %08X %08X %08X %08X %08X\n",
        name.bytes, after[i].size, p, p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
      ++total;
      }
    }
  if (total)
    writer->printf("============================\n");
  }


static bool allocate()
  {
  if (!before)
    {
    before = (heap_dump_block_t*)heap_caps_malloc(sizeof(heap_dump_block_t)*DUMPSIZE, MALLOC_CAP_32BIT);
    after = (heap_dump_block_t*)heap_caps_malloc(sizeof(heap_dump_block_t)*DUMPSIZE, MALLOC_CAP_32BIT);
    taskstatus = (TaskStatus_t*)heap_caps_malloc(sizeof(TaskStatus_t)*MAX_TASKS, MALLOC_CAP_32BIT);
    tasklist = (TaskHandle_t*)heap_caps_malloc(sizeof(TaskHandle_t)*(TASKLIST), MALLOC_CAP_32BIT);
    void* p = heap_caps_malloc(sizeof(HeapTotals), MALLOC_CAP_32BIT);
    changes = new(p) HeapTotals();
    if (!before || !after || !taskstatus || !tasklist || !changes)
      {
      if (before)
        free(before);
      if (after)
        free(after);
      if (taskstatus)
        free(taskstatus);
      if (tasklist)
        free(tasklist);
      if (changes)
        delete(changes);
      return false;
      }
    }
  return true;
  }


static UBaseType_t get_tasks()
  {
  TaskMap* tm = TaskMap::instance();
  UBaseType_t numtasks = 0;
  if (tm)
    numtasks = tm->populate();
  return numtasks;
  }


static void get_memory(TaskHandle_t* tasks, size_t taskslen)
  {
  changes->clear();
  numafter = esp_heap_debug_dump_totals(changes->array(), changes->size(), NUMTASKS,
    tasks, taskslen, after, DUMPSIZE);
  }


static void module_memory(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  static const char* ctasks[] = {}; // { "tiT", "wifi"};
  const char* const* tasks = ctasks;
  bool all = false;
  if (argc > 0)
    {
    if (**argv == '*')
      tasks = NULL;
    else if (**argv == '=')
      {
      all = true;
      argc = sizeof(ctasks) / sizeof(char*);
      }
    else
      tasks = argv;
    }
  else
    argc = sizeof(ctasks) / sizeof(char*);
  if (!allocate())
    {
    writer->printf("Can't allocate storage for memory diagnostics\n");
    return;
    }
  get_tasks();
  TaskMap* tm = TaskMap::instance();
  TaskHandle_t* tl = NULL;
  size_t tln = 0;
  if (tasks)
    {
    tl = tasklist;
    for ( ; argc > 0; --argc, ++tasks)
      {
      Name name(*tasks);
      TaskHandle_t tk = tm->find(name);
      if (tk != NOT_FOUND)
        {
        *tl++ = tk;
        if (++tln == TASKLIST)
          break;
        }
      else
        writer->printf("Task %s unknown\n", *tasks);
      }
    tl = tasklist;
    }
  get_memory(tl, tln);

  FreeHeap now;
  writer->printf("Free 8-bit %zu/%zu, 32-bit %zu/%zu, SPIRAM %zu/%zu\n",
    now.Free8bit(), now.Total8bit(), now.Free32bit(), now.Total32bit(),
    now.FreeSPI(), now.TotalSPI());
  bool first = true;
  for (int i = changes->begin(); i < changes->end(); ++i)
    {
    int change[NUM_USED_TYPES];
    bool any = false;
    for (int j = 0; j < NUM_USED_TYPES; ++j)
      {
      change[j] = (*changes).After(i, j) - (*changes).Before(i, j);
      if (change[j])
        any = true;
      }
    if (any || all)
      {
      if (first)
        {
        writer->printf("--Task--     Total DRAM D/IRAM   IRAM SPIRAM"
          "   +/- DRAM D/IRAM   IRAM SPIRAM\n");
        first = false;
        }
      Name name("NoTaskMap");
      if (tm)
        tm->find((*changes).Task(i), name);
      writer->printf("%-15s %7d%7d%7d%7d    %+7d%+7d%+7d%+7d\n", name.bytes,
        (*changes).After(i, 0), (*changes).After(i, 1), (*changes).After(i, 2),
        (*changes).After(i, 3), change[0], change[1], change[2], change[3]);
      }
    }

  if (tln)
    {
    writer->printf("============================ blocks dumped = %d%s\n",
      numafter, numafter < DUMPSIZE ? "" : " (limited)");
    tl = tasklist;
    for (int i = 0; i < tln; ++i, ++tl)
      print_blocks(writer, *tl);
    }
  else if (!tasks)
    {
    for (int i = changes->begin(); i < changes->end(); ++i)
      {
      bool any = false;
      for (int j = 0; j < NUM_USED_TYPES; ++j)
        {
        if ((*changes).After(i, j) - (*changes).Before(i, j) != 0)
          any = true;
        }
      if (any)
        {
        writer->printf("============================ blocks dumped = %d%s\n",
          numafter, numafter < DUMPSIZE ? "" : " (limited)");
        print_blocks(writer, (*changes).Task(i));
        }
      }
    }

  for (int i = changes->begin(); i < changes->end(); ++i)
    {
    if (tm && (*changes).After(i, 0) == 0 && (*changes).After(i, 1) == 0 &&
      (*changes).After(i, 2) == 0 && (*changes).After(i, 3) == 0)
      tm->zero((*changes).Task(i));
    }
  changes->transfer();
  for (int i = 0; i < numafter; ++i)
    {
    before[i] = after[i];
    }
  numbefore = numafter;
  }


static void module_tasks(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  UBaseType_t num = uxTaskGetNumberOfTasks();
  writer->printf("Number of Tasks =%3u%s   Stack:  Now   Max Total    Heap 32-bit SPIRAM\n", num,
    num > MAX_TASKS ? ">max" : "    ");
  if (!allocate())
    {
    writer->printf("Can't allocate storage for task diagnostics\n");
    return;
    }
  UBaseType_t n = get_tasks();
  get_memory(tasklist, 0);
  num = 0;
  for (UBaseType_t j = 0; j < n; )
    {
    for (UBaseType_t i = 0; i < n; ++i)
      {
      if (taskstatus[i].xTaskNumber == num)
        {
        int k = changes->find(taskstatus[i].xHandle);
        int heaptotal = 0, heap32bit = 0, heapspi = 0;
        if (k >= 0)
          {
          heaptotal = (*changes).After(k, 0) + (*changes).After(k, 1);
          heap32bit = (*changes).After(k, 2);
          heapspi = (*changes).After(k, 3);
          }
        uint32_t total = (uint32_t)taskstatus[i].pxStackBase >> 16;
        writer->printf("Task %08X %2u %-15s %5u %5u %5u %7u%7u%7u\n", taskstatus[i].xHandle, taskstatus[i].xTaskNumber,
          taskstatus[i].pcTaskName, total - ((uint32_t)taskstatus[i].pxStackBase & 0xFFFF),
          total - taskstatus[i].usStackHighWaterMark, total, heaptotal, heap32bit, heapspi);
        ++j;
        break;
        }
      }
    ++num;
    }
  }

static void module_check(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  heap_caps_check_integrity_all(true);
  }
#endif // NOGO

static void module_fault(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  ESP_LOGI(TAG,"Abort faulting module (on command)");
  abort();
  }

static void module_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Resetting system...");
  esp_restart();
  }

class OvmsModuleInit
  {
  public:
  OvmsModuleInit()
    {
    ESP_LOGI(TAG, "Initialising MODULE (5100)");
#ifndef NOGO
	TaskMap::instance()->insert(0x00000000, "no task");
#endif

    OvmsCommand* cmd_module = MyCommandApp.RegisterCommand("module","Module framework",NULL);
    cmd_module->RegisterCommand("memory","Show module memory usage",module_memory,"[<task names or ids>]",0,TASKLIST,true);
    cmd_module->RegisterCommand("tasks","Show module task usage",module_tasks,"",0,0,true);
    cmd_module->RegisterCommand("fault","Abort fault the module",module_fault,"",0,0,true);
    cmd_module->RegisterCommand("reset","Reset module",module_reset,"",0,0,true);
    cmd_module->RegisterCommand("check","Check heap integrity",module_check,"",0,0,true);
    }
  } MyOvmsModuleInit  __attribute__ ((init_priority (5100)));

// Returns the value of the stack pointer in the calling function.
// The stack frame for this function is 32 bytes, hence the add.

void* stack() {
    __asm__(
"   addi a2, sp, 32\n"
"   retw.n\n"
    );
    return 0;
}
