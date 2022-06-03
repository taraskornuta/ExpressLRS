#include "targets.h"
#include "common.h"
#include "device.h"

#if defined(GPIO_PIN_BUTTON)
#include "logging.h"
#include "button.h"
#include "config.h"

#include <map>
#include <list>

#ifndef GPIO_BUTTON_INVERTED
#define GPIO_BUTTON_INVERTED false
#endif
#ifndef GPIO_BUTTON2_INVERTED
#define GPIO_BUTTON2_INVERTED false
#endif
#if !defined(GPIO_PIN_BUTTON2)
#define GPIO_PIN_BUTTON2 UNDEF_PIN
#endif

static Button button1;
#if defined(GPIO_PIN_BUTTON2)
static Button button2;
#endif

static std::map<const char *, std::function<void()>> actions;

typedef struct action {
    uint8_t button;
    bool longPress;
    uint8_t count;
    const char *name;
} action_t;

static std::list<action_t> buttonActions;

void registerButtonFunction(const char *name, std::function<void()> function)
{
    actions[name] = function;
}


void addButtonAction(uint8_t button, bool longPress, uint8_t count, const char *name)
{
    action_t action = {button, longPress, count, name};
    buttonActions.push_back(action);
}

static void handlePress(uint8_t button, bool longPress, uint8_t count)
{
    std::list<action_t>::iterator it;
    DBGLN("handle press");
    for (it = buttonActions.begin(); it != buttonActions.end(); ++it)
    {
        if (it->button == button && it->longPress == longPress && (count == 0 || it->count == count))
        {
            if (actions[it->name])
            {
                actions[it->name]();
            }
        }
    }
}

static int start()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
    if (GPIO_PIN_BUTTON != UNDEF_PIN)
    {
        button1.init(GPIO_PIN_BUTTON, GPIO_BUTTON_INVERTED);
        button1.OnShortPress = [](){ handlePress(1, false, button1.getCount()); };
        button1.OnLongPress = [](){ handlePress(1, true, button1.getCount()); };
    }
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.init(GPIO_PIN_BUTTON2, GPIO_BUTTON_INVERTED);
        button2.OnShortPress = [](){ handlePress(2, false, button2.getCount()); };
        button2.OnLongPress = [](){ handlePress(2, true, button2.getCount()); };
    }
    return DURATION_IMMEDIATELY;
}

static int timeout()
{
    if (GPIO_PIN_BUTTON == UNDEF_PIN)
    {
        return DURATION_NEVER;
    }
#if defined(GPIO_PIN_BUTTON2)
    if (GPIO_PIN_BUTTON2 != UNDEF_PIN)
    {
        button2.update();
    }
#endif
    return button1.update();
}

device_t Button_device = {
    .initialize = nullptr,
    .start = start,
    .event = nullptr,
    .timeout = timeout
};

#endif