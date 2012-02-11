# Variables
CC = gcc
CFLAGS = -Wall
DEBUG = 
CC_ALL = $(CC) $(CFLAGS) $(DEBUG)
LINKER = -pthread

targets_server = error.o ptftpd.o
targets_client = error.o ptftp.o

all:
	server client

server: $(targets_server)
	$(CC_ALL) $^ $(LINKER) -o ptftpd

client: $(targets_client)
	$(CC_ALL) $^ -o ptftp

clean:
	rm -f ptftpd ptftp *.o *.gch *~

# Object files
ptftpd.o: ptftpd.c ptftp.h
	$(CC_ALL) -c $^

ptftp.o: ptftp.c ptftp.h
	$(CC_ALL) -c $^

error.o: error.c error.h
	$(CC_ALL) -c $^

