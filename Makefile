server_target = server
clients_target = client_ncurses client_sdl
client_ncurses_target = client_ncurses
client_sdl_target = client_sdl

server_objs = src/server.o src/cdata.o
client_generic_objs = src/client.o src/cdata.o
client_ncurses_objs = src/ui/ncurses/backend.o
client_sdl_objs = src/ui/sdl/backend.o

server_headers = src/cdata.h
client_generic_headers = src/cdata.h
client_ncurses_headers = src/ui/ncurses/backend.h
client_sdl_headers = src/ui/sdl/backend.h

# Tests
tests_target = test_client_connect
test_client_connect_target = test_client_connect

test_generic_objs = src/cdata.o
test_client_connect_objs = src/test/test_client_connect.o src/ui/ncurses/backend.o
test_generic_headers = src/cdata.h
test_client_connect_headers = src/ui/ncurses/backend.h

LDFLAGS += -pthread
CFLAGS += -Wall -Wextra -g

.PHONY: server client clean
server: $(server_objs)
	gcc -o $(server_target) $(server_objs) $(LDFLAGS) $(CFLAGS)

clients: $(clients_target)

client_ncurses: $(client_generic_objs) $(client_ncurses_objs)
	gcc -DUI_BACKEND_NCURSES -o $(client_ncurses_target) $(client_generic_objs) $(client_ncurses_objs) $(LDFLAGS) $(CFLAGS)

client_sdl: $(client_generic_objs) $(client_sdl_objs)
	gcc -DUI_BACKEND_SDL -o $(client_sdl_target) $(client_generic_objs) $(client_sdl_objs) $(LDFLAGS) $(CFLAGS)

$(server_objs): $(server_headers)

$(client_generic_objs): $(client_generic_headers) $(client_ncurses_headers) $(client_sdl_headers)

$(client_ncurses_objs): $(client_generic_headers) $(client_ncurses_headers)

$(client_sdl_objs): $(client_generic_headers) $(client_sdl_headers)

tests: $(tests_target)

test_client_connect: $(test_generic_objs) $(test_client_connect_objs)
	gcc -DUI_BACKEND_NCURSES -o $(test_client_connect_target) $(test_generic_objs) $(test_client_connect_objs) $(LDFLAGS) $(CFLAGS)

$(test_generic_objs): $(test_generic_headers) $(test_client_connect_headers)

$(test_client_connect_objs): $(test_generic_headers) $(test_client_connect_headers)

clean:
	rm -fv $(server_target) $(clients_target) $(tests_target) src/*.o src/ui/*/*.o src/test/*.o


