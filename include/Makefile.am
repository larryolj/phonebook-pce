
includedir = @includedir@/pce

include_HEADERS = \
	pce.h

MAINTAINERCLEANFILES = Makefile.in

all-local:
	@if [ ! -e pce ] ; then $(LN_S) $(top_srcdir)/include pce ; fi

clean-local:
	@rm -f pce
