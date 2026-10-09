CXX      = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17
LDFLAGS  =

TARGETS = kayles_client kayles_server

.PHONY: all clean

all: $(TARGETS)

kayles_client: kayles_client.o client_parser.o validators.o err.o common.o
	$(CXX) $(LDFLAGS) -o $@ $^

kayles_server: kayles_server.o server_parser.o server_handlers.o validators.o err.o common.o
	$(CXX) $(LDFLAGS) -o $@ $^

client_parser.o: client_parser.cpp client_parser.h err.h validators.h messages.h
common.o: common.cpp common.h
err.o: err.cpp err.h
kayles_client.o: kayles_client.cpp err.h validators.h client_parser.h common.h messages.h
kayles_server.o: kayles_server.cpp err.h messages.h server_parser.h common.h server_handlers.h game_session.h
server_handlers.o: server_handlers.cpp err.h server_parser.h server_handlers.h game_session.h messages.h common.h
server_parser.o: server_parser.cpp server_parser.h err.h validators.h
validators.o: validators.cpp validators.h err.h messages.h

clean:
	rm -f $(TARGETS) *.o