// Copyright (c) 2015-2019 The HomeKit ADK Contributors
//
// Licensed under the Apache License, Version 2.0 (the “License”);
// you may not use this file except in compliance with the License.
// See [CONTRIBUTORS.md] for the list of HomeKit ADK project authors.

// An example that implements the light bulb HomeKit profile. It can serve as a basic implementation for
// any platform. The accessory logic implementation is reduced to internal state updates and log output.
//
// This implementation is platform-independent.
//
// The code consists of multiple parts:
//
//   1. The definition of the accessory configuration and its internal state.
//
//   2. Helper functions to load and save the state of the accessory.
//
//   3. The definitions for the HomeKit attribute database.
//
//   4. The callbacks that implement the actual behavior of the accessory, in this
//      case here they merely access the global accessory state variable and write
//      to the log to make the behavior easily observable.
//
//   5. The initialization of the accessory state.
//
//   6. Callbacks that notify the server in case their associated value has changed.

#include "HAP.h"

#include "App.h"
#include "DB.h"
#include <stdio.h>
#include <string.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
// #include <Adafruit_DPS310.h>

#include "esp_log.h"
static const char *TAG = "app";

#include <dht.h>
static const gpio_num_t dht_gpio = 25;
static const dht_sensor_type_t sensor_type = DHT_TYPE_AM2301;

#include "u8g2_esp32_hal.h"
#define GPIO_OLED_SDA 23
#define GPIO_OLED_SCL 22
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Domain used in the key value store for application data.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreDomain_Configuration ((HAPPlatformKeyValueStoreDomain) 0x00)

/**
 * Key used in the key value store to store the configuration state.
 *
 * Purged: On factory reset.
 */
#define kAppKeyValueStoreKey_Configuration_State ((HAPPlatformKeyValueStoreDomain) 0x00)

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Global accessory configuration.
 */
typedef struct {
    struct {
        bool lightBulbOn;
        float temperature;
        float humidity;
        uint8_t airquality;
        uint32_t airval;
        char airstr[50];
    } state;
    HAPAccessoryServerRef* server;
    HAPPlatformKeyValueStoreRef keyValueStore;
} AccessoryConfiguration;

static AccessoryConfiguration accessoryConfiguration;


// HAPService* current_service;
// HAPCharacteristic* current_characteristic;


/**
 * Global u8g2
 */
u8g2_t u8g2;


/**
 * Print values on oled display
 */
void PrintOnScreen() {

    // float temp = accessoryConfiguration.state.temperature;
    // float hum = accessoryConfiguration.state.humidity;
    // uint32_t air_val = accessoryConfiguration.state.airval;
    // char* air_str = accessoryConfiguration.state.airstr;

    u8g2_ClearBuffer(&u8g2);
    u8g2_SetFont(&u8g2, u8g2_font_timR14_tf);
    // u8g2_DrawStr(&u8g2, 80, 20, "00:00");

    char str[100];

    u8g2_SetFont(&u8g2, u8g2_font_fub35_tf);
    sprintf(str, "%.1f \260", accessoryConfiguration.state.temperature);
    u8g2_DrawStr(&u8g2, 0, 50, str);

    // Drop icon
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_all_2x_t);
    u8g2_DrawGlyph(&u8g2, 0, 90, 152);

    u8g2_SetFont(&u8g2, u8g2_font_fub17_tf);
    sprintf(str, "%.1f %%", accessoryConfiguration.state.humidity);
    u8g2_DrawStr(&u8g2, 22, 90, str);

    // Fire icon
    u8g2_SetFont(&u8g2, u8g2_font_open_iconic_all_2x_t);
    u8g2_DrawGlyph(&u8g2, 0, 120, 168);

    u8g2_SetFont(&u8g2, u8g2_font_fub17_tf);
    sprintf(str, "%s", accessoryConfiguration.state.airstr);
    u8g2_DrawStr(&u8g2, 22, 120, str);
    // sprintf(str, "  value: %d", accessoryConfiguration.state.airval);
    // u8g2_DrawStr(&u8g2, 0, 120, str);



    // u8g2_SetFont(&u8g2, u8g2_font_timR24_tf);
    // u8g2_DrawStr(&u8g2, 0, 40, "-");

    // // u8g2_DrawStr(&u8g2, 80, 20, "00:00");
    // u8g2_SetFont(&u8g2, u8g2_font_timR14_tf);
    // // u8g2_DrawStr(&u8g2, 0, 40, "Temp: -");
    // u8g2_DrawStr(&u8g2, 0, 70, "Humidity: -");
    // u8g2_DrawStr(&u8g2, 0, 100, "Air: -");

    u8g2_SendBuffer(&u8g2);
}

