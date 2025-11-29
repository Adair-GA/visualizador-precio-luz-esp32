#pragma once


#include "epd.h"
#include "esp_timer.h"

struct app_state
{
    Epd *display;
    double today_prices_KWh[24];
    double tomorrow_prices_KWh[24];
    bool tomorrow_prices_available = false;
    esp_timer_handle_t hourly_timer;
    esp_timer_handle_t nightly_timer;
    esp_timer_handle_t fetch_next_day_timer;
};
