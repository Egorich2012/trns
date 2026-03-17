CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra
TARGET = trns
SRC = labatr.cpp

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

test: $(TARGET)
	./$(TARGET) tests/correct/test1.txt || true

.PHONY: clean test