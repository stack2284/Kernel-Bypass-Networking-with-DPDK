# --- Variables ---
CXX = g++
CXXFLAGS = -O3 -march=native -std=c++17 -g

# Project Structure
SRC_DIR = src
BUILD_DIR = build

# --- DPDK Flags ---
PKG_CONFIG = pkg-config
DPDK_CFLAGS = $(shell $(PKG_CONFIG) --cflags libdpdk)
DPDK_LIBS = $(shell $(PKG_CONFIG) --libs libdpdk)

# Combine all flags
CFLAGS = $(CXXFLAGS) $(DPDK_CFLAGS)
LDFLAGS = $(DPDK_LIBS)

# --- Targets ---

# Default target: build both
all: $(BUILD_DIR)/l2_echo $(BUILD_DIR)/l2_multi_echo

# Rule for the original l2_echo
$(BUILD_DIR)/l2_echo: $(SRC_DIR)/l2_echo.cpp
	@echo "--- Compiling l2_echo ---"
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "--- Build Complete: $(BUILD_DIR)/l2_echo ---"

# Rule for the new l2_multi_echo
$(BUILD_DIR)/l2_multi_echo: $(SRC_DIR)/l2_multi_echo.cpp
	@echo "--- Compiling l2_multi_echo ---"
	@mkdir -p $(BUILD_DIR)
	$(CXX) $(CFLAGS) -o $@ $< $(LDFLAGS)
	@echo "--- Build Complete: $(BUILD_DIR)/l2_multi_echo ---"

clean:
	@echo "--- Cleaning build files ---"
	rm -rf $(BUILD_DIR)

.PHONY: all clean
