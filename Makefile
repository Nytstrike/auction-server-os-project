# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -pthread

# Target executable
TARGET = auction

# Source files (added semaphore.c)
SOURCES = main.c auction.c semaphore.c

# Object files
OBJECTS = main.o auction.o semaphore.o

# Header files
HEADERS = auction.h semaphore.h

# Default target
all: $(TARGET)

# Link object files to create executable
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Compile main.c
main.o: main.c $(HEADERS)
	$(CC) $(CFLAGS) -c main.c

# Compile auction.c
auction.o: auction.c auction.h semaphore.h
	$(CC) $(CFLAGS) -c auction.c

# Compile semaphore.c
semaphore.o: semaphore.c semaphore.h
	$(CC) $(CFLAGS) -c semaphore.c

# Clean up build files
clean:
	rm -f $(OBJECTS) $(TARGET)

# Run the program
run: $(TARGET)
	./$(TARGET)

# Help target
help:
	@echo "Available commands:"
	@echo "  make        - Build the auction program"
	@echo "  make run    - Build and run the program"
	@echo "  make clean  - Remove compiled files"
	@echo "  make help   - Show this help message"

.PHONY: all clean run help