CXX ?= g++
MPICXX ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build

.PHONY: all sequential mpi generator clean

all: sequential mpi generator

sequential: $(BUILD_DIR)/bitonic_seq

mpi: $(BUILD_DIR)/bitonic_mpi

generator: $(BUILD_DIR)/generate_data

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/bitonic_seq: bitonic_seq.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/bitonic_mpi: bitonic_mpi.cpp | $(BUILD_DIR)
	$(MPICXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/generate_data: generate_data.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)
