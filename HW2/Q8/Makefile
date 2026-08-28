CXX ?= g++
MPICXX ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build

.PHONY: all sequential mpi generator clean

all: sequential mpi generator

sequential: $(BUILD_DIR)/weather_seq

mpi: $(BUILD_DIR)/weather_mpi

generator: $(BUILD_DIR)/generate

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/weather_seq: weather_seq.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/weather_mpi: weather_mpi.cpp | $(BUILD_DIR)
	$(MPICXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/generate: generate.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)
