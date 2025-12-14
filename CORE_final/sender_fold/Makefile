CC = gcc
CFLAGS =
LIBS = -lssl -lcrypto
SRC = crc32.c sha256.c

all: sender

sender: sender.c $(SRC)
	$(CC) $(CFLAGS) sender.c $(SRC) $(LIBS) -o sender

#receiver: receiver.c $(SRC)
#	$(CC) $(CFLAGS) receiver.c $(SRC) $(LIBS) -o receiver

clean:
	rm -f sender receiver
