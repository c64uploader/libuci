# Makefile for libuci
#
# Builds example programs and hardware-in-the-loop tests for the
# Ultimate Command Interface.  Supports two compilers:
#
#   make               # cc65 (default, uses cl65)
#   make CC=oscar64    # oscar64
#
# Targets:
#   make              Build examples and tests
#   make examples     Build only the example programs
#   make build-tests  Build only the test PRGs
#   make test         Build test PRGs and run them on hardware (requires C64U_ADDRESS)
#   make clean        Remove all build artifacts
#   make help         Show available targets

# Configuration
ifeq ($(origin CC),default)
CC        = cl65
endif
CC       ?= cl65

# Test runner filter - pass a Go -run pattern to select tests,
# e.g.  make test TEST_RUN=TestDOS/SetTime
TEST_RUN ?=

# Build directories
BUILD_DIR = build
TEST_BUILD_DIR = tests/build

# Detect compiler for flag differences.
IS_OSCAR := $(findstring oscar,$(notdir $(CC)))

# Library sources
LIBSRCS  = src/core.c src/codec.c src/dos.c src/net.c src/ctrl.c src/siec.c
LIB_OBJS = $(patsubst src/%.c,$(TEST_BUILD_DIR)/lib/%.o,$(LIBSRCS))

# Example directories (each has its own Makefile)
EXAMPLE_DIRS := $(sort $(wildcard examples/*/))

# Test programs (one per source in tests/c/)
TEST_SRCS := $(wildcard tests/c/test_*.c)
TEST_PRGS := $(patsubst tests/c/%.c,$(TEST_BUILD_DIR)/%.prg,$(TEST_SRCS))
TEST_OBJS := $(patsubst tests/c/%.c,$(TEST_BUILD_DIR)/%.o,$(TEST_SRCS))

# Targets
.PHONY: all build-tests examples test clean help

## all            Build examples and tests
all: examples build-tests

## examples       Build only the example programs
examples:
	@for dir in $(EXAMPLE_DIRS); do $(MAKE) -C $$dir CC=$(CC); done

## build-tests    Build only the test PRGs
build-tests: $(TEST_PRGS)

## test           Build and run tests on hardware (TEST_RUN=Pattern)
test: build-tests
ifeq ($(TEST_RUN),)
	go -C tests/go test -v -timeout 10m
else
	go -C tests/go test -v -timeout 10m -run $(TEST_RUN)
endif

## clean          Remove all build artifacts
clean:
	rm -rf $(BUILD_DIR) $(TEST_BUILD_DIR)
	@for dir in $(EXAMPLE_DIRS); do $(MAKE) -C $$dir clean; done

## help           Show this help
help:
	@echo "Usage:  make <target> [VARIABLE=value]"
	@echo ""
	@grep -E '^## ' Makefile | \
	awk '{ sub(/^## /,""); split($$0, a, "  "); target=a[1]; desc=$$0; sub(/^[^ ]+  +/,"",desc); printf "  %-16s %s\n", target, desc }'

ifeq ($(IS_OSCAR),)
# Compile library sources to .o (once, shared by all tests)
$(LIB_OBJS): $(TEST_BUILD_DIR)/lib/%.o: src/%.c
	@mkdir -p $(TEST_BUILD_DIR)/lib
	$(CC) -t c64 -Iinclude -O -c -o $@ $<

# Compile test sources to .o
$(TEST_OBJS): $(TEST_BUILD_DIR)/%.o: tests/c/%.c tests/c/harness.h
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) -t c64 -Iinclude -O -c -o $@ $<

# Link test programs
$(TEST_PRGS): $(TEST_BUILD_DIR)/%.prg: $(TEST_BUILD_DIR)/%.o $(LIB_OBJS)
	$(CC) -t c64 -o $@ $^
else
# Compile test PRGs directly from sources with oscar64 (no -c option)
$(TEST_PRGS): $(TEST_BUILD_DIR)/%.prg: tests/c/%.c $(LIBSRCS) tests/c/harness.h
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) -i=include -O -o=$@ $< $(LIBSRCS)
endif

# Build directory creation
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

## list-tests     Show available test names for TEST_RUN=
list-tests:
	@echo "Available tests (use with: make test TEST_RUN=<name>):"
	@echo ""
	@awk '/^func Test[A-Z]/ { sub(/^func /,""); sub(/\(.*/,""); suite=$$0 } \
	      /t\.Run\("/ { sub(/.*t\.Run\("/,""); sub(/".*/,""); print "  " suite "/" $$0 }' \
	      tests/go/suite_test.go | sort
