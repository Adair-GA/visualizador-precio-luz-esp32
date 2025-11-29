#pragma once

#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_tls.h"

#include "epd.h"

#include "state.h"

#define MAX_HTTP_OUTPUT_BUFFER 16384 // 16kB
#define URL_BUFFER_LEN 150

void hourly_remove_rectangle(void *state);
void nightly_update_screen(void *arg);
esp_err_t get_day_prices(tm *request_day, double result_buffer[24]);
void fetch_tomorrow_prices(void *arg);