/**
 * Calculates air quality from read ADC value
 */
void CalcAirQuality() {

    uint32_t reading = accessoryConfiguration.state.airval;

    if (reading < 1800) {
        accessoryConfiguration.state.airquality = 1; // Excellent
        strcpy(accessoryConfiguration.state.airstr, "Excellent");
    } else if (reading < 1900) {
        accessoryConfiguration.state.airquality = 2; // Good
        strcpy(accessoryConfiguration.state.airstr, "Good");
    } else if (reading < 2000) {
        accessoryConfiguration.state.airquality = 3; // Fair
        strcpy(accessoryConfiguration.state.airstr, "Fair");
    } else if (reading < 2100) {
        accessoryConfiguration.state.airquality = 4; // Inferior
        strcpy(accessoryConfiguration.state.airstr, "Inferior");
    } else {
        accessoryConfiguration.state.airquality = 5; // Poor
        strcpy(accessoryConfiguration.state.airstr, "Poor");
    }
}


TickType_t xLastDHTread;

void readDHT() {
    // Don't read the DHT sensor in less than 0.5 seconds
    int time_diff = (xTaskGetTickCount() - xLastDHTread) * portTICK_PERIOD_MS;
    if (time_diff < 500) {
        return;
    }

    esp_err_t err = dht_read_float_data(sensor_type,
                                        dht_gpio,
                                        &accessoryConfiguration.state.humidity,
                                        &accessoryConfiguration.state.temperature);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "readDHT - Humidity: %.1f%% Temp: %.1f\n",
            accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    } else {
        ESP_LOGW(TAG, "readDHT - Could not read data from sensor\n");
    }

    xLastDHTread = xTaskGetTickCount();
}



//----------------------------------------------------------------------------------------------------------------------

/**
 * Load the accessory state from persistent memory.
 */
static void LoadAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;

    // Load persistent state if available
    bool found;
    size_t numBytes;

    err = HAPPlatformKeyValueStoreGet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state,
            &numBytes,
            &found);

    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
    if (!found || numBytes != sizeof accessoryConfiguration.state) {
        if (found) {
            HAPLogError(&kHAPLog_Default, "Unexpected app state found in key-value store. Resetting to default.");
        }
        HAPRawBufferZero(&accessoryConfiguration.state, sizeof accessoryConfiguration.state);
    }
}

/**
 * Save the accessory state to persistent memory.
 */
static void SaveAccessoryState(void) {
    HAPPrecondition(accessoryConfiguration.keyValueStore);

    HAPError err;
    err = HAPPlatformKeyValueStoreSet(
            accessoryConfiguration.keyValueStore,
            kAppKeyValueStoreDomain_Configuration,
            kAppKeyValueStoreKey_Configuration_State,
            &accessoryConfiguration.state,
            sizeof accessoryConfiguration.state);
    if (err) {
        HAPAssert(err == kHAPError_Unknown);
        HAPFatalError();
    }
}

//----------------------------------------------------------------------------------------------------------------------

/**
 * HomeKit accessory that provides the Light Bulb service.
 *
 * Note: Not constant to enable BCT Manual Name Change.
 */
static HAPAccessory accessory = { .aid = 1,
                                  // .category = kHAPAccessoryCategory_Lighting,
                                  .category = kHAPAccessoryCategory_Sensors,
                                  .name = "Marco's Sensor",
                                  .manufacturer = "Marco",
                                  .model = "Sensor1,1",
                                  .serialNumber = "000000000001",
                                  .firmwareVersion = "1",
                                  .hardwareVersion = "1",
                                  .services = (const HAPService* const[]) { &accessoryInformationService,
                                                                            &hapProtocolInformationService,
                                                                            &pairingService,
                                                                            // &lightBulbService,
                                                                            &sensorHumidityService,
                                                                            &sensorTemperatureService,
                                                                            &sensorAirQualityService,
                                                                            NULL },
                                  .callbacks = { .identify = IdentifyAccessory } };

//----------------------------------------------------------------------------------------------------------------------

