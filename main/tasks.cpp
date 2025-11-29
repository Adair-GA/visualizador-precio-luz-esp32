#include <time.h>
#include "freertos/FreeRTOS.h"

#include "display.h"
#include "epd.h"
#include "tasks.h"
#include <sys/param.h>
#include "cJSON.h"

extern const char server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const char server_root_cert_pem_end[] asm("_binary_server_root_cert_pem_end");
static const char *TAG = "tasks";

static esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer; // Buffer to store response of http request from event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id)
    {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data)
        {
            // we are just starting to copy the output data into the use
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
         *  However, event handler can also be used in case chunked encoding is used.
         */
        if (esp_http_client_is_chunked_response(evt->client))
        {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data)
            {
                // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len)
                {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            }
            else
            {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (esp_http_client_is_chunked_response(evt->client))
                {
                    ESP_LOGE(TAG, "Chunked response");

                    esp_http_client_get_chunk_length(evt->client, &content_len);
                }

                if (output_buffer == NULL)
                {
                    // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                    output_buffer = (char *)calloc(MAX_HTTP_OUTPUT_BUFFER, sizeof(char));
                    output_len = 0;
                    if (output_buffer == NULL)
                    {
                        ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (content_len - output_len));
                if (copy_len)
                {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL)
        {
            memcpy(evt->user_data, output_buffer, output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
    {
        ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0)
        {
            ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL)
        {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
    }
    break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

static tm get_current_time()
{
    time_t now;
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);
    return timeinfo;
}

esp_err_t get_day_prices(tm *request_day, double result_buffer[24])
{
    esp_err_t result = ESP_OK;

    char *response_buffer = (char *)malloc(MAX_HTTP_OUTPUT_BUFFER);
    if (response_buffer == NULL)
    {
        return ESP_ERR_NO_MEM;
    }

    char url_buffer[URL_BUFFER_LEN];

    strftime(
        url_buffer,
        URL_BUFFER_LEN,
        "https://apidatos.ree.es/es/datos/mercados/precios-mercados-tiempo-real?"
        "start+date=%Y-%m-%dT00:00&"
        "end+date=%Y-%m-%dT23:59&"
        "time+trunc=hour",
        request_day);
    ESP_LOGI(TAG, "Requesting url: %s", url_buffer);

    esp_http_client_config_t config = {
        .url = url_buffer,
        .cert_pem = server_root_cert_pem_start,
        .timeout_ms = 10000,
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "Status = %d", esp_http_client_get_status_code(client));
    }

    cJSON *response = cJSON_Parse(response_buffer);

    cJSON *included = cJSON_GetObjectItem(response, "included");
    cJSON *PVPC = cJSON_GetArrayItem(included, 0);
    cJSON *attributes = cJSON_GetObjectItem(PVPC, "attributes");
    cJSON *values = cJSON_GetObjectItem(attributes, "values");

    int valueCount = cJSON_GetArraySize(values);

    if (valueCount != 24)
    {
        ESP_LOGE(TAG, "Expected 24 values, got %d", valueCount);
    }

    double sum = 0;
    for (size_t i = 0; i < 24; i++)
    {
        cJSON *valueObject = cJSON_GetArrayItem(values, i);
        cJSON *value = cJSON_GetObjectItem(valueObject, "value");
        if (!cJSON_IsNumber(value))
        {
            ESP_LOGE(TAG, "NOT A NUMBER");
            result = ESP_ERR_INVALID_ARG;
            goto end;
        }

        result_buffer[i] = value->valuedouble / 1000;
        sum += result_buffer[i];
    }

end:
    if (response != NULL)
    {
        cJSON_Delete(response);
    }
    if (response_buffer != NULL)
    {
        free(response_buffer);
    }
    return result;
}

void hourly_remove_rectangle(void *arg)
{
    ESP_LOGI(TAG, "hourly_remove_rectangle called");
    app_state *state = (app_state *)arg;
    tm now = get_current_time();

    if (state->hourly_timer == NULL)
    {
        ESP_LOGI(TAG, "hourly_remove_rectangle, setting up hourly callback");
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &hourly_remove_rectangle,
            .arg = state,
            .name = "periodic hourly_remove_rectangle",
        };
        esp_timer_create(&periodic_timer_args, &state->hourly_timer);
        // esp_timer_start_periodic(*(state->hourly_timer), pdMS_TO_TICKS(60 * 60 * 1000));
        esp_timer_start_periodic(state->hourly_timer, (60LL * 60 * 1000 * 1000));
    }
    else
    {
        ESP_LOGI(TAG, "hourly_remove_rectangle called from periodic callback");
    }

    if (now.tm_hour == 0)
    {
        return;
    }

    display_update(state->today_prices_KWh, &now, state->display);
}

void nightly_update_screen(void *arg)
{
    ESP_LOGI(TAG, "nightly_update_screen called");
    app_state *state = (app_state *)arg;
    tm now = get_current_time();

    if (state->nightly_timer == NULL)
    {
        ESP_LOGI(TAG, "nightly_update_screen, setting up hourly callback");
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &nightly_update_screen,
            .arg = state,
            .name = "periodic nightly_update_screen",
        };
        esp_timer_create(&periodic_timer_args, &state->nightly_timer);
        esp_timer_start_periodic(state->nightly_timer, ((long long)24L * 60L * 60L * 1000L * 1000L));
    }
    else
    {
        ESP_LOGI(TAG, "nightly_update_screen called from periodic callback");
    }

    if (state->tomorrow_prices_available)
    {
        for (size_t i = 0; i < 24; i++)
        {
            state->today_prices_KWh[i] = state->tomorrow_prices_KWh[i];
            state->tomorrow_prices_available = false;
        }
    }
    else
    {
        get_day_prices(&now, state->today_prices_KWh);
    }

    display_update(state->today_prices_KWh, &now, state->display);
}

void fetch_tomorrow_prices(void *arg)
{
    ESP_LOGI(TAG, "fetch_tomorrow_prices called");
    app_state *state = (app_state *)arg;
    tm now = get_current_time();

    if (state->fetch_next_day_timer == NULL && now.tm_hour == 20 && now.tm_min == 30)
    {
        ESP_LOGI(TAG, "fetch_tomorrow_prices, setting up hourly callback");
        const esp_timer_create_args_t periodic_timer_args = {
            .callback = &fetch_tomorrow_prices,
            .arg = state,
            .name = "periodic fetch_tomorrow_prices",
        };
        esp_timer_create(&periodic_timer_args, &state->fetch_next_day_timer);
        esp_timer_start_periodic(state->fetch_next_day_timer, ((long long)24L * 60L * 60L * 1000L * 1000L));
    }
    else
    {
        ESP_LOGI(TAG, "fetch_tomorrow_prices called from periodic callback");
    }

    tm tomorrow_pre = get_current_time();
    tomorrow_pre.tm_min = 0;
    tomorrow_pre.tm_sec = 0;
    tomorrow_pre.tm_hour = 0;
    tomorrow_pre.tm_mday += 1;

    tm tomorrow = {};

    time_t t_next = mktime(&tomorrow_pre);
    localtime_r(&t_next, &tomorrow);

    if(get_day_prices(&tomorrow, state->tomorrow_prices_KWh)){
        state->tomorrow_prices_available = true;
    }
}