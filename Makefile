CFLAGS = -g -Wall -DDEBUG 
EXEC = server client
SRC= $(EXEC:=.c)
OBJ= $(SRC:.c=.o)

.PRECIOUS: $(OBJ)

all: $(EXEC)

server: server.o funcs.o

client: client.o funcs.o

%: %.o
	gcc  $^ -o $@

%.o: %.c %.h
	gcc $(CFLAGS) -c $< -o $@

clean: 
	\rm -rf $(OBJ) *.o $(EXEC)