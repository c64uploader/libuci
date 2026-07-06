# 04_dma_fastload - DMA Fast Loader
# Modules: core codec siec
ifeq ($(origin CC),default)
CC       = cl65
endif
CC      ?= cl65
BUILD    = build
LIBUCI   = ../..
SRCS     = main.c $(LIBUCI)/src/core.c $(LIBUCI)/src/codec.c $(LIBUCI)/src/siec.c
PRG      = $(BUILD)/04_dma_fastload.prg
C64U    ?= c64u

IS_OSCAR := $(findstring oscar,$(notdir $(CC)))

.PHONY: all run clean

all: $(PRG)

$(PRG): $(SRCS) | $(BUILD)
ifeq ($(IS_OSCAR),)
	$(CC) -t c64 -I$(LIBUCI)/include -O -o $@ $(SRCS)
else
	$(CC) -i=$(LIBUCI)/include -O3 -o=$@ $(SRCS)
endif

$(BUILD):
	mkdir -p $(BUILD)

run: $(PRG)
	curl -X POST --data-binary @$(PRG) "http://$(C64U)/v1/runners:run_prg"

clean:
	rm -rf $(BUILD)
