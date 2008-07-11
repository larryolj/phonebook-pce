PKG = `pkg-config --libs --cflags glib-2.0`

SRC = client.c
LIBSRC = libpce.c

PROG = pbclient

OBJS = libpce.so client.o

LIB = -lbluetooth -lopenobex -lglib-2.0

all: $(OBJS)

libpce.so:
		gcc -shared -fPIC -Wall -O2 $(LIBSRC) $(PKG) $(LIB) -o libpce.so

client.o:
		gcc $(SRC) -o $(PROG) ./libpce.so

clean:
		rm -f $(PROG) $(OBJS) *.c~ *.h~ 2>/dev/null
