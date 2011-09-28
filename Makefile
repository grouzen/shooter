server_target = shooterd
clients_target = shooter_ncurses shooter_sdl
client_ncurses_target = shooter_ncurses
client_sdl_target = shooter_sdl

srcdir = src
server_srcdir = src/server
client_srcdir = src/client

server_objs = $(server_srcdir)/server.o $(server_srcdir)/cdata.o
client_generic_objs = $(client_srcdir)/client.o $(client_srcdir)/cdata.o
client_ncurses_objs = $(client_srcdir)/ui/ncurses/backend.o
client_sdl_objs = $(client_srcdir)/ui/sdl/backend.o

server_headers = $(srcdir)/cdata.h
client_generic_headers = $(srcdir)/cdata.h $(client_srcdir)/ui/backend.h $(client_srcdir)/client.h
client_ncurses_headers = 
client_sdl_headers = 

# Tests
#tests_target = test_client
#test_client_target = test_client

#test_generic_objs = src/server/cdata.o src/client/cdata.o
#test_client_objs = src/test/test_client.o src/client/ui/ncurses/backend.o
#test_generic_headers = src/cdata.h
#test_client_headers = src/client/ui/backend.h src/client/client.h

LDFLAGS += -pthread
CFLAGS += -Wall -Wextra -g

.PHONY: server clients client_ncurses client_sdl tests test_client clean

server: $(server_objs)
	gcc -o $(server_target) $(server_objs) $(LDFLAGS) $(CFLAGS)

$(server_srcdir)/%.o: $(server_srcdir)/%.c
	gcc -D_SERVER_ $(CFLAGS) -c $< -o $@

$(server_srcdir)/cdata.o: $(srcdir)/cdata.c
	gcc -D_SERVER_ $(CFLAGS) -c $(srcdir)/cdata.c -o $(server_srcdir)/cdata.o

clients: $(clients_target)

client_ncurses: $(client_generic_objs) $(client_ncurses_objs)
	gcc -o $(client_ncurses_target) $(client_generic_objs) $(client_ncurses_objs) $(LDFLAGS) -lncurses

client_sdl: $(client_generic_objs) $(client_sdl_objs)
	gcc -o $(client_sdl_target) $(client_generic_objs) $(client_sdl_objs) $(LDFLAGS) $(CFLAGS)

$(client_srcdir)/%.o: $(client_srcdir)/%.c
	gcc -D_CLIENT_ $(CFLAGS) -c $< -o $@

$(client_srcdir)/cdata.o: $(srcdir)/cdata.c
	gcc -D_CLIENT_ $(CFLAGS) -c $(srcdir)/cdata.c -o $(client_srcdir)/cdata.o

$(client_srcdir)/ui/ncurses/%.o: $(client_srcdir)/ui/ncurses/%.c
	gcc -D_CLIENT_ $(CFLAGS) -c $< -o $@

$(client_srcdir)/ui/sdl/%.o: $(client_srcdir)/ui/sdl/%.c
	gcc -D_CLIENT_ $(CFLAGS) -c $< -o $@

#tests: $(tests_target)

#test_client: $(test_generic_objs) $(test_client_objs)
#	gcc -DUI_BACKEND_NCURSES -o $(test_client_target) $(test_generic_objs) $(test_client_objs) $(LDFLAGS) -lncurses $(CFLAGS)

#$(test_generic_objs): $(test_generic_headers) $(test_client_headers)

#$(test_client_objs): $(test_generic_headers) $(test_client_headers)

clean:
	rm -fv $(clients_target) $(server_target) $(server_objs) $(client_generic_objs) $(client_ncurses_objs) $(client_sdl_objs)


