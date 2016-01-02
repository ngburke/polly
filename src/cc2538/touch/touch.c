#include <touch.h>
#include <stdio.h>
#include <string.h>

#include <gpio.h>
#include <ioc.h>
#include <systick.h>
#include <hw_nvic.h>
#include <assert.h>
#include <pins.h>
#include <sys_ctrl.h>

typedef enum
{
    TICKS_RESET,
    TICKS_START,
    TICKS_STOPPED,
    TICKS_ALL_STOPPED,
    TICKS_STOP,
    TICKS_GET,
} TickCtrl_e;

#define BUTTONS       4
#define SINGLE_BURST  1
#define MULTI_BURST   !SINGLE_BURST
#define TOUCH_THRESH  64
#define SLIDER        1

const int TICK_PERIOD = 0x1000000;

const uint32_t KEY[BUTTONS] = {TOUCH_KEY_1, TOUCH_KEY_2, TOUCH_KEY_3, TOUCH_KEY_4};

static touch_type_e get(unsigned *down, unsigned *up, bool slide_only);


#if TOUCH_CAPACITIVE

static uint32_t     base_ticks[BUTTONS];
static uint32_t     curr_ticks[BUTTONS];

static uint32_t     ticks[BUTTONS];
static bool         ticks_stopped[BUTTONS];

static void         ticks_reset (void);
static unsigned     ticks_get   (void);
static uint32_t     ticks_ctrl  (TickCtrl_e ctrl, int button);

#else

static uint32_t pressed_button;

#endif


void touch_isr(void)
{
    int status;

    status = GPIOPinIntStatus(TOUCH_BASE, true);

    if (status & TOUCH_KEY_1)
    {
#if TOUCH_CAPACITIVE
        ticks_ctrl(TICKS_STOP, 0);
#else
        pressed_button = 1;
#endif
        GPIOPinIntClear(TOUCH_BASE, TOUCH_KEY_1);
    }
    if (status & TOUCH_KEY_2)
    {
#if TOUCH_CAPACITIVE
        ticks_ctrl(TICKS_STOP, 1);
#else
        pressed_button = 2;
#endif
        GPIOPinIntClear(TOUCH_BASE, TOUCH_KEY_2);
    }
    if (status & TOUCH_KEY_3)
    {
#if TOUCH_CAPACITIVE
        ticks_ctrl(TICKS_STOP, 2);
#else
        pressed_button = 3;
#endif
        GPIOPinIntClear(TOUCH_BASE, TOUCH_KEY_3);
    }
    if (status & TOUCH_KEY_4)
    {
#if TOUCH_CAPACITIVE
        ticks_ctrl(TICKS_STOP, 3);
#else
        pressed_button = 4;
#endif
        GPIOPinIntClear(TOUCH_BASE, TOUCH_KEY_4);
    }
}

void touch_init()
{
    int i;

    for (i = 0; i < BUTTONS; i++)
    {
        GPIOPinTypeGPIOInput (TOUCH_BASE, KEY[i]);
        GPIOIntTypeSet       (TOUCH_BASE, KEY[i], GPIO_FALLING_EDGE);
        GPIOPinIntClear      (TOUCH_BASE, KEY[i]);
        GPIOPinIntEnable     (TOUCH_BASE, KEY[i]);
#if TOUCH_CAPACITIVE
        IOCPadConfigSet      (TOUCH_BASE, KEY[i], IOC_OVERRIDE_PDE);
#else
        IOCPadConfigSet      (TOUCH_BASE, KEY[i], IOC_OVERRIDE_PUE);
#endif
    }

    GPIOPortIntRegister(TOUCH_BASE, &touch_isr);

    pressed_button = 0;
}

touch_type_e touch_get(unsigned *down, unsigned *up)
{
    return get(down, up, false);
}


touch_type_e touch_get_any(void)
{
    unsigned down, up;

    return get(&down, &up, false);
}


touch_type_e touch_get_slide(void)
{
    unsigned down, up;

    return get(&down, &up, true);
}


#if TOUCH_CAPACITIVE

static touch_type_e get(unsigned *down, unsigned *up, bool slide_only)
{
    // How many buttons of separtion indicate a slide
    const unsigned SLIDE_DELTA = 2;

    uint32_t curr;

    while (1)
    {
        curr  = 0;
        *down = 0;
        *up   = 0;

        // Checking for an initial touch
        while (0 == *down)
        {
            *down = touch_cap_read(false);
        }

        curr = *down;

        // Now checking for a release
        while (curr > 0)
        {
            *up = curr;
            curr = touch_cap_read(false);
        }

        if ((*up > *down) && (*up - *down >= SLIDE_DELTA))
        {
            return TOUCH_SLIDE_RIGHT;
        }
        else if ((*down > *up) && (*down - *up >= SLIDE_DELTA))
        {
            return TOUCH_SLIDE_LEFT;
        }
        else if (!slide_only)
        {
            // Unify up and down even if they were slightly off (based on SLIDE_DELTA)
            *up = *down;
            return TOUCH_POINT;
        }
    }
}

#else // !TOUCH_CAPACITIVE

#define DEBOUNCE_COUNT  50000
#define HOLD_COUNT      250000

