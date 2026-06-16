CC = cc
CFLAGS = -std=c11 -Wall -Wextra -O2 -I.
TARGET = pacman-raylib
SRC = main_raylib.c model/model.c

# No Mac com Homebrew, pkg-config costuma funcionar após: brew install raylib pkg-config
RAYLIB_FLAGS := $(shell pkg-config --libs --cflags raylib 2>/dev/null)
ifeq ($(strip $(RAYLIB_FLAGS)),)
RAYLIB_FLAGS = -lraylib -framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo
endif

all: $(TARGET)

$(TARGET): $(SRC) pacman.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(RAYLIB_FLAGS) -lm

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
