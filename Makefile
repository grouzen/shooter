server_target = shooterd
clients_target = shooter_ncurses shooter_sdl
client_ncurses_target = shooter_ncurses
client_sdl_target = shooter_sdl

srcdir = src
server_srcdir = src/server
client_srcdir = src/client

server_objs = $(server_srcdir)/server.o $(server_srcdir)/cdata.o $(server_srcdir)/events.o
client_generic_objs = $(client_srcdir)/client.o $(client_srcdir)/cdata.o
client_ncurses_objs = $(client_srcdir)/ui/ncurses/backend.o
client_sdl_objs = $(client_srcdir)/ui/sdl/backend.o

server_headers = $(srcdir)/cdata.h $(server_srcdir)/events.h $(server_srcdir)/server.h
client_generic_headers = $(srcdir)/cdata.h $(client_srcdir)/ui/backend.h $(client_srcdir)/client.h
client_ncurses_headers =
client_sdl_headers =

LDFLAGS += -pthread
CFLAGS += -Wall -Wextra -g -D_DEBUG_

.PHONY: server clients client_ncurses client_sdl tests test_client clean

server: $(server_objs)
	${CC} -o $(server_target) $(server_objs) $(LDFLAGS) $(CFLAGS)

$(server_srcdir)/%.o: $(server_srcdir)/%.c
	${CC} -D_SERVER_ $(CFLAGS) -c $< -o $@

$(server_srcdir)/cdata.o: $(srcdir)/cdata.c
	${CC} -D_SERVER_ $(CFLAGS) -c $(srcdir)/cdata.c -o $(server_srcdir)/cdata.o

clients: $(clients_target)

client_ncurses: $(client_generic_objs) $(client_ncurses_objs)
	${CC} -o $(client_ncurses_target) $(client_generic_objs) $(client_ncurses_objs) $(LDFLAGS) -lncurses

client_sdl: $(client_generic_objs) $(client_sdl_objs)
	${CC} -o $(client_sdl_target) $(client_generic_objs) $(client_sdl_objs) $(LDFLAGS) $(CFLAGS)

$(client_srcdir)/%.o: $(client_srcdir)/%.c
	${CC} -D_CLIENT_ $(CFLAGS) -c $< -o $@

$(client_srcdir)/cdata.o: $(srcdir)/cdata.c
	${CC} -D_CLIENT_ $(CFLAGS) -c $(srcdir)/cdata.c -o $(client_srcdir)/cdata.o

$(client_srcdir)/ui/ncurses/%.o: $(client_srcdir)/ui/ncurses/%.c
	${CC} -D_CLIENT_ $(CFLAGS) -c $< -o $@

$(client_srcdir)/ui/sdl/%.o: $(client_srcdir)/ui/sdl/%.c
	${CC} -D_CLIENT_ $(CFLAGS) -c $< -o $@

clean:
	rm -fv $(clients_target) $(server_target) $(server_objs) $(client_generic_objs) $(client_ncurses_objs) $(client_sdl_objs)


