CC = gcc
CFLAGS = -I./include -Wall -std=c99
LDFLAGS = -lraylib -lm -ldl -lpthread -lGL -lrt -lX11 -lXrandr -lXi -lXcursor

SRC = main.c
OBJ = $(SRC:.c=.o)
OUT = main

all: $(OUT)

$(OUT): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(OUT)
