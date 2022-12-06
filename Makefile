CC = gcc
CFLAG = -Os -Wall -Wextra -static
LIB = -lpthread

img: img.c
	$(CC) $(CFLAG) $< -o ../$@ $(LIB)