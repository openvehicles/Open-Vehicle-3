
//
// Warning: don't edit - generated by generate_ecu_code.pl processing ../dev/eps_i1.json: EPS 30: Power steering
// This generated code  makes it easier to process CANBUS messages from the EPS ecu in a BMW i3
//

  case I3_PID_EPS_STEUERN_EPS_PULLDRIFT_OFFSET_RESET: {                           // 0xA2BB
    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_EPS_LENKWINKELSENSOR_KALIBRIERUNG_RESET: {                      // 0xAB69
    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_EPS_INITIALISIERUNG_SERVICE: {                                  // 0xAB6C
    if (datalen < 4) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_EPS_INITIALISIERUNG_SERVICE", 4);
        break;
    }

    unsigned char STAT_ROUTINE_STATUS = (RXBUF_UCHAR(0));
        // Execution status / Ausf�hrungsstatus
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_INITIALISIERUNG_SERVICE", "STAT_ROUTINE_STATUS", STAT_ROUTINE_STATUS, "\"0-n\"");

    short STAT_LENKRADWINKEL_WERT = (RXBUF_SINT(1));
        // Steering wheel angle / Lenkradwinkel
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "EPS", "EPS_INITIALISIERUNG_SERVICE", "STAT_LENKRADWINKEL_WERT", STAT_LENKRADWINKEL_WERT, "\"�\"");

    char STAT_SENSOR_ZUSTAND_NR = (RXBUF_SCHAR(3));
        // Condition sensor pinion angle sensor / Zustand Sensor Ritzelwinkelsensor
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_INITIALISIERUNG_SERVICE", "STAT_SENSOR_ZUSTAND_NR", STAT_SENSOR_ZUSTAND_NR, "\"0-n\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_STEUERN_EPS_MULTITURNWERT_RESET: {                              // 0xAB7D
    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_EPS_RITZELWINKELSENSOR: {                                       // 0xDB57
    if (datalen < 7) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_EPS_RITZELWINKELSENSOR", 7);
        break;
    }

    float STAT_RITZELWINKEL_WERT = (RXBUF_SINT32(0)/100.0f);
        // Pinion angle / Ritzelwinkel
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EPS", "EPS_RITZELWINKELSENSOR", "STAT_RITZELWINKEL_WERT", STAT_RITZELWINKEL_WERT, "\"�\"");

    short STAT_RITZELWINKELGESCHWINDIGKEIT_WERT = (RXBUF_SINT(4));
        // Pinion angle angular speed / Ritzelwinkel Winkelgeschwindigkeit
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%d%s\n", "EPS", "EPS_RITZELWINKELSENSOR", "STAT_RITZELWINKELGESCHWINDIGKEIT_WERT", STAT_RITZELWINKELGESCHWINDIGKEIT_WERT, "\"�/s\"");

    char STAT_SENSOR_ZUSTAND_NR_0XDB57 = (RXBUF_SCHAR(6));
        // Condition sensor pinion angle sensor / Zustand Sensor Ritzelwinkelsensor
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_RITZELWINKELSENSOR", "STAT_SENSOR_ZUSTAND_NR_0XDB57", STAT_SENSOR_ZUSTAND_NR_0XDB57, "\"0-n\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_EPS_MOMENTENSENSOR: {                                           // 0xDB99
    if (datalen < 3) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_EPS_MOMENTENSENSOR", 3);
        break;
    }

    float STAT_MOMENT_WERT = (RXBUF_SINT(0)/128.0f);
        // Current moment / Aktuelles Moment
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%.4f%s\n", "EPS", "EPS_MOMENTENSENSOR", "STAT_MOMENT_WERT", STAT_MOMENT_WERT, "\"Nm\"");

    char STAT_SENSOR_ZUSTAND_NR_0XDB99 = (RXBUF_SCHAR(2));
        // State of the steering torque sensor / Zustand Sensor Lenkmoment
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_MOMENTENSENSOR", "STAT_SENSOR_ZUSTAND_NR_0XDB99", STAT_SENSOR_ZUSTAND_NR_0XDB99, "\"0-n\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_EPS_ZAHNSTANGENMITTE: {                                         // 0xDC77
    if (datalen < 1) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_EPS_ZAHNSTANGENMITTE", 1);
        break;
    }

    unsigned char STAT_ZAHNSTANGENMITTE_ZUSTAND_NR = (RXBUF_UCHAR(0));
        // State of rack center learned / Zustand Zahnstangenmitte gelernt
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "EPS_ZAHNSTANGENMITTE", "STAT_ZAHNSTANGENMITTE_ZUSTAND_NR", STAT_ZAHNSTANGENMITTE_ZUSTAND_NR, "\"0-n\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_GELERNTER_ZAHNSTANGENWEG: {                                     // 0xDFDD
    if (datalen < 0) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_GELERNTER_ZAHNSTANGENWEG", 0);
        break;
    }

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_READHWMODIFICATIONINDEX: {                                      // 0xF152
    if (datalen < 2) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_READHWMODIFICATIONINDEX", 2);
        break;
    }

    unsigned char STAT_HW_MODIFICATION_INDEX_WERT = (RXBUF_UCHAR(0));
        // Index of hardware modification: FF: Not supported index / Index of hardware modification:  FF: Not supported
        // index
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "READHWMODIFICATIONINDEX", "STAT_HW_MODIFICATION_INDEX_WERT", STAT_HW_MODIFICATION_INDEX_WERT, "\"HEX\"");

    unsigned char BF_22_F152_SUPPLIERINFO = (RXBUF_UCHAR(1));
        // Supplier info tab / Tab Supplierinfo
        // BF_22_F152_SUPPLIERINFO is a BITFIELD of size unsigned char.  We don't yet generate definitions for each bit, we treat as the host data type
            // STAT_HWMODEL: Mask: 0xC0 - hardware model
            // STAT_SUPPLIERINFOFIELD: Mask: 0x3F - supplierInfo
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%lx%s\n", "EPS", "READHWMODIFICATIONINDEX", "BF_22_F152_SUPPLIERINFO", (unsigned long)BF_22_F152_SUPPLIERINFO, "\"Bit\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }

  case I3_PID_EPS_FLASH_UPDATE_MULTITURNZAEHLER: {                                // 0x1234
    if (datalen < 2) {
        ESP_LOGW(TAG, "Received %d bytes for %s, expected %d", datalen, "I3_PID_EPS_FLASH_UPDATE_MULTITURNZAEHLER", 2);
        break;
    }

    unsigned char STAT_SERVICE = (RXBUF_UCHAR(0));
        // Status of the service / Status des Service
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "FLASH_UPDATE_MULTITURNZAEHLER", "STAT_SERVICE", STAT_SERVICE, "\"0-n\"");

    unsigned char STAT_PIC = (RXBUF_UCHAR(1));
        // Status of the processor to be flashed / Status des zu flashenden Prozessors
    ESP_LOGD(TAG, "From ECU %s, pid %s: got %s=%x%s\n", "EPS", "FLASH_UPDATE_MULTITURNZAEHLER", "STAT_PIC", STAT_PIC, "\"0-n\"");

    // ==========  Add your processing here ==========
    hexdump(rxbuf, type, pid);

    break;
  }