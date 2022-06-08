#pragma once

#include "device.h"

#include <functional>
#include <list>
#include <map>

typedef struct action {
    uint8_t button;
    bool longPress;
    uint8_t count;
    const char *name;
} action_t;

#if defined(GPIO_PIN_BUTTON)
    #if defined(TARGET_TX) || \
        (defined(TARGET_RX) && (defined(PLATFORM_ESP32) || defined(PLATFORM_ESP8266)))
        extern device_t Button_device;
        #define HAS_BUTTON
    #endif
    void registerButtonFunction(const char *name, std::function<void()> function);
    void addButtonAction(uint8_t button, bool longPress, uint8_t count, const char *name);
    const std::list<action_t> &getButtonActions();
    const std::map<const char *, std::function<void()>> &getButtonFunctions();
#endif
