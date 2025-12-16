# compile sender
gcc sender.c crc32.c sha256.c -lssl -lcrypto -o sender

# compile receiver
gcc receiver.c crc32.c sha256.c -lssl -lcrypto -o receiver


# run sender
./sender 172.0.0.1 7777 5555 test.jpg
./sender 192.168.238.159 5555 7777 test.jpg

# run receiver
./receiver 5556 7776

# NetDerper
{
  "Data": {
    "Connection": { "SourcePort": 5555, "TargetPort": 5556, "TargetHostName":"192.168.238.159"},
    "Manipulation": { "DropRate": 5, "ErrorRate": 5 },
    "Delay": { "Mean": 0, "StdDev": 0 }
  },
  "Acknowledgement": {
    "Connection": { "SourcePort": 7776, "TargetPort": 7777, "TargetHostName": "192.168.238.159"},
    "Manipulation": { "DropRate": 5, "ErrorRate": 5 },
    "Delay": { "Mean": 0, "StdDev": 0 }
  }
}

# read the port data raw
sudo tcpdump -i lo udp port 7777 -vv