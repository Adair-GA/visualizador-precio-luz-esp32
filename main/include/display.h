#include "esp_err.h"
#include "epd.h"

#pragma once

esp_err_t display_init(Epd *display, const double prices_KWh[24]);
// esp_err_t display_init(const double prices_MWh[24]);
esp_err_t display_update(const double prices_KWh[24], const tm *time_info, Epd *display);

