bin_PROGRAMS = obexpb

obexpb_SOURCES = obexpb.c

obexpb_LDADD = $(top_builddir)/lib/libpce.la @GLIB_LIBS@ @OPENOBEX_LIBS@ @BLUEZ_LIBS@ @GTK_LIBS@

AM_CFLAGS = @GLIB_CFLAGS@ @OPENOBEX_CFLAGS@ @BLUEZ_CFLAGS@ @GTK_CFLAGS@

INCLUDES = -I$(top_builddir)/include

MAINTAINERCLEANFILES = Makefile.in
