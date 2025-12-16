CC = gcc
CFLAGS =
LIBS = -lssl -lcrypto
SRC = crc32.c sha256.c

all: receiver


receiver: receiver.c $(SRC)
	$(CC) $(CFLAGS) receiver.c $(SRC) $(LIBS) -o receiver

clean:
	rm -f sender receiver
