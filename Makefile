all: chat

CFLAGS = `pkg-config --cflags --libs glib-2.0`
SOURCEFILES = unicast.c mcast.c chat.c
HEADERS = mp1.h

chat: $(SOURCEFILES) $(HEADERS)
	gcc ${CFLAGS} -g -pthread -o $@ $(SOURCEFILES)

clean:	restart
	-rm -f chat *.o *~

restart:
	-rm -f GROUPLIST
