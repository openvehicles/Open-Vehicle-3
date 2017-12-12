#include "ovms_log.h"
static const char *TAG = "ovms_main";

#include <string>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <string.h>
#include "ovms.h"
#include "ovms_peripherals.h"
#include "ovms_housekeeping.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_module.h"

extern "C"
  {
  void app_main(void);
  }

static class LogLevelSetter
  {
  public:
    inline LogLevelSetter()
      {
      ESP_LOGI(TAG, "Set default logging level for * to %s",
        CONFIG_LOG_DEFAULT_LEVEL == 5 ? "VERBOSE" :
        CONFIG_LOG_DEFAULT_LEVEL == 4 ? "DEBUG" :
        CONFIG_LOG_DEFAULT_LEVEL == 3 ? "INFO" :
        CONFIG_LOG_DEFAULT_LEVEL == 2 ? "WARN" :
        CONFIG_LOG_DEFAULT_LEVEL == 1 ? "ERROR" : "None");
      esp_log_level_set("*",(esp_log_level_t)CONFIG_LOG_DEFAULT_LEVEL);
      }
  } lss  __attribute__ ((init_priority (0150)));

uint32_t monotonictime = 0;
Housekeeping* MyHousekeeping = NULL;
Peripherals* MyPeripherals = NULL;

void app_main(void)
  {
  nvs_flash_init();

  ESP_LOGI(TAG, "Executing on CPU core %d",xPortGetCoreID());
  AddTaskToMap(xTaskGetCurrentTaskHandle());

  ESP_LOGI(TAG, "Mounting CONFIG...");
  MyConfig.mount();

  ESP_LOGI(TAG, "Registering default configs...");
  MyConfig.RegisterParam("vehicle", "Vehicle", true, true);

  ESP_LOGI(TAG, "Starting HOUSEKEEPING...");
  MyHousekeeping = new Housekeeping();
  }
