#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "protocol_examples_common.h"
#include "esp_netif.h"

#include "sdkconfig.h"
#include "time_sync.h"
#include "display.h"
#include "tasks.h"
#include "state.h"

#include "esp_http_client.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_system.h"
#include "color/gdeh042Z96.h"

static const char *TAG = "preciosluz";


EpdSpi io;
Gdeh042Z96 display(io);

static app_state state;

extern "C" void app_main(void)
{
    bool tomorrow_available = false;
    time_t now;
    struct tm timeinfo;

    state.display = &display;

    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    ESP_ERROR_CHECK(sync_time());
    
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
    tzset();

    
    time(&now);
    localtime_r(&now, &timeinfo);
    
    get_day_prices(&timeinfo, state.today_prices_KWh);

    tm next_hour_pre = timeinfo;
    next_hour_pre.tm_min = 0;
    next_hour_pre.tm_sec = 0;
    next_hour_pre.tm_hour += 1;

    tm next_hour = {};

    time_t t_next_hour = mktime(&next_hour_pre);
    localtime_r(&t_next_hour, &next_hour);

    esp_timer_create_args_t next_hour_timer_args = {
        .callback = &hourly_remove_rectangle,
        .arg = &state,
    };
    esp_timer_handle_t next_hour_timer;
    esp_timer_create(&next_hour_timer_args, &next_hour_timer);
    
    
    tm next_midnight_pre = timeinfo;
    next_midnight_pre.tm_min = 0;
    next_midnight_pre.tm_sec = 0;
    next_midnight_pre.tm_hour = 0;
    next_midnight_pre.tm_mday += 1;
    time_t t_next_midnight = mktime(&next_midnight_pre);
    
    
    esp_timer_create_args_t next_midnight_timer_args = {
        .callback = &nightly_update_screen,
        .arg = &state,
    };
    esp_timer_handle_t next_midnight_timer;
    esp_timer_create(&next_midnight_timer_args, &next_midnight_timer);

    tm next_fetch_next_day_pre = timeinfo;
    next_fetch_next_day_pre.tm_sec = 0;
    next_fetch_next_day_pre.tm_min = 30;
    next_fetch_next_day_pre.tm_hour = 20;
    if (timeinfo.tm_hour > 20 || (timeinfo.tm_hour == 20 && timeinfo.tm_min > 30)){
        next_fetch_next_day_pre.tm_mday += 1;
        tomorrow_available = true;
    }
    time_t t_next_fetch_next_day = mktime(&next_fetch_next_day_pre);
    
    
    esp_timer_create_args_t next_fetch_next_day_timer_args = {
        .callback = &fetch_tomorrow_prices,
        .arg = &state,
    };
    esp_timer_handle_t next_fetch_next_day_timer;
    esp_timer_create(&next_fetch_next_day_timer_args, &next_fetch_next_day_timer);


    time_t diff_to_next_hour = difftime(t_next_hour, now);
    time_t diff_to_next_midnight = difftime(t_next_midnight, now);
    time_t diff_to_next_fetch_next_day = difftime(t_next_fetch_next_day, now);


    ESP_LOGI(TAG, "%llu seconds until next hour", diff_to_next_hour);
    ESP_LOGI(TAG, "%llu seconds until next midnight", diff_to_next_midnight);
    ESP_LOGI(TAG, "%llu seconds until next fetch next day", diff_to_next_fetch_next_day);


    esp_timer_start_once(next_hour_timer, diff_to_next_hour * 1000000);
    esp_timer_start_once(next_midnight_timer, diff_to_next_midnight * 1000000);
    // esp_timer_start_once(next_fetch_next_day_timer, diff_to_next_fetch_next_day * 1000000);
    
    // if (tomorrow_available){
    //     fetch_tomorrow_prices((void *)&state);
    // }
    
    display_init(state.display, state.today_prices_KWh);
}
