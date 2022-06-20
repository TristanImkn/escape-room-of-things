# name of the application
APPLICATION = LEDPuzzle

# If no BOARD is found in the environment, use this default:
BOARD ?= nRF52840DK

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= ../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:

# LED strip
USEMODULE += apa102
USEMODULE += color
USEMODULE += xtimer

# Button
USEMODULE += periph_gpio
USEMODULE += periph_gpio_irq


include $(RIOTBASE)/Makefile.include
