# Variables
CC = gcc
CFLAGS = -Wall -g `sdl2-config --cflags` -I./fonts -I./images
LDFLAGS = `sdl2-config --libs` -lSDL2_image -lSDL2_ttf
SRC = src/terminal.c
BIN = terminal
INSTALL_DIR = /opt/mi_terminal

# Reglas
all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

install: $(BIN)
	mkdir -p $(INSTALL_DIR)/images
	mkdir -p $(INSTALL_DIR)/fonts
	cp $(BIN) $(INSTALL_DIR)/
	cp images/* $(INSTALL_DIR)/images/
	cp fonts/* $(INSTALL_DIR)/fonts/

uninstall:
	rm -rf $(INSTALL_DIR)

clean:
	rm -f $(BIN)

.PHONY: all install uninstall clean
