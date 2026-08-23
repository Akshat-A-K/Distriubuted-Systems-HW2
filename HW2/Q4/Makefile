CXX ?= g++
MPICXX ?= mpicxx
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

BUILD_DIR := build

.PHONY: all sequential mpi generator clean

all: sequential mpi generator

sequential: $(BUILD_DIR)/triangle_seq

mpi: $(BUILD_DIR)/triangle_mpi

generator: $(BUILD_DIR)/generate_graph

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/triangle_seq: triangle_seq.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/triangle_mpi: triangle_mpi.cpp | $(BUILD_DIR)
	$(MPICXX) $(CXXFLAGS) $< -o $@

$(BUILD_DIR)/generate_graph: generate_graph.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -rf $(BUILD_DIR)