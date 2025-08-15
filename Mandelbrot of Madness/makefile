# Compiler and flags
CXX      := g++
CXXFLAGS := -O3 -march=native -ffast-math -std=c++20
LDFLAGS  := 
SRC      := mandelbrot.cpp
BIN      := mandelbrot

# Default target: build with OpenMP
all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) -fopenmp $(SRC) -o $(BIN) $(LDFLAGS)

# Build without OpenMP
noomp: $(SRC)
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN) $(LDFLAGS)

# Build and run (with OpenMP)
run: all
	./$(BIN)

# Remove build artifacts
clean:
	rm -f $(BIN)
