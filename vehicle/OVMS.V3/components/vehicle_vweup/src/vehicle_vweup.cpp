#include "ovms_log.h"
#include "vehicle_vweup.h"

class OvmsVehicleVWeUpInit
{
public:
    OvmsVehicleVWeUpInit()
    {
        ESP_LOGI("vwup", "Registering Vehicle: VW e-Up (9000)");
        MyVehicleFactory.RegisterVehicle<VWeUpT26>("VWUP.T26", "VW e-Up (Komfort CAN)");
        MyVehicleFactory.RegisterVehicle<VWeUpObd>("VWUP.OBD", "VW e-Up (OBD2)");
    }
} OvmsVehicleVWeUpInit __attribute__((init_priority(9000)));