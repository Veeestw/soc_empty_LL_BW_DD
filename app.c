/***************************************************************************//**
 * @file
 * @brief Core application logic.
 *******************************************************************************
 * # License
 * <b>Copyright 2020 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "em_common.h"
#include "app_assert.h"
#include "sl_bluetooth.h"
#include "app.h"
#include "app_log.h"
#include "sl_sensor_rht.h"
#include "temperature.h"
#include "sl_simple_led_instances.h"

// The advertising set handle allocated from Bluetooth stack.
static uint8_t advertising_set_handle = 0xff;
static sl_sleeptimer_timer_handle_t handle_timer;
static int start = 0;
#define TEMPERATURE_TIMER_SIGNAL (1<<0)
static int32_t temperature=0;
static uint32_t humidite =0;
static int16_t temperature_BLE=0;
/**************************************************************************//**
 * Application Init.
 *****************************************************************************/
SL_WEAK void app_init(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application init code here!                         //
  // This is called once during start-up.                                    //
  /////////////////////////////////////////////////////////////////////////////
  app_log_info("%s\n", __FUNCTION__);

}

/**************************************************************************//**
 * Application Process Action.
 *****************************************************************************/
SL_WEAK void app_process_action(void)
{
  /////////////////////////////////////////////////////////////////////////////
  // Put your additional application code here!                              //
  // This is called infinitely.                                              //
  // Do not call blocking functions from here!                               //
  /////////////////////////////////////////////////////////////////////////////
}


void Fonction_callback(){ // Fonction de callback pour timer
  app_log_info("Timer step %d\n", start);
  start++;
  sl_bt_external_signal(TEMPERATURE_TIMER_SIGNAL);
}



/**************************************************************************//**
 * Bluetooth stack event handler.
 * This overrides the dummy weak implementation.
 *
 * @param[in] evt Event coming from the Bluetooth stack.
 *****************************************************************************/
