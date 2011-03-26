server_target = server
client_target = client
server_objs = src/server.c src/cdata.c
client_objs = src/client.c src/cdata.c
server_headers = src/cdata.h
client_headers = src/cdata.h

LDFLAGS += -pthread
CFLAGS += -Wall -Wextra -g

all: server client

server: cdata $(server_src)
	gcc $(CFLAGS) $(LDFLAGS) -o $(server_bin) $(cdata_o) $(server_headers) $(server_src)

client: cdata $(client_src)
	gcc $(CFLAGS) $(LDFLAGS) -o $(client_bin) $(cdata_o) $(client_headers) $(client_src)

cdata: $(cdata_src)
	gcc -c $(CFLAGS) $(LDFLAGS) -o $(cdata_o) $(cdata_src)

clean:
	rm -fv $(server_target) $(client_target) src/*.o

.PHONY: all server client clean

