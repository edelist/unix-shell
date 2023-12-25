CC = gcc 
CFLAGS = -std=c99 -Wall -Wextra

myshell: myshell.c
        $(CC) $(CFLAGS) -o $@ $^

clean:
        rm -f myshell