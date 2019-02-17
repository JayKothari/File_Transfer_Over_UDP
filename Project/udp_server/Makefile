.PHONY: clean

all: server

server : udpserver.cpp
	g++ -o server udpserver.cpp -lpthread

clean:
	rm -f *.o *.exe server client
