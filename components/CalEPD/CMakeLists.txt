# Add only the display srcs you are going to use if you want to keep compiled srcs to a minimum:
set(srcs 
    "models/color/gdeh042Z96.cpp"

    # Common base classes
    "epd.cpp"
    "epd7color.cpp"
    "epdspi.cpp"
    "epd4spi.cpp"
    )

idf_build_get_property(target IDF_TARGET)
message(STATUS "→ Building example for ${target}")

if (${target} EQUAL "esp32")
    list(APPENDS srcs "epd4spi.cpp")
endif()

idf_component_register(SRCS ${srcs}      
                    REQUIRES "Adafruit-GFX"
                    REQUIRES esp_timer 
                             driver

                    INCLUDE_DIRS "include"
)