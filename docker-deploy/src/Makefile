# Compiler settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Werror -ggdb3 -fPIC -ggdb3
LDFLAGS = -lpthread

# Build targets
TARGET = main
SOURCES = main.cpp proxy.cpp request.cpp response.cpp cache.cpp log.cpp
HEADERS = proxy.hpp request.hpp response.hpp cache.hpp log.hpp
OBJECTS = $(SOURCES:.cpp=.o)

# Default target
all: $(TARGET)

# Link the program (Fix: Moved -lpthread here)
$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Compile source files
%.o: %.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# Clean build files
clean:
	rm -f $(TARGET) $(OBJECTS)