static touch_type_e get(unsigned *down, unsigned *up, bool slide_only)
{
    volatile uint32_t counter, bounce;

    pressed_button = 0;

    while (pressed_button == 0)
    {
        // Wait for an interrupt 
        //SysCtrlSleep();
    }

    // Capture the pressed button
    *down = pressed_button;
    *up   = pressed_button;

    // Debounce loop
    for (bounce = 0; bounce < DEBOUNCE_COUNT; bounce++);

    // Wait until the button is released
    for (counter = 0; 0 == GPIOPinRead(TOUCH_BASE, KEY[*down - 1]) && counter < HOLD_COUNT; counter++);

    // Debounce loop
    for (bounce = 0; bounce < DEBOUNCE_COUNT; bounce++);

    if (slide_only)
    {
        if (*up >= 3) return TOUCH_SLIDE_RIGHT;
        else          return TOUCH_SLIDE_LEFT;
    }

    if (counter >= HOLD_COUNT)
    {
        // Long holds equate to a slide
        if (*up >= 3) return TOUCH_SLIDE_RIGHT;
        else          return TOUCH_SLIDE_LEFT;
    }

    // Regular holds are just normal presses
    return TOUCH_POINT;
}

#endif


#if TOUCH_CAPACITIVE

void touch_cap_calibrate()
{
    int i;

    // Calibrate the default ticks
    for (i = 0; i < 10; i++)
    {
        touch_cap_read(true);
    }
}

uint32_t* touch_cap_ticks(unsigned *count)
{
    *count = BUTTONS;
    return curr_ticks;
}

uint32_t touch_cap_read(bool init)
{
    const int SAMPLE_ROUNDS = 8;
    const int SETUP_LOOPS   = 2000;

    unsigned ret     = 0;
    unsigned high    = 0;
    unsigned highidx = 0xFF;

    int i, j;
    volatile int chrg;

    // Reset the averages
    for (i = 0; i < BUTTONS; i++)
    {
        curr_ticks[i] = 0;
    }

    // Reset the tick timer
    ticks_ctrl(TICKS_RESET, 0);

    for (i = 0; i < SAMPLE_ROUNDS; i++)
    {
#if SINGLE_BURST
        for (j = 0; j < BUTTONS; j++)
        {
            // Set pull-up and charge the buttons
            IOCPadConfigSet(TOUCH_BASE, KEY[j], IOC_OVERRIDE_PUE);

            // Let the buttons capacitance charge up
            for (chrg = 0; chrg < SETUP_LOOPS; chrg++);
            while (0 == GPIOPinRead(TOUCH_BASE, KEY[j]));

            ticks_ctrl(TICKS_START, j);

            // Release pull-up and wait until interrupt fires - interrupt will stop ticks
            IOCPadConfigSet(TOUCH_BASE, KEY[j], IOC_OVERRIDE_DIS);

            while (!ticks_ctrl(TICKS_STOPPED, j));

            IOCPadConfigSet(TOUCH_BASE, KEY[j], IOC_OVERRIDE_PDE);
        }
#endif
#if MULTI_BURST

        // Set pull-up and charge the buttons
        for (j = 0; j < BUTTONS; j++)
        {
            IOCPadConfigSet(TOUCH_BASE, KEY[j], IOC_OVERRIDE_PUE);
        }

        // Let the buttons capacitance charge up
        for (chrg = 0; chrg < SETUP_LOOPS; chrg++);

        for (j = 0; j < BUTTONS; j++)
        {
            ticks_ctrl(TICKS_START, j);

            // Release pull-up and wait until interrupt fires - interrupt will stop ticks
            IOCPadConfigSet(TOUCH_BASE, KEY[j], IOC_OVERRIDE_DIS);
        }

        while (!ticks_ctrl(TICKS_ALL_STOPPED, 0));
#endif

        // Low detected, add up the ticks
        for (j = 0; j < BUTTONS; j++)
        {
            curr_ticks[j] += ticks_ctrl(TICKS_GET, j);
        }
    }

    // Calculate the averages
    for (i = 0; i < BUTTONS; i++)
    {
        curr_ticks[i] /= SAMPLE_ROUNDS;

        // Just get the averages and assign to the base if initializing
        if (init)
        {
            base_ticks[i] = curr_ticks[i];
            continue;
        }

        // Calculate the delta from the base
        if (curr_ticks[i] > base_ticks[i])
        {
            curr_ticks[i] -= base_ticks[i];
        }
        else
        {
            curr_ticks[i]  = 0;
        }

        // Has the button crossed the threshold?
        if (curr_ticks[i] > TOUCH_THRESH)
        {
            if (curr_ticks[i] > high)
            {
                high = curr_ticks[i];
                highidx = i;
            }
        }
    }

    if (highidx < 0xFF)
    {
        ret = highidx + 1;
    }

    return ret;
}

uint32_t ticks_ctrl(TickCtrl_e ctrl, int button)
{
    int i;

    switch (ctrl)
    {
    case TICKS_RESET:

        for (i = 0; i < BUTTONS; i++)
        {
            ticks[i] = 0;
            ticks_stopped[i] = true;
        }

        ticks_reset();
        return 0;

    case TICKS_START:

        ticks_stopped[button] = false;
        ticks[button] = TICK_PERIOD - ticks_get();
        return 0;

    case TICKS_STOP:

        ticks[button] = TICK_PERIOD - ticks_get() - ticks[button];
        ticks_stopped[button] = true;
        return 0;

    case TICKS_STOPPED:

        return ticks_stopped[button];

    case TICKS_ALL_STOPPED:

        for (i = 0; i < BUTTONS; i++)
        {
            if (false == ticks_stopped[i])  return false;
        }
        return true;

    case TICKS_GET:

        return ticks[button];

    default:
        assert(0);
        return 0;
    }
}

#endif

void ticks_reset(void)
{
    // Clear the current tick count and re-enable the counter

    SysTickPeriodSet(TICK_PERIOD);
    HWREG(NVIC_ST_CURRENT) = 0;
    SysTickEnable();
}

unsigned ticks_get(void)
{
    return SysTickValueGet();
}

