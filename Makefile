CC = gcc
CFLAGS = -I./include -I./src -Wall -std=c99
BUILDDIR = build

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

OBJ = $(patsubst src/%.c,$(BUILDDIR)/%.o,$(SRC))

ifeq ($(OS),Windows_NT)
    OUT = main.exe
    LDFLAGS = libraylib.a -lopengl32 -lgdi32 -lwinmm
    MKDIR = if not exist $(subst /,\,$1) mkdir $(subst /,\,$1)
    RM = if exist $(subst /,\,$(BUILDDIR)) rmdir /S /Q $(subst /,\,$(BUILDDIR))
else
    OUT = main
    LDFLAGS = -lraylib -lm -ldl -lpthread -lGL -lrt -lX11 -lXrandr -lXi -lXcursor
    MKDIR = mkdir -p $1
    RM = rm -rf $(BUILDDIR)
endif

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/%.o: src/%.c
	$(call MKDIR,$(dir $@))
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM)
	$(RM) $(OUT)
