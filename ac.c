//#include <stdio.h>
//#include <string.h>
#include <esp/uart.h>
#include <rboot-api.h>
#include <sysparam.h>
#include <esplibs/libmain.h>

#include <homekit/homekit.h>
#include <homekit/characteristics.h>
#include <wifi_config.h>
#include <led_codes.h>
#include <adv_button.h>
#include <led_status.h>

#include <dht/dht.h>
#include <ds18b20/ds18b20.h>

//#include "../common/custom_characteristics.h"
#include <ir/ir.h>
#include <ir/raw.h>
//#include "http.h"
 #include <udplogger.h>

#include <lwip/dhcp.h>


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
#include <unistd.h>
#include <string.h>

#include <espressif/esp_common.h>

#include "FreeRTOS.h"
#include "task.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "lwip/api.h"

#include "ac_commands.h"

#define UDPLOG_PRINTF_TO_UDP
#define UDPLOG_PRINTF_ALSO_SERIAL
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

#define DEVICE_MANUFACTURER "Unknown"
#define DEVICE_NAME "MultiSensor"
#define DEVICE_MODEL "esp8266"
char serial_value[13];  //Device Serial is set upon boot based on MAC address
#define FW_VERSION "0.0.1"


//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

// add this section to make your device OTA capable
// create the extra characteristic &ota_trigger, at the end of the primary service (before the NULL)
// it can be used in Eve, which will show it, where Home does not
// and apply the four other parameters in the accessories_information section

#include "ota-api.h"
homekit_characteristic_t ota_trigger  = API_OTA_TRIGGER;
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

const int led_gpio = 2; // Pin D4
const int button_gpio = 4; //Pin (D2)
const int motion_sensor_gpio = 16; // Wemos D1 mini pin: D0
const int SENSOR_PIN = 5; //DHT sensor on pin D1
const int IR_PIN = 14;  // Wemos D1 mini pin: D5. (Just for Reference)

//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

//####################################################################################
//LED codes
// 1000ms ON, 1000ms OFF
led_status_pattern_t waiting_wifi = LED_STATUS_PATTERN({1500, -500});

// one short blink every 3 seconds
led_status_pattern_t normal_mode = LED_STATUS_PATTERN({100, -2900});

// one short blink every 3 seconds
led_status_pattern_t unpaired  = LED_STATUS_PATTERN({100, -100, 100, -100, 100, -700});

led_status_pattern_t pairing_started  = LED_STATUS_PATTERN({400, -200,400, -200, 400, -200, 100, -700});


// three short blinks
led_status_pattern_t three_short_blinks = LED_STATUS_PATTERN({100, -100, 100, -100, 100, -700});

#define STATUS_LED_PIN 2

static led_status_t led_status;
//####################################################################################


void on_update(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
        update_state();
}

void on_temp_update(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
        update_temp();
}


void on_active_change(homekit_characteristic_t *ch, homekit_value_t value, void *context) {
        update_active();
}

////%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%



//Temperature, Humidity sensors
homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
//AC Required Parameters
homekit_characteristic_t active = HOMEKIT_CHARACTERISTIC_(ACTIVE, 0,.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_active_change));
homekit_characteristic_t current_temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t current_heater_cooler_state = HOMEKIT_CHARACTERISTIC_(CURRENT_HEATER_COOLER_STATE, 0);
homekit_characteristic_t target_heater_cooler_state = HOMEKIT_CHARACTERISTIC_(TARGET_HEATER_COOLER_STATE, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update));
//optionals
//homekit_characteristic_t units = HOMEKIT_CHARACTERISTIC_(TEMPERATURE_DISPLAY_UNITS, 0);
homekit_characteristic_t cooling_threshold = HOMEKIT_CHARACTERISTIC_(COOLING_THRESHOLD_TEMPERATURE, 18,.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_temp_update),.min_value = (float[]) {18},.max_value = (float[]) {30},.min_step = (float[]) {1} );
homekit_characteristic_t heating_threshold = HOMEKIT_CHARACTERISTIC_(HEATING_THRESHOLD_TEMPERATURE, 25,.callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_temp_update),.min_value = (float[]) {18},.max_value = (float[]) {30},.min_step = (float[]) {1});
//homekit_characteristic_t rotation_speed = HOMEKIT_CHARACTERISTIC_(ROTATION_SPEED, 0, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(on_update));

