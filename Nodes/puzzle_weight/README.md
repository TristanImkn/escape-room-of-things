# Weight puzzle

This program measures the ouput of a load cell and converts it to a weight value. When the value is within the borders of a set goal for 2.5 seconds an LED will turn on, the puzzle is solved. The wanted value is 400g (water) + 22g weight of the bottle. When starting the puzzle the bottle cannot be on top of the weigh.

Connection Load Cell:
VDD - 3.3V
VCC - 5V
DAT - D6
CLK - D2
GND - GND

Connection LED strip:
5V - Externe Spannungsquelle
DI - D11
CI - D13
GND - GND

To set up the right pins and LED number for the LED strip type:
```
$ CFLAGS="-DAPA102_PARAM_DATA_PIN=GPIO_PIN\(2,6\) -DAPA102_PARAM_CLK_PIN=GPIO_PIN\(2,5\) -DAPA102_PARAM_LED_NUMOF=9" make all flash term
```
