
CFLAGS = -Wall -g -O2 `pkg-config --cflags openobex dbus-1 dbus-glib-1`

PKG = `pkg-config --libs --cflags glib-2.0`

SRC = main.c

PROG = pbcliet

OBJS = main.o client.o

LIB = -lbluetooth -lopenobex -lglib-2.0

all:
		gcc -shared -fPIC -Wall -O2 libpce.c $(PKG) $(LIB) -o libpce.so
		gcc $(SRC) -o $(PROG) $(PKG) $(LIB)
		gcc client.c -o pce_client $(PKG) $(LIB) ./libpce.so

clean:
		rm -f $(PROG) $(OBJS) *.c~ *.h~ 2>/dev/null
