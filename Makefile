
CC = gcc

CFLAGS = -Wall -g -O2 `pkg-config --cflags openobex dbus-1 dbus-glib-1`

PROG = pbclient

OBJS = main.o

LIB = -lbluetooth -lopenobex

all: $(OBJS)
		gcc -o $(PROG) $(OBJS) `pkg-config --libs  dbus-1 dbus-glib-1` $(LIB)

clean:
		rm -f $(PROG) $(OBJS) *.c~ *.h~ 2>/dev/null
