/*
LED1 signals when HP_CORE is running
LED2 signals when LP_CORE is running
GPIO7 - button input used for IOC
*/

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "hal/gpio_types.h"
#include "ulp_lp_core.h"
#include "ulp_main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lp_core_uart.h"
#include "defines.h"

#include "driver/i2c_master.h"

extern const uint8_t ulp_main_bin_start[] asm("_binary_ulp_main_bin_start");
extern const uint8_t ulp_main_bin_end[]   asm("_binary_ulp_main_bin_end");

static void init_ulp_program(void);

i2c_master_bus_handle_t bus_handle;
i2c_master_dev_handle_t dev_handle;

int probeAddress(uint8_t addr)
{
    esp_err_t err = 0;
    if ((err = i2c_master_probe(bus_handle, addr, 500)))
    {
        if (err == ESP_ERR_TIMEOUT) return 0;
        return -1;
    }
    return 1;
}

/** Simple helper function to scan a bus for all devices and dump to logs */
void scan()
{
    uint8_t address;
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            address = (uint8_t)(i + j);
            int ret = probeAddress(address);
            if (ret == 1)         printf("%02x ", address);
            else if (ret == 0)    printf("UU ");
            else                  printf("-- ");
        }
        printf("\r\n");
    }
}


static void i2c_init()
{
	i2c_master_bus_config_t bus_config = {
	        .i2c_port = I2C_NUM_0,
	        .sda_io_num = 6,
	        .scl_io_num = 7,
	        .clk_source = I2C_CLK_SRC_DEFAULT,
	        .glitch_ignore_cnt = 7,
	        .flags.enable_internal_pullup = true,
	    };
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));
	
	i2c_device_config_t dev_config = {
		    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
	        .device_address = 0x70,
	        .scl_speed_hz = 50000,
	        .scl_wait_us = 100000,
	    };
	ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle));
}

static void writeCommand(uint16_t c)
{
	uint8_t cmd[2] = { c >> 8, c & 0xFF };

	esp_err_t err = i2c_master_transmit(dev_handle, cmd, sizeof(cmd), 3000);
	if (err != ESP_OK)
	{
	    printf("Failed to write data to device %d (%s)\n", err, esp_err_to_name(err));
		return;
	}
}

void getTemperatureAndHumidity()
{
     writeCommand(0x7866); // Todo: enable low power with 0x609C instead
 	 vTaskDelay(pdMS_TO_TICKS(20));
     uint8_t raw[6] = { };
 	 esp_err_t err = 0;
	 if ((err = i2c_master_receive(dev_handle, raw, sizeof(raw), 3000)) != ESP_OK)
	 {
	 	printf("Failed to read data from device: %d (%s)\n", err, esp_err_to_name(err));
	 	return;
	 }
     // Extract the temperature and humidity here
     uint16_t temp = raw[0] << 8 | raw[1];
     // CRC Check disabled here
     // uint8 crc = computeCRC(raw[1], computeCRC(raw[0]));
     // if (crc != raw[2])
     // {
     //     elogm(Log::SystemError, "CRC failed, got %02X expected %02X", crc, raw[2]);
     //     return false;
     // }
     double temperature = -45.0 + 175.0 * temp / 65536.0;
     uint16_t hum = raw[3] << 8 | raw[4];
     double humidity = 100.0 * hum / 65536.0;
     printf("Temp: %g / Hum: %g\n", temperature, humidity);
     return;
}

static void SHTC3()
{
	i2c_init();
	scan();
    writeCommand(0x3517);
	vTaskDelay(pdMS_TO_TICKS(1000));
	getTemperatureAndHumidity();
	writeCommand(0xB098);
}

void dumpULPLog()
{
#define LP_CORE_LL_WAKEUP_SOURCE_HP_CPU    BIT(0) // Started by HP core (1 single wakeup)
#define LP_CORE_LL_WAKEUP_SOURCE_LP_UART   BIT(1) // Enable wake-up by a certain number of LP UART RX pulses
#define LP_CORE_LL_WAKEUP_SOURCE_LP_IO     BIT(2) // Enable wake-up by LP IO interrupt
#define LP_CORE_LL_WAKEUP_SOURCE_ETM       BIT(3) // Enable wake-up by ETM event
#define LP_CORE_LL_WAKEUP_SOURCE_LP_TIMER  BIT(4) // Enable wake-up by LP timer

    printf("\n\n====== Log Array ========\n");
	for (uint32_t i = 0; i < ulp_logIndex; i+=3)
	{
	    
		printf("LPcore wake up cause: (%lu) = %lu %s%s%s%s%s\n", i, ulp_logArray[i],
			(ulp_logArray[i] & LP_CORE_LL_WAKEUP_SOURCE_HP_CPU) ? "HP_CPU:" : "",
			(ulp_logArray[i] & LP_CORE_LL_WAKEUP_SOURCE_LP_UART) ? "LP_UART:" : "",
			(ulp_logArray[i] & LP_CORE_LL_WAKEUP_SOURCE_LP_IO) ? "LP_IO:" : "",
			(ulp_logArray[i] & LP_CORE_LL_WAKEUP_SOURCE_ETM) ? "ETM:" : "",
			(ulp_logArray[i] & LP_CORE_LL_WAKEUP_SOURCE_LP_TIMER) ? "LP_TIMER" : ""
			);
		printf("LPcore cycle count: 0x%08lX%08lX\n", ulp_logArray[i+1], ulp_logArray[i+2]);
	}
}


void app_main(void)
{
	/* Allow USB to connect before outputting any data to it */
	vTaskDelay(pdMS_TO_TICKS(1000));

    /* Initialize selected GPIO as RTC IO, enable input, disable pullup and pulldown */
    rtc_gpio_init(INPUT_PIN);
    rtc_gpio_set_direction(INPUT_PIN, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(INPUT_PIN);
    rtc_gpio_pullup_en(INPUT_PIN);
	rtc_gpio_wakeup_enable(INPUT_PIN, GPIO_INTR_NEGEDGE);

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    if (cause == ESP_SLEEP_WAKEUP_ULP) {
            printf("ULP woke up the main CPU! \r\n");
            while(true)
            {
            	dumpULPLog();
           		vTaskDelay(pdMS_TO_TICKS(1000));

            }
            // Start logging the 
        }
    else {
        printf("WakeUp %d, initializing ULP! \r\n", cause);
        init_ulp_program();
    }

    /* Go back to sleep, only the ULP will run */
    SHTC3();
    printf("Entering deep sleep\r\n");
    vTaskDelay(200);

    ESP_ERROR_CHECK( esp_sleep_enable_ulp_wakeup());
    esp_deep_sleep_start();
}


static void init_ulp_program(void)
{
    esp_err_t err = ulp_lp_core_load_binary(ulp_main_bin_start, (ulp_main_bin_end - ulp_main_bin_start));
    ESP_ERROR_CHECK(err);

	// Initialize the log array
	memset(&ulp_logArray, 0, 256*sizeof(*ulp_logArray));
	ulp_logIndex = 0;
	
    /* Start the program */
    ulp_lp_core_cfg_t cfg = {
        .wakeup_source = ULP_LP_CORE_WAKEUP_SOURCE_LP_TIMER | ULP_LP_CORE_WAKEUP_SOURCE_LP_IO,
        .lp_timer_sleep_duration_us = 3000000,
    };

    err = ulp_lp_core_run(&cfg);
    ESP_ERROR_CHECK(err);
}
