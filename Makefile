CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I. -pthread
LDFLAGS = -pthread

SRCDIR = src
SOURCES = $(wildcard $(SRCDIR)/core/*.cpp) $(wildcard $(SRCDIR)/network/*.cpp) $(wildcard $(SRCDIR)/rpc/*.cpp) $(wildcard $(SRCDIR)/wallet/*.cpp)
OBJECTS = $(SOURCES:.cpp=.o)
TARGET = pragma_node

.PHONY: all clean

all: $(TARGET)

$(TARGET): pragma_node.cpp $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)
	rm -rf pragma_data_*
