/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @{
 *
 * @file
 * @brief       Example on using the puzzle_coap module
 *
 * @author      Bennet Blischke <bennet.blischke@haw-hamburg.de>
 *
 * @}
 */

#include <stdio.h>
#include "shell.h"
#include "xtimer.h"
#include "hx711.h"
#include "hx711_params.h"
#include "periph/gpio.h"
#include "test_utils/expect.h"
#include "thread.h"
#include "ztimer.h"

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "color.h"

#include "apa102.h"
#include "apa102_params.h"

#include "puzzle_coap.h"


#define STEP        (200 * US_PER_MS)

#define DIM         (100 * US_PER_MS)

#define BSTEP       (8U)

#define weight 422 /*wanted weight*/
#define tolerance 0.2f /*tolerance of wanted weight*/

#define STARTUP_DELAY   (1U)
#define MAIN_QUEUE_SIZE (4)


static apa102_t dev_apa;

static color_rgba_t leds[APA102_PARAM_LED_NUMOF];

static msg_t _main_msg_queue[MAIN_QUEUE_SIZE];

static void setcolor(int color, uint8_t alpha)
{
    for (int i = 0; i < (int)APA102_PARAM_LED_NUMOF; i++) {
        leds[i].alpha = alpha;
        memset(&leds[i].color, 0, sizeof(color_rgb_t));
        switch (color) {
            case 0:
                leds[i].color.r = 255;
                break;
            case 1:
                leds[i].color.g = 255;
                break;
            case 2:
                leds[i].color.b = 255;
                break;
        }
    }
}

char meassure_thread_stack[THREAD_STACKSIZE_MAIN + THREAD_EXTRA_STACKSIZE_PRINTF];

/* forward declarations */
static bool _solved(void);
static bool _ready(void);
static void _set_ready(bool maintainance);

/* Setting up the puzzle */
static const puzzle_t puzzle = {
    .get_solved_handler = _solved,
    .get_ready_handler = _ready,
    .set_ready_handler = _set_ready,
    .name = "Weight puzzle",
    .resource_dir_uri = "coap://[fd00:dead:beef::1]:5555",
};

static bool _is_solved = false;
static bool _is_ready = true;

void *meassure_weight(void *arg)
{
    (void) arg;
    int pos = 0;
    int step = 1;
    color_hsv_t col = { 0.0, 1.0, 1.0 };

    /* initialize all LED color values to black (off) */
    memset(leds, 0, sizeof(color_rgba_t) * APA102_PARAM_LED_NUMOF);

    /* initialize the driver */
    apa102_init(&dev_apa, &apa102_params[0]);

    int cnt = 0; /*counter*/
    static hx711_t dev;
    uint8_t times = 10;

    puts("In thread");
    /*initialize LED*/
    gpio_t led0 = GPIO_PIN(PORT_D, 6);
    gpio_mode_t led0_mode = GPIO_OUT;

    gpio_init(led0, led0_mode);

    /*initialize load cell and reset offset*/
    hx711_init(&dev, &(hx711_params[0]));
    hx711_power_up(&dev);
    hx711_tare(&dev, times);
    int32_t units_after = hx711_get_units(&dev, times);

    expect(units_after <= 1);

    puts("Entering while loop");
    /*endless loop*/
    while (1) {
        int32_t units = hx711_get_units(&dev, times);
        printf("%ld\n", units);
        xtimer_ticks32_t now = xtimer_now();
        if (units < weight) {
            /* change the active color by running around the hue circle */
            col.h += 1.0;
            if (col.h > 360.0) {
                col.h = 0.0;
            }

            /* set the active LED to the active color value */
            memset(&leds[pos].color, 0, sizeof(color_rgb_t));
            pos += step;
            color_hsv2rgb(&col, &leds[pos].color);

            /* apply the values to the LED strip */
            apa102_load_rgba(&dev_apa, leds);

            /* switch direction once reaching an end of the strip */
            if ((pos == (APA102_PARAM_LED_NUMOF - 1)) || (pos == 0)) {
                step *= -1;
            }

            xtimer_periodic_wakeup(&now, STEP);
        }
        else {
            setcolor(2,255);
        }
        /* int32_t value = hx711_get_value(&dev, times); */
        /*check if measurement is within the borders*/
        if ((units > weight - weight * tolerance) && (units < weight + weight * tolerance)) {
            cnt = cnt + 1; 
        }
        else {
            cnt = 0;
            gpio_set(led0);
            if (_is_solved) {
                _is_solved = false;
                /* Notify the module about the news */
                puzzle_update();
            }
        }

        /*set LED if measurement stays 5 seconds*/
        if (cnt == 5) {
            gpio_clear(led0);
            _is_solved = true;
            /* Notify the module about the news */
            puzzle_update();
        }
    }
    return NULL;
}


int main(void)
{
    /* just wait a sec, otherwise the `make falsh term` users 
     * wouldn't see the first print's and might think the app is hanging */
    xtimer_sleep(STARTUP_DELAY);

    /* Initialise the puzzle coap module */
    puzzle_init(&puzzle);

    puts("Starting thread");
    thread_create(
        meassure_thread_stack,
        sizeof(meassure_thread_stack),
        THREAD_PRIORITY_MAIN + 1,
        THREAD_CREATE_STACKTEST,
        meassure_weight, NULL, "meassure_weight"
    );
    puts("go on to shell");

    /* for the thread running the shell */
    msg_init_queue(_main_msg_queue, MAIN_QUEUE_SIZE);
    
    /* start shell */
    puts("All up, running the shell now");
    char line_buf[SHELL_DEFAULT_BUFSIZE];
    shell_run(NULL, line_buf, SHELL_DEFAULT_BUFSIZE);
    return 0;
}

/* dummy shell commande to "solve" this puzzle 
 * a real puzzle would call `puzzle_update()` everytime it gets 
 * solved (or when it is no-longer solved). */
static int _cmd_solve(int argc, char **argv)
{
    (void) argc;
    (void) argv;
    _is_solved = !_is_solved;
    /* Notify the module about the news */
    puzzle_update();
    return 0;
}


/* callback handler for the coap API. Returns if the puzzle is solved */
static bool _solved(void)
{
    return _is_solved;
}

/* callback handler for the coap API. Returns if the puzzle is ready to be solved */
static bool _ready(void)
{
    return _is_ready;
}

/* callback handler for the coap API. Is called to set the puzzle in either maintainance or ready mode */
static void _set_ready(bool maintainance)
{
    if (maintainance) {
        /* We are in maintainance mode now; Open the doors, locks etc.... */
        _is_ready = false;
    } else {
        /* Get back to be ready to play! */
        _is_ready = true;
    }
}

SHELL_COMMAND(puzzle_solve, "Toggle if the puzzle is solved", _cmd_solve);
