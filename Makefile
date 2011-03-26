server_target = server
client_target = client
server_objs = src/server.c src/cdata.c
client_objs = src/client.c src/cdata.c
server_headers = src/cdata.h
client_headers = src/cdata.h

LDFLAGS += -pthread
CFLAGS += -Wall -Wextra -g

.PHONY: server client clean
server: $(server_objs)
	gcc -o $(server_target) $(server_objs) $(LDFLAGS) $(CFLAGS)

client: $(client_objs)
	gcc -o $(client_target) $(client_objs) $(LDFLAGS) $(CFLAGS)

$(server_objs): $(server_headers)

$(client_objs): $(client_headers)

clean:
	rm -fv $(server_target) $(client_target) src/*.o