HAP_RESULT_USE_CHECK
HAPError IdentifyAccessory(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPAccessoryIdentifyRequest* request HAP_UNUSED,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s", __func__);
    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPBoolCharacteristicReadRequest* request HAP_UNUSED,
        bool* value,
        void* _Nullable context HAP_UNUSED) {
    *value = accessoryConfiguration.state.lightBulbOn;
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");
    printf("*********************** reading value: %d \n", *value);

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleTemperatureRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPFloatCharacteristicReadRequest* request HAP_UNUSED,
        float* value,
        void* _Nullable context HAP_UNUSED) {

    // esp_err_t err = dht_read_float_data(sensor_type, dht_gpio, &accessoryConfiguration.state.humidity, &accessoryConfiguration.state.temperature);
    // if (err == ESP_OK) {
    //     ESP_LOGI(TAG, "HandleTemperatureRead - Humidity: %.1f%% Temp: %.1f\n", accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    // } else {
    //     ESP_LOGW(TAG, "HandleTemperatureRead - Could not read data from sensor\n");
    // }
    readDHT();
    *value = accessoryConfiguration.state.temperature;
    ESP_LOGI(TAG, "HandleTemperatureRead - Humidity: %.1f%% Temp: %.1f\n", accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    PrintOnScreen();

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleHumidityRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPFloatCharacteristicReadRequest* request HAP_UNUSED,
        float* value,
        void* _Nullable context HAP_UNUSED) {
    // esp_err_t err = dht_read_float_data(sensor_type, dht_gpio, &accessoryConfiguration.state.humidity, &accessoryConfiguration.state.temperature);
    // if (err == ESP_OK) {
    //     ESP_LOGI(TAG, "HandleHumidityRead - Humidity: %.1f%% Temp: %.1f\n", accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    // } else {
    //     ESP_LOGW(TAG, "HandleHumidityRead - Could not read data from sensor\n");
    // }
    readDHT();
    *value = accessoryConfiguration.state.humidity;
    ESP_LOGI(TAG, "HandleHumidityRead - Humidity: %.1f%% Temp: %.1f\n", accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, *value ? "true" : "false");

    return kHAPError_None;
}

HAP_RESULT_USE_CHECK
HAPError HandleAirQualityRead(
        HAPAccessoryServerRef* server HAP_UNUSED,
        const HAPUInt8CharacteristicReadRequest* request HAP_UNUSED,
        uint8_t* value,
        void* _Nullable context HAP_UNUSED) {

    accessoryConfiguration.state.airval =  adc1_get_raw(ADC1_CHANNEL_0);

    CalcAirQuality();

    *value = accessoryConfiguration.state.airquality;

    ESP_LOGI(TAG, "HandleAirQualityRead - Airquality reading value raw: %d \n", accessoryConfiguration.state.airval);

    // int air_quality = gpio_get_level(36);

    PrintOnScreen();

    return kHAPError_None;
}



HAP_RESULT_USE_CHECK
HAPError HandleLightBulbOnWrite(
        HAPAccessoryServerRef* server,
        const HAPBoolCharacteristicWriteRequest* request,
        bool value,
        void* _Nullable context HAP_UNUSED) {
    HAPLogInfo(&kHAPLog_Default, "%s: %s", __func__, value ? "true" : "false");
    if (accessoryConfiguration.state.lightBulbOn != value) {
        accessoryConfiguration.state.lightBulbOn = value;

        printf("*********************** write value: %d \n", value);
        gpio_set_level(4, value);



        int air_quality = gpio_get_level(36);
        printf("********************************** air quality: %i \n", air_quality);


        // uint32_t reading =  adc1_get_raw(ADC1_CHANNEL_0);

        // esp_adc_cal_characteristics_t *adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
        // esp_adc_cal_characterize(ADC_UNIT_1, ADC_EXAMPLE_ATTEN, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);


        // uint32_t voltage = esp_adc_cal_raw_to_voltage(reading, adc_chars);
        // printf("*********************** temp reading: %i \n", reading);
        // printf("*********************** temp reading, mV: %i \n", voltage);

        SaveAccessoryState();

        HAPAccessoryServerRaiseEvent(server, request->characteristic, request->service, request->accessory);
    }

    return kHAPError_None;
}

//----------------------------------------------------------------------------------------------------------------------

void AccessoryNotification(
        const HAPAccessory* accessory,
        const HAPService* service,
        const HAPCharacteristic* characteristic,
        void* ctx) {
    HAPLogInfo(&kHAPLog_Default, "Accessory Notification");

    HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, characteristic, service, accessory);
}

void AppCreate(HAPAccessoryServerRef* server, HAPPlatformKeyValueStoreRef keyValueStore) {
    HAPPrecondition(server);
    HAPPrecondition(keyValueStore);

    HAPLogInfo(&kHAPLog_Default, "%s", __func__);

    HAPRawBufferZero(&accessoryConfiguration, sizeof accessoryConfiguration);
    accessoryConfiguration.server = server;
    accessoryConfiguration.keyValueStore = keyValueStore;
    LoadAccessoryState();
}

void AppRelease(void) {
}

