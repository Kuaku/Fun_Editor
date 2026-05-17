CC = gcc
CFLAGS = -I./include -I./src -Wall -std=c99
LDFLAGS = -lraylib -lm -ldl -lpthread -lGL -lrt -lX11 -lXrandr -lXi -lXcursor

SRC = src/main.c \
      src/platform/platform.c \
      src/statistics/statistics.c \
      src/filesystem/filesystem.c \
      src/text_buffer/text_buffer.c \
      src/command/command.c \
      src/input/input.c \
      src/modal/modal.c \
      src/modal/modals/statistics_modal.c \
      src/modal/modals/buffer_list.c \
      src/modal/modals/string_input.c \
      src/modal/modals/file_explorer.c \
      src/editor/editor.c \
      src/render/render.c \
      src/utils/utf8.c

OBJ = $(SRC:.c=.o)
OUT = main

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT)
