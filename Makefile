PROGRAM = BS53LIV

EXTRA_COMPONENTS = \
	extras/http-parser \
	extras/dhcpserver \
  extras/rboot-ota \
  extras/i2c \
  extras/bh1750 \
  $(abspath ../../components/common/button) \
  $(abspath ../../components/common/ota-api) \
	$(abspath ../../components/esp8266-open-rtos/wifi_config) \
	$(abspath ../../components/esp8266-open-rtos/cJSON) \
	$(abspath ../../components/common/wolfssl) \
	$(abspath ../../components/common/homekit)

FLASH_SIZE ?= 8
FLASH_MODE ?= dout
FLASH_SPEED ?= 40
HOMEKIT_SPI_FLASH_BASE_ADDR ?= 0x7A000

EXTRA_CFLAGS += -I../.. -DHOMEKIT_SHORT_APPLE_UUIDS

include $(SDK_PATH)/common.mk

monitor:
	$(FILTEROUTPUT) --port $(ESPPORT) --baud 115200 --elf $(PROGRAM_OUT)
