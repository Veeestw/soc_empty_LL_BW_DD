

#include "temperature.h"
#include "sl_sensor_rht.h"
void temperature_capteur_2_BLE(int16_t *point_temperature_BLE){
  int32_t temperature;
  uint32_t humidite;
  sl_sensor_rht_get(&humidite, &temperature);
  *point_temperature_BLE=temperature/10;
}