void AppAccessoryServerStart(void) {
    HAPAccessoryServerStart(accessoryConfiguration.server, &accessory);
}

//----------------------------------------------------------------------------------------------------------------------

void AccessoryServerHandleUpdatedState(HAPAccessoryServerRef* server, void* _Nullable context) {
    HAPPrecondition(server);
    HAPPrecondition(!context);

    switch (HAPAccessoryServerGetState(server)) {
        case kHAPAccessoryServerState_Idle: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Idle.");
            return;
        }
        case kHAPAccessoryServerState_Running: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Running.");
            return;
        }
        case kHAPAccessoryServerState_Stopping: {
            HAPLogInfo(&kHAPLog_Default, "Accessory Server State did update: Stopping.");
            return;
        }
    }
    HAPFatalError();
}

const HAPAccessory* AppGetAccessoryInfo() {
    return &accessory;
}

TickType_t xLastWakeTime;

void updateHAPCurrentState(void* _Nullable context, size_t contextSize)
{
    // Add delay, fixes https://github.com/espressif/arduino-esp32/issues/3871#issuecomment-913186206
    const TickType_t xDelay = 100 / portTICK_PERIOD_MS;
    vTaskDelay( xDelay );

    int time_diff = (xTaskGetTickCount() - xLastWakeTime) * portTICK_PERIOD_MS;
    if (time_diff > 10000) { // 30 seconds
        ESP_LOGI(TAG, "updateHAPCurrentState - emitting trigger");
        HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, &AirQualityCharacteristic, &sensorAirQualityService, AppGetAccessoryInfo());
        HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, &TemperatureCharacteristic, &sensorTemperatureService, AppGetAccessoryInfo());
        // HAPAccessoryServerRaiseEvent(accessoryConfiguration.server, &HumidityCharacteristic, &sensorHumidityService, AppGetAccessoryInfo());
        xLastWakeTime = xTaskGetTickCount();
        ESP_LOGI(TAG, "updateHAPCurrentState - Humidity: %.1f%% Temp: %.1f\n", accessoryConfiguration.state.humidity, accessoryConfiguration.state.temperature);
    }

    // Re-add the callback to the event loop.
    HAPError err = HAPPlatformRunLoopScheduleCallback(updateHAPCurrentState, NULL, 0);
}




void AppInitialize(
        HAPAccessoryServerOptions* hapAccessoryServerOptions,
        HAPPlatform* hapPlatform,
        HAPAccessoryServerCallbacks* hapAccessoryServerCallbacks) {
    /*no-op*/

    strcpy(accessoryConfiguration.state.airstr, "Null");

    gpio_pad_select_gpio(4);
    gpio_set_direction(4, GPIO_MODE_OUTPUT);

    gpio_pad_select_gpio(36);
    gpio_set_direction(36, GPIO_MODE_INPUT);

    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11);


    u8g2_esp32_hal_t u8g2_esp32_hal = U8G2_ESP32_HAL_DEFAULT;
    u8g2_esp32_hal.sda = GPIO_OLED_SDA;
    u8g2_esp32_hal.scl = GPIO_OLED_SCL;

    u8g2_esp32_hal_init(u8g2_esp32_hal);

    // u8g2_Setup_ssd1327_i2c_seeed_96x96_1(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    // u8g2_Setup_ssd1327_i2c_seeed_96x96_2(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    // u8g2_Setup_ssd1327_i2c_seeed_96x96_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    // u8g2_Setup_ssd1327_i2c_ea_w128128_1(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    // u8g2_Setup_ssd1306_i2c_128x32_univision_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    // u8g2_Setup_sh1107_i2c_seeed_96x96_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
    u8g2_Setup_sh1107_i2c_seeed_128x128_f(&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);



    u8x8_SetI2CAddress(&u8g2.u8x8, 0x3C);
    u8g2_InitDisplay(&u8g2);
    u8g2_SetPowerSave(&u8g2, 0);

    u8g2_ClearBuffer(&u8g2);

    u8g2_SetFont(&u8g2, u8g2_font_timR24_tf);
    u8g2_DrawStr(&u8g2, 0, 40, "-");

    // u8g2_DrawStr(&u8g2, 80, 20, "00:00");
    u8g2_SetFont(&u8g2, u8g2_font_timR14_tf);
    // u8g2_DrawStr(&u8g2, 0, 40, "Temp: -");
    u8g2_DrawStr(&u8g2, 0, 70, "Humidity: -");
    u8g2_DrawStr(&u8g2, 0, 100, "Air: -");

    u8g2_SendBuffer(&u8g2);
}

void AppDeinitialize() {
    /*no-op*/
}
