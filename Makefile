CXX			:= g++
CXXFLAGS	:= -O2 -std=c++17 -Wall -Wextra -Isrc -MMD -MP -fno-exceptions -fno-rtti
LDLIBS		:=

SRC_DIR		:= src
BUILD_DIR	:= build
BIN			:= evdev-forward

SRCS		:= $(shell find $(SRC_DIR) -name '*.cpp')
OBJS		:= $(patsubst $(SRC_DIR)/%.cpp,$(BUILD_DIR)/%.o,$(SRCS))
DEPS		:= $(OBJS:.o=.d)

all: $(BIN)

$(BIN): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(BIN)

-include $(DEPS)

.PHONY: all clean