server_bin=server
client_bin=client
cdata_o=cdata.o
cdata_src=src/cdata.c
server_src=src/server.c
client_src=src/client.c
server_headers=src/cdata.h
client_headers=src/cdata.h

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
	rm -fv $(server_bin) $(client_bin) $(cdata_o)

.PHONY: all server client clean

