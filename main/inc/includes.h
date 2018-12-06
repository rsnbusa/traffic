#ifndef includes_h
#define includes_h

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "I2C.h"
#include "SSD1306.h"
#include "time.h"
#include "Sodaq_DS3231.h"

extern "C"{
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "math.h"
#include "driver/spi_master.h"
#include "mongoose.h"
#include "sdkconfig.h"
#include "ds18b20.h"
#include "driver/uart.h"
#include "driver/i2c.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "mdns.h"
#include "apps/sntp/sntp.h"
#include "soc/timer_group_struct.h"
#include "esp_types.h"
#include "driver/periph_ctrl.h"
#include "driver/timer.h"
#include "cJSON.h"
#include "sys/time.h"
#include "tcpip_adapter.h"
#include "apps/sntp/sntp.h"
#include "posix/sys/socket.h"
#include <sys/types.h>
#include "String.h"
#include "rom/rtc.h"
#include "rom/uart.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "esp_request.h"
#include "esp_vfs_fat.h"
#include "esp_system.h"
#include "mqtt_client.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include "esp_log.h"
#include "errno.h"
#include "mbedtls/md.h"
#include <esp_http_server.h>
void app_main();
}
#endif
