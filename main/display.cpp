#include "color/gdeh042Z96.h"
#include <Fonts/ubuntu/Ubuntu_M16pt8b.h>
#include <Fonts/ubuntu/Ubuntu_M8pt8b.h>
#include <time.h>
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "Display controller";

static void addDate(Epd *display, const tm *current_time)
{
    char strftime_buf[64];
    // ESP_LOGI(TAG, "Cursor x: %d, cursor y: %d", display->getCursorX(), display->getCursorY());
    
    // display->setCursor(120, 16);
    // ESP_LOGI(TAG, "Cursor after move x: %d, cursor after move y: %d", display->getCursorX(), display->getCursorY());
    display->setTextColor(EPD_BLACK);
    display->setFont(&Ubuntu_M16pt8b);

    strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%Y", current_time);
    display->draw_centered_text(&Ubuntu_M16pt8b, 195, 8, 16, 16, strftime_buf);
}


static double get_threshold(const double prices_KWh[24]){
    double sum = 0, mean = 0;

    for (int i = 0; i < 24; i++)
    {
        sum += prices_KWh[i];
    }

    return (sum / 24) * 1.1;
}

static void rectangles(Epd *display, const double prices_KWh[24], const tm *current_time)
// static void rectangles(Epd *display, const tm *current_time)
{
    double threshold = get_threshold(prices_KWh);

    display->setFont(&Ubuntu_M8pt8b);
    for (size_t rectangle = 0; rectangle < 24; rectangle++)
    {
        int x_pos = 5 + 195 * (rectangle >= 12);
        int y_pos = ((rectangle % 12) * 20) + 50;
        int color = prices_KWh[rectangle] > threshold ? EPD_RED : EPD_BLACK;

        if (rectangle < current_time->tm_hour)
        {
            display->drawRect(x_pos, y_pos, 190, 15, color);
            display->setTextColor(EPD_BLACK);
        }
        else
        {
            display->fillRect(x_pos, y_pos, 190, 15, color);
            display->setTextColor(EPD_WHITE);
        }
        display->setCursor(x_pos + 50, y_pos + 12);
        display->printerf("%02d:00 %.3lf eur", rectangle, prices_KWh[rectangle]);
    }
}



esp_err_t display_update(const double prices_KWh[24], const tm *time_info, Epd *display){
    display->fillScreen(EPD_WHITE);
    addDate(display, time_info);
    rectangles(display, prices_KWh, time_info);
    display->update();
    return ESP_OK;
}


esp_err_t display_init(Epd *display, const double prices_KWh[24])
{
    static bool initialized = false;

    if (initialized)
        return ESP_ERR_INVALID_STATE;

    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    display->init(false);
    display->fillScreen(EPD_WHITE);
    display->setRotation(2);

    addDate(display, &timeinfo);
    rectangles(display, prices_KWh, &timeinfo);

    display->update();
    initialized = true;

    return ESP_OK;
}