//Device Information
homekit_characteristic_t manufacturer     = HOMEKIT_CHARACTERISTIC_(MANUFACTURER,  DEVICE_MANUFACTURER);
homekit_characteristic_t serial           = HOMEKIT_CHARACTERISTIC_(SERIAL_NUMBER, serial_value);
homekit_characteristic_t model            = HOMEKIT_CHARACTERISTIC_(MODEL,         DEVICE_MODEL);
homekit_characteristic_t revision         = HOMEKIT_CHARACTERISTIC_(FIRMWARE_REVISION,  FW_VERSION);
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
//AC related code
void update_state() {
        uint8_t target_state = target_heater_cooler_state.value.int_value;
        uint8_t active_status = active.value.int_value;
        uint8_t current_state = current_heater_cooler_state.value.int_value;
        //printf("Running update_state() \n" );
        printf("UPDATE STATE: Active State: %d, Current State: %d, Target State: %d \n",active_status, current_state, target_state );


        //current_heater_cooler_state.value = HOMEKIT_UINT8(target_state+1);
        //homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
        sysparam_set_int8("ac_mode",target_state);
        current_ac_mode = target_state;
        stored_active_state = active_status;

        if (active_status == 0) {
                //	printf("OFF\n" );
                //	ac_button_off();
        }
        else
        {
                if(target_state == 0) {
                        taskENTER_CRITICAL();
                        ac_button_aut();
                        taskEXIT_CRITICAL();
                        led_status_signal(led_status, &three_short_blinks);
                }
                else if ((target_state + 1 )!=current_state) {
                        //printf("AC is now ON, updating temp\n" );
                        update_temp();
                }
        }

}
void update_temp() {
        //TARGET:
        //0 = AUTOMATIC
        //1 = HEAT
        //2 = COOL

        //CURRENT STATE:
        //0 = inactive
        //1 = idle
        //2 = heating
        //3 = cooling

        //printf("Running update_temp() \n" );
        uint8_t target_state = target_heater_cooler_state.value.int_value;
        uint8_t active_status = active.value.int_value;
        uint8_t current_state = current_heater_cooler_state.value.int_value;
        printf("UPDATE TEMP: Active Status: %d, Current State: %d, Target State: %d \n",active_status, current_state, target_state );


        float target_temp1 = 0;
        target_state = (int)target_state;

        if (target_state == 0)//AUTOMATIC
        {
                if (active_status ==1)
                {
                        printf("This should normally sent AUTO command\n" );
                        //Auto mode
                        current_heater_cooler_state.value = HOMEKIT_UINT8(target_state);
                        homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
                }

                /* code */
        }
        else if (target_state == 1) { //HEAT
                //Read the Heat target
                target_temp1= heating_threshold.value.float_value;
                current_heater_cooler_state.value = HOMEKIT_UINT8(target_state+1);
                homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
        }
        else if (target_state == 2) //COOLING
        {
                //Else read the Cool target
                target_temp1= cooling_threshold.value.float_value;
                current_heater_cooler_state.value = HOMEKIT_UINT8(target_state+1);
                homekit_characteristic_notify(&current_heater_cooler_state, current_heater_cooler_state.value);
        }
        current_ac_temp = (int)target_temp1;
        printf("Target temp: %.1f\n",target_temp1 );

        sysparam_set_int8("ac_mode",target_state);
        sysparam_set_int8("ac_temp",(int)target_temp1);

        taskENTER_CRITICAL();
        ac_command(target_state,target_temp1);
        taskEXIT_CRITICAL();

        led_status_signal(led_status, &three_short_blinks);

}

void update_active() {

        //printf("\nRunning update_active() \n" );
        uint8_t target_state = target_heater_cooler_state.value.int_value;
        uint8_t active_status = active.value.int_value;
        uint8_t current_state = current_heater_cooler_state.value.int_value;
        printf("\nUPDATE ACTIVE: Active Status: %d, Current State: %d, Target State: %d \n",active_status, current_state, target_state );

        //If its being requested to turn ON and saved status is different, then send IR command

        if (active_status == 1  )
        {
                if (target_state == 0)
                {
                        taskENTER_CRITICAL();
                        ac_button_aut();
                        taskEXIT_CRITICAL();
                        led_status_signal(led_status, &three_short_blinks);
                }
                else
                {
                        update_temp();
                }
        }

        //If it is requested to turn off
        if (active_status == 0  ) {
                //	printf("OFF\n" );
                taskENTER_CRITICAL();
                ac_button_off();
                taskEXIT_CRITICAL();
                led_status_signal(led_status, &three_short_blinks);
        }


        stored_active_state = active_status;

        //save ON/OFF status to sysparameters
        sysparam_set_int8("ac_active",stored_active_state);


}

void int_ac_state(){
//At reboot, set initial AC state from saved parameters
        sysparam_status_t status;

        printf("Initializing AC parameters\n");

        status = sysparam_get_int8("ac_mode", &current_ac_mode);
        if (status == SYSPARAM_OK) {

                int new_current_status = 1;
                if (current_ac_mode == 1) {
                        //Cool
                        new_current_status =2;

                        status = sysparam_get_int8("ac_temp", &current_ac_temp);
                        if (status == SYSPARAM_OK) {
                                heating_threshold.value = HOMEKIT_FLOAT((float)current_ac_temp);
                        }

                }
                if (current_ac_mode == 2)
                {
                        //Heating
                        new_current_status =3;
                        status = sysparam_get_int8("ac_temp", &current_ac_temp);
                        if (status == SYSPARAM_OK) {
                                printf("Temp:%d \n",current_ac_temp);
                                cooling_threshold.value = HOMEKIT_FLOAT((float)current_ac_temp);

                        }
                }

                current_heater_cooler_state.value = HOMEKIT_UINT8(new_current_status);
                target_heater_cooler_state.value = HOMEKIT_UINT8(current_ac_mode);

        }


        status = sysparam_get_int8("ac_active", &stored_active_state);
        if (status == SYSPARAM_OK) {
                active.value = HOMEKIT_UINT8(stored_active_state);
        }


}
//%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%




void identify_task(void *_args) {
        vTaskDelete(NULL);
}

void identify(homekit_value_t _value) {
        printf("identify\n");
        xTaskCreate(identify_task, "identify", 128, NULL, 2, NULL);
        sensor_worker();
        vTaskDelay(100 / portTICK_PERIOD_MS);
}



void led_write(bool on) {
        gpio_write(led_gpio, on ? 0 : 1);
        led_value = on; //Keep track of the status
}


void reset_configuration_task() {
        //Flash the LED first before we start the reset
        for (int i=0; i<3; i++) {
                led_write(true);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                led_write(false);
                vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        printf("Resetting Wifi Config\n");
        wifi_config_reset();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Resetting HomeKit Config\n");
        homekit_server_reset();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        printf("Restarting\n");
        sdk_system_restart();

        vTaskDelete(NULL);
}

void reset_configuration() {
        printf("Resetting Sonoff configuration\n");
        xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
        vTaskDelay(100 / portTICK_PERIOD_MS);
}


void sensor_worker() {
//Temperature measurement
        float humidity_value, temperature_value;
        bool get_temp = false;

        get_temp = dht_read_float_data(DHT_TYPE_DHT22, SENSOR_PIN, &humidity_value, &temperature_value);
        if (get_temp) {
                // printf("RC >>> Sensor: temperature %g, humidity %g\n", temperature_value, humidity_value);

                if (temperature_value != old_temperature_value) {
                        old_temperature_value = temperature_value;

                        current_temperature.value = HOMEKIT_FLOAT(temperature_value); //Update AC current temp
                        homekit_characteristic_notify(&current_temperature, HOMEKIT_FLOAT(temperature_value));

                        temperature.value.float_value = temperature_value;
                        homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
                }


                if (humidity_value != old_humidity_value) {
                        old_humidity_value = humidity_value;
                        humidity.value.float_value =humidity_value;
                        //current_humidity.value = HOMEKIT_FLOAT(humidity_value);
                        //homekit_characteristic_notify(&current_humidity, current_humidity.value);
                        homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
                }

        } else
        {
                printf("RC !!! ERROR Sensor\n");

                //led_code(LED_GPIO, SENSOR_ERROR);

        }
}



homekit_accessory_t *accessories[] = {
        HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                        &name,
                        &manufacturer,
                        &serial,
                        &model,
                        &revision,
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(HEATER_COOLER, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "AirConditioner"),
                        &active,
                        &current_temperature,
                        //&target_temperature,
                        &current_heater_cooler_state,
                        &target_heater_cooler_state,
                        &cooling_threshold,
                        &heating_threshold,
                        //          &units,
//          &rotation_speed,
                        &ota_trigger,
                        NULL
                }),
                NULL
        }),
        HOMEKIT_ACCESSORY(.category=homekit_accessory_category_sensor, .services=(homekit_service_t*[]){
                HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                        &name,
                        &manufacturer,
                        &serial,
                        &model,
                        &revision,
                        HOMEKIT_CHARACTERISTIC(IDENTIFY, temperature_sensor_identify),
                        NULL
                }),
                HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
                        &temperature,
                        NULL
                }),
                HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
                        HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
                        &humidity,
                        NULL
                }),
                NULL
        }),
        NULL

};




