# 02_http_client - HTTP Client
# Modules: core codec net
ifeq ($(origin CC),default)
CC       = cl65
endif
CC      ?= cl65
BUILD    = build
LIBUCI   = ../..
SRCS     = main.c http_client.c json.c $(LIBUCI)/src/core.c $(LIBUCI)/src/codec.c $(LIBUCI)/src/net.c
PRG      = $(BUILD)/02_http_client.prg
C64U    ?= c64u

IS_OSCAR := $(findstring oscar,$(notdir $(CC)))

.PHONY: all run clean

all: $(PRG)

$(PRG): $(SRCS) | $(BUILD)
ifeq ($(IS_OSCAR),)
	$(CC) -t c64 -I. -I$(LIBUCI)/include -O -o $@ $(SRCS)
else
	$(CC) -i=. -i=$(LIBUCI)/include -O -o=$@ $(SRCS)
endif

$(BUILD):
	mkdir -p $(BUILD)

run: $(PRG)
	curl -X POST --data-binary @$(PRG) "http://$(C64U)/v1/runners:run_prg"

clean:
	rm -rf $(BUILD)
