# Pokedex SDL2 Makefile
# =============================================================================

# Default to local build
CC = gcc
STRIP = strip
CFLAGS = -Iinclude -Wall -O2 -I/usr/include/SDL2
LIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lm

SRC = src/main.c src/tsv_parser.c
OBJ = $(SRC:.c=.o)
TARGET = bin/pokedex

# Cross-compilation (e.g. make CROSS_COMPILE=aarch64-linux-gnu-)
# Cross-compilation (e.g. make CROSS_COMPILE=aarch64-linux-gnu-)
ifdef CROSS_COMPILE
    CC = $(CROSS_COMPILE)gcc
    STRIP = $(CROSS_COMPILE)strip
    # On Ubuntu, the headers are shared, and the compiler knows where to find
    # the ARM64 libraries in /usr/lib/aarch64-linux-gnu/
    CFLAGS = -Iinclude -Wall -O2 -I/usr/include/SDL2
    LIBS = -lSDL2 -lSDL2_image -lSDL2_ttf -lm
endif

.PHONY: all clean local arm64

all: $(TARGET)

$(TARGET): $(OBJ)
	mkdir -p bin
	$(CC) -o $@ $^ $(LIBS)
	$(STRIP) $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)

# Helper for TrimUI Brick (NextUI)
arm64:
	$(MAKE) CROSS_COMPILE=aarch64-linux-gnu-
	mkdir -p Pokedex.pak/bin/arm64
	mv $(TARGET) Pokedex.pak/bin/arm64/pokedex

# Local build for prototyping
local: CFLAGS += -DDESKTOP
local: all

# Local Test Run
run: local
	(cd Pokedex.pak && ../bin/pokedex)
