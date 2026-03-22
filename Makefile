CXX = g++
CXXFLAGS = -std=c++11 -O2 -pthread
TARGET = brandes_thread
SRC = brandes_thread.cpp

.PHONY: all run run-file clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

run: $(TARGET)
	./$(TARGET)

run-file: $(TARGET)
	./$(TARGET) $(FILE)

clean:
	rm -f $(TARGET)