void create_accessory_name() {
        uint8_t macaddr[6];
        sdk_wifi_get_macaddr(STATION_IF, macaddr);

        int name_len = snprintf(NULL, 0, "HomekitSensor-%02X%02X%02X",
                                macaddr[3], macaddr[4], macaddr[5]);
        char *name_value = malloc(name_len+1);
        snprintf(name_value, name_len+1, "HomekitSensor-%02X%02X%02X",
                 macaddr[3], macaddr[4], macaddr[5]);

        name.value = HOMEKIT_STRING(name_value);


        snprintf(serial_value, 13, "%02X%02X%02X%02X%02X%02X", macaddr[0], macaddr[1], macaddr[2], macaddr[3], macaddr[4], macaddr[5]);

}

homekit_server_config_t config = {
        .accessories = accessories,
        .password = "222-22-222",
        .on_event = on_event,
};

void on_wifi_event(wifi_config_event_t event) {


        if (event == WIFI_CONFIG_CONNECTED)
        {
                printf("CONNECTED TO >>> WIFI <<<\n");
                //led_status_signal(led_status, &three_short_blinks); //This seemed like needed, otherwise the library does not work well.
                led_status_set(led_status, NULL);

                //led_status_set(led_status,NULL);
                Wifi_Connected=true;

                create_accessory_name();
                //  if (homekit_is_paired()){
                //  char *custom_hostname = name.value.string_value;
                //  struct netif *netif = sdk_system_get_netif(STATION_IF);
                //    if (netif) {
                //      printf("HOSTNAME set>>>>>:%s\n", custom_hostname);
                //      LOCK_TCPIP_CORE();
                //      dhcp_release_and_stop(netif);
                //      netif->hostname = "foobar";
                //      netif->hostname =custom_hostname;
                //      dhcp_start(netif);
                //      UNLOCK_TCPIP_CORE();
                //     }
                // }
                //OTA
                int c_hash=ota_read_sysparam(&manufacturer.value.string_value,&serial.value.string_value,
                                             &model.value.string_value,&revision.value.string_value);


                c_hash=1; revision.value.string_value="0.0.1"; //cheat line
                config.accessories[0]->config_number=c_hash;



                homekit_server_init(&config);
        }
        else if (event == WIFI_CONFIG_DISCONNECTED)
        {
                Wifi_Connected = false;
                printf("DISCONNECTED FROM >>> WIFI <<<\n");
                led_status_set(led_status,&waiting_wifi);
        }
}



void hardware_init() {
        gpio_enable(led_gpio, GPIO_OUTPUT);
        //  led_write(true);

        gpio_set_pullup(SENSOR_PIN, false, false);
        gpio_set_pullup(IR_PIN, false, false);

        // IR Common initialization (can be done only once)
        ir_tx_init();

        led_status = led_status_init(STATUS_LED_PIN, 0);



}


void user_init(void) {
        uart_set_baud(0, 115200);
        printf("SDK version:%s\n", sdk_system_get_sdk_version());
        hardware_init();



        //wifi_config_init("Homekit-sensor", NULL, on_wifi_ready);
        wifi_config_init2("Homekit-Sensor", NULL, on_wifi_event);

}
