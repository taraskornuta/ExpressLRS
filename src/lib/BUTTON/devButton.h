#pragma once

#include "device.h"
#include <functional>

#if defined(GPIO_PIN_BUTTON)
    #if defined(TARGET_TX) || \
        (defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)))
        extern device_t Button_device;
        #define HAS_BUTTON
    #endif
    void registerButtonFunction(const char *name, std::function<void()> function);
    void addButtonAction(uint8_t button, bool longPress, uint8_t count, const char *name);
#else
    inline void registerButtonFunction(const char *name, std::function<void()> function) {}
    inline void addButtonAction(uint8_t button, bool longPress, uint8_t count, const char *name) {}
#endif
