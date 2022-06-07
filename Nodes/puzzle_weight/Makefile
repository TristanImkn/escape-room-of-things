# name of the application
APPLICATION = WeightPuzzle

# If no BOARD is found in the environment, use this default:
BOARD ?= pba-d-01-kw2x

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= ../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# LED strip
USEMODULE += apa102
USEMODULE += color
USEMODULE += xtimer

# For the coap_client
USEMODULE += od

USEMODULE += netdev_default
USEMODULE += auto_init_gnrc_netif
USEMODULE += gnrc_ipv6_default
USEMODULE += gnrc_icmpv6_echo
USEMODULE += netutils

USEMODULE += fmt
USEMODULE += xtimer

USEMODULE += puzzle_coap
USEMODULE += shell
USEMODULE += shell_commands
USEMODULE += ps

FEATURES_REQUIRED += periph_gpio
USEMODULE += periph_gpio
USEMODULE += periph_gpio_irq
USEMODULE += hx711
CFLAGS += "-DHX711_PARAM_SCK=GPIO_PIN(4, 4)"
CFLAGS += "-DHX711_PARAM_DOUT=GPIO_PIN(0, 2)"
#CFLAGS += "-DHX711_PARAM_DOUT=GPIO_PIN(3, 6)"
CFLAGS += "-DHX711_PARAM_DIVIDER=204"


# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

include ../common.mk
