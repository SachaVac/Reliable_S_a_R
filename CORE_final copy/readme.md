# compile sender
gcc sender.c crc32.c sha256.c -lssl -lcrypto -o sender

# compile receiver
gcc receiver.c crc32.c sha256.c -lssl -lcrypto -o receiver

