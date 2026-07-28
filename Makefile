CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -O2
TARGET := bin/app
TOOLS_TARGET := bin/fast_gen

.PHONY: all clean tools

all: $(TARGET)

$(TARGET): src/main.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $@

tools: tools/fast_gen.cpp
	@mkdir -p bin
	$(CXX) $(CXXFLAGS) $< -o $(TOOLS_TARGET)

clean:
	rm -rf bin
