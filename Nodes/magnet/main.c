/*
 * Copyright (C) 2022 HAW Hamburg
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

#include <stdio.h>

#include "board.h"
#include "ztimer.h"
#include "periph/gpio.h"

 /* [TASK 2: define button and led1 here] */
gpio_t input = GPIO_PIN(PORT_A, 1);
gpio_t led0 = GPIO_PIN(PORT_D, 6);
gpio_mode_t input_mode = GPIO_IN_PU;
gpio_mode_t led0_mode = GPIO_OUT;

int main(void)
{
    puts("Magnet example.");
    gpio_init(led0, led0_mode);
    gpio_set(led0);
    gpio_init(input, input_mode);
    
    while (1){;
        if (gpio_read(input) == 0){
            gpio_clear(led0);
            puts("magnet detected");}
        else{
            gpio_set(led0);
            puts("no magnet");
        }
        ztimer_sleep(ZTIMER_MSEC, 50);
    }

    return 0;
}
