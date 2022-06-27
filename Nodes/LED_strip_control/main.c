/*
 * Copyright (C) 2017 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     tests
 * @{
 *
 * @file
 * @brief       Test application for the APA102 LED strip driver
 *
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 *
 * @}
 */

#include <string.h>
#include <stdio.h>
#include <stdint.h>

#include "xtimer.h"
#include "color.h"
#include "event.h"
#include "event/periodic.h"
#include "apa102.h"
#include "apa102_params.h"
#include "periph/gpio.h"

int pos = 0;

/**
 * @brief   Switch to the next LED every 10ms
 */
#define STEP        (200 * US_PER_MS)

/**
 * @brief   Interval for dimming colors
 */
#define DIM         (100 * US_PER_MS)

/**
 * @brief   Step through brightness levels (0-255) in 32 steps
 */
#define BSTEP       (8U)

/**
 * @brief   Allocate the device descriptor
 */
static apa102_t dev;

gpio_t button = GPIO_PIN(0, 12);
/**
 * @brief   Allocate a color_rgb_t struct for each LED on the strip
 */
static color_rgba_t leds[APA102_PARAM_LED_NUMOF];
static event_periodic_t event_periodic;
static event_queue_t event_queue;

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

static void button_handler(event_t *event)
{
    (void)event;
    puts("Button!");
    printf("pos: %d\n", pos);
    if (pos == 4) {
        puts("You win");
        event_periodic_stop(&event_periodic);
        event_queue_init(&event_queue);
        for (unsigned i = 0; i <= UINT8_MAX; i += BSTEP) {
            setcolor(1, (uint8_t)i);
            apa102_load_rgba(&dev, leds);
            xtimer_usleep(DIM);
        }
    }
    else {
        memset(leds, 0, sizeof(color_rgba_t) * APA102_PARAM_LED_NUMOF);
        for (unsigned i = 0; i < APA102_PARAM_LED_NUMOF; i++) {
           leds[i].alpha = 255;
        }
        pos = 0;
        /* apply the values to the LED strip */
        apa102_load_rgba(&dev, leds);
    }
}

static void timeout_handler(event_t *event)
{
    (void)event;
    static int step = 1;
    static color_hsv_t col = { 0.0, 1.0, 1.0 };
    printf("Timeout!\n");
    printf("pos: %d\n", pos);

    /* change the active color by running around the hue circle */
    col.h += 1.0;
    if (col.h > 360.0) {
        col.h = 0.0;
    }
    /* set the active LED to the active color value */
    memset(&leds[pos].color, 0, sizeof(color_rgb_t));

    /* switch direction once reaching an end of the strip */
    if (!pos) {
        step = 1;
    }
    else if (pos == (APA102_PARAM_LED_NUMOF - 1)) {
        step = -1;
    }
    pos += step;

    color_hsv2rgb(&col, &leds[pos].color);

    /* apply the values to the LED strip */
    apa102_load_rgba(&dev, leds);

    puts("here");   
}

static event_t button_event = { .handler = button_handler };
static event_t timeout_event = { .handler = timeout_handler };

void button_callback(void *arg)
{
    (void) arg; /* the argument is not used */
    event_post(&event_queue, &button_event);
}

int main(void)
{
    gpio_mode_t input_mode = GPIO_IN_PU;
    gpio_init(button, input_mode);

    puts("APA102 Test App");

    /* initialize all LED color values to black (off) */
    memset(leds, 0, sizeof(color_rgba_t) * APA102_PARAM_LED_NUMOF);
    for (unsigned i = 0; i < APA102_PARAM_LED_NUMOF; i++) {
        leds[i].alpha = 255;
    }

    /* initialize the driver */
    apa102_init(&dev, &apa102_params[0]);

    gpio_init_int(button, GPIO_IN_PU, GPIO_FALLING, button_callback, NULL);

    /* reset color values */
    apa102_load_rgba(&dev, leds);

    event_queue_init(&event_queue);

    puts("Initialization done.");

    event_periodic_init(&event_periodic, ZTIMER_USEC, &event_queue, (event_t*)&timeout_event);
    event_periodic_start(&event_periodic, STEP);
    event_loop(&event_queue);

    return 0;
}
