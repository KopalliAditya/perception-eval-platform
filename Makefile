# Makefile — AV Perception Evaluation Platform (pev)
#
# Plain clang++/GNU-make build; no CMake required. Produces:
#   build/pev         the command-line binary
#   build/pev_tests   the unit + integration test runner
#
#   make            build the pev binary (default)
#   make all        build pev and the test runner
#   make test       build and run the test suite
#   make clean      remove the build directory
#
# Author: Aditya Kopalli, 2026. MIT licensed.

CXX      ?= clang++
CXXSTD   ?= c++17
WARNINGS := -Wall -Wextra -Wpedantic
OPT      ?= -O2
CXXFLAGS ?= -std=$(CXXSTD) $(WARNINGS) $(OPT) -Iinclude
LDFLAGS  ?=
LDLIBS   ?= -pthread

BUILD := build

# Library translation units (everything in src/ except the CLI entry point).
LIB_SRCS := \
	src/assignment.cpp \
	src/dataset.cpp \
	src/evaluator.cpp \
	src/generator.cpp \
	src/geometry.cpp \
	src/io_util.cpp \
	src/matching.cpp \
	src/metrics.cpp \
	src/report.cpp \
	src/taxonomy.cpp \
	src/tracking.cpp \
	src/triage.cpp

LIB_OBJS  := $(patsubst src/%.cpp,$(BUILD)/%.o,$(LIB_SRCS))
MAIN_OBJ  := $(BUILD)/main.o
TEST_OBJ  := $(BUILD)/test_main.o

PEV_BIN   := $(BUILD)/pev
TEST_BIN  := $(BUILD)/pev_tests

HEADERS   := $(wildcard include/*.hpp)

.PHONY: all test clean run-example

$(PEV_BIN): $(LIB_OBJS) $(MAIN_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

all: $(PEV_BIN) $(TEST_BIN)

$(TEST_BIN): $(LIB_OBJS) $(TEST_OBJ)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LDLIBS) -o $@

test: $(TEST_BIN)
	./$(TEST_BIN)

$(BUILD)/%.o: src/%.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD)/test_main.o: tests/test_main.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD):
	mkdir -p $(BUILD)

clean:
	rm -rf $(BUILD)
