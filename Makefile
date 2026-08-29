CXX			:= g++
CXXFLAGS	:= -O2 -std=c++17 -Wall -Wextra -Isrc
LDLIBS		:=

SRC_DIR		:= src
BUILD_DIR	:= build
BIN			:= input-forwarder

SRCS		:= $(shell find $(SRC_DIR) -name '*.cpp')
OBJS		:= $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BUILD_DIR):
		mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN)

.PHONY: all clean