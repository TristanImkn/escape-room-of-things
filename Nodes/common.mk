ROOT_DIR = $(abspath $(dir $(lastword $(MAKEFILE_LIST))))

EXTERNAL_MODULE_DIRS := $(ROOT_DIR)/modules
RIOTBASE ?= $(ROOT_DIR)/RIOT

JSON_OUTPUT = node.json
UUID_OUTPUT = uuid.txt
QRCODE = qrcode.png

# hardcoded wow, many secure, much professional, such cool
PSK_ID = hardcoded_id
PSK = hardcoded_psk

ifneq (,$(filter qr,$(MAKECMDGOALS)))

$(UUID_OUTPUT):
	python3 -c "import uuid; print(uuid.uuid4())" > $@

$(JSON_OUTPUT): $(UUID_OUTPUT)
	echo {\
	\"serial\":\"`cat $(UUID_OUTPUT)`\",\
	\"id\":\"$(PSK_ID)\",\
	\"psk\": \"$(PSK)\",\
	\"name\":\"$(APPLICATION)\"\
	} > $@

$(QRCODE): $(JSON_OUTPUT)
ifeq (, $(shell which qrencode))
	$(warning The tool qrencode is not installed. Falling back to an online service...)
	wget 'https://api.qrserver.com/v1/create-qr-code/?size=300x300&data=$(shell cat $(JSON_OUTPUT))' -O $@
else
	qrencode -o $@ < $<
endif

.PHONY: qr
qr: $(QRCODE)
else
CFLAGS += -DSERIAL_UUID=\"$(shell cat $(UUID_OUTPUT))\"
CFLAGS += -DPSK=\"$(PSK)\"
CFLAGS += -DPSK_ID=\"$(PSK_ID)\"
CFLAGS += -DCONFIG_CORD_EP=\"$(shell cat $(UUID_OUTPUT))\"
CFLAGS += -DTHREAD_STACKSIZE_MAIN=THREAD_STACKSIZE_DEFAULT*3
include $(RIOTBASE)/Makefile.include
endif