void sl_bt_on_event(sl_bt_msg_t *evt)
{
  sl_status_t sc;

  switch (SL_BT_MSG_ID(evt->header)) {
    // -------------------------------
    // This event indicates the device has started and the radio is ready.
    // Do not call any stack command before receiving this boot event!
    case sl_bt_evt_system_boot_id:
      // Create an advertising set.
      sc = sl_bt_advertiser_create_set(&advertising_set_handle);
      app_assert_status(sc);

      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Set advertising interval to 100ms.
      sc = sl_bt_advertiser_set_timing(
        advertising_set_handle,
        160, // min. adv. interval (milliseconds * 1.6)
        160, // max. adv. interval (milliseconds * 1.6)
        0,   // adv. duration
        0);  // max. num. adv. events
      app_assert_status(sc);
      // Start advertising and enable connections.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    // -------------------------------
    // This event indicates that a new connection was opened.
    case sl_bt_evt_connection_opened_id:
      app_log_info("%s: Connection opened ! \n", __FUNCTION__);
      sc = sl_sensor_rht_init();
      app_assert_status(sc);
      sl_simple_led_init_instances();
      sl_led_led0.init(sl_led_led0.context);
      break;

    // -------------------------------
    // This event indicates that a connection was closed.
    case sl_bt_evt_connection_closed_id:
      app_log_info("%s: Connection closed ! \n", __FUNCTION__);
      sl_sensor_rht_deinit();
      // Generate data for advertising
      sc = sl_bt_legacy_advertiser_generate_data(advertising_set_handle,
                                                 sl_bt_advertiser_general_discoverable);
      app_assert_status(sc);

      // Restart advertising after client has disconnected.
      sc = sl_bt_legacy_advertiser_start(advertising_set_handle,
                                         sl_bt_legacy_advertiser_connectable);
      app_assert_status(sc);
      break;

    ///////////////////////////////////////////////////////////////////////////
    // Add additional event handlers here as your application requires!      //
    ///////////////////////////////////////////////////////////////////////////
    case sl_bt_evt_gatt_server_user_read_request_id :

      sc = sl_sensor_rht_get(&humidite, &temperature);
      app_assert_status(sc);
      //Affichage console programmeur
      app_log_info(" Temperature : %ld,%ld°C\n", temperature/1000,temperature%1000);
      app_log_info(" Humidite :  %ld,%ld %% \n", humidite/1000,humidite%1000);

      //Conversion en BLE :
      //Format BLE :
      //R = C * M * 10^d * 2^b avec M = 1; d = -2; b = 0
      //int16_t temperature_BLE;
      //temperature_BLE = temperature/10;
      //app_log_info(" Temperature format BLE :  %d °C\n", temperature_BLE);


      temperature_capteur_2_BLE(&temperature_BLE);
      app_log_info(" Temperature format BLE (par fonction temperature.c) :  %d °C\n", temperature_BLE);

      //Verification de la demande au capteur de temeprature :
      if ((*evt).data.evt_gatt_server_user_read_request.characteristic==27){
          app_log_info("Acces lecture temperature accepte !\n");
          uint16_t octets_data_envoyees;
          sc = sl_bt_gatt_server_send_user_read_response((*evt).data.evt_gatt_server_user_read_request.connection,
                                                         (*evt).data.evt_gatt_server_user_read_request.characteristic,
                                                         0,
                                                         sizeof(temperature_BLE),
                                                         (uint8_t*) &temperature_BLE,
                                                         &octets_data_envoyees);
          app_assert_status(sc);
          app_log_info(" Combien d'octets ont ete envoyes : %d\n", octets_data_envoyees);
      }
      else app_log_info("Acces lecture temperature refuse !\n");

      break;


    case sl_bt_evt_gatt_server_characteristic_status_id :
      app_log_info("Bouton Notify appuyé ");
      if((*evt).data.evt_gatt_server_characteristic_status.characteristic == 27){
          app_log_info("+ de Temperature ");
          if((*evt).data.evt_gatt_server_characteristic_status.status_flags == 0x01){
              app_log_info("+ la config a ete changee !\n");
              if ((*evt).data.evt_gatt_server_characteristic_status.client_config_flags == 1){ //Btn notif actif

                  sl_sleeptimer_start_periodic_timer_ms(&handle_timer,1000,Fonction_callback,(void *)NULL,0,0);

              }
              else if((*evt).data.evt_gatt_server_characteristic_status.client_config_flags == 0){ ////Btn notif desactif
                  sl_sleeptimer_stop_timer(&handle_timer);
                  start = 0;

              }
          }
      }
      break;

    case sl_bt_evt_system_external_signal_id :
      if((*evt).data.evt_system_external_signal.extsignals == TEMPERATURE_TIMER_SIGNAL){
          sc=sl_sensor_rht_get(&humidite, &temperature);
          app_assert_status(sc);
          temperature_capteur_2_BLE(&temperature_BLE);
          sc=sl_bt_gatt_server_send_notification((*evt).data.evt_gatt_server_characteristic_status.connection,
                                              27,
                                              sizeof(temperature_BLE),
                                              (uint8_t*) &temperature_BLE);
          app_assert_status(sc);
      }
      break;

    case sl_bt_evt_gatt_server_user_write_request_id :
      if((*evt).data.evt_gatt_server_user_write_request.characteristic==31){
          app_log_info("ECRITURE : \n");
          uint8_t  val_len = (*evt).data.evt_gatt_server_user_write_request.value.len;
          app_log_info("val_len : %d\n", val_len);
          uint8_t* val_data = (*evt).data.evt_gatt_server_user_write_request.value.data;
          app_log_info("val_data : %d \n", *val_data);
          switch(*val_data){
            case 1 :
              if((*evt).data.evt_gatt_server_user_write_request.att_opcode==0x12){
                  sl_led_led0.turn_on(sl_led_led0.context);
                  uint8_t test = (*evt).data.evt_gatt_server_user_write_request.att_opcode;
                  app_log_info("opcode : %d \n", test);
                  sc=sl_bt_gatt_server_send_user_write_response((*evt).data.evt_gatt_server_user_write_request.connection,
                                                             (*evt).data.evt_gatt_server_user_write_request.characteristic,
                                                             0);
                  app_assert_status(sc);
              }else{

                  sl_led_led0.turn_on(sl_led_led0.context);

              }

              break;

            case 0 :

              if((*evt).data.evt_gatt_server_user_write_request.att_opcode==0x12){
                  sl_led_led0.turn_off(sl_led_led0.context);
                  sc=sl_bt_gatt_server_send_user_write_response((*evt).data.evt_gatt_server_user_write_request.connection,
                                                             (*evt).data.evt_gatt_server_user_write_request.characteristic,
                                                             0);
                  app_assert_status(sc);
              }else{
                  sl_led_led0.turn_off(sl_led_led0.context);
              }
              break;

            case 2 :

              break;

            case 3 :

              break;

            default:
              break;

          }
      }


      break;


    // -------------------------------
    // Default event handler.
    default:
      break;
  }
}
