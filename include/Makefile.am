## Process this file with automake to produce Makefile.in

pslibincdir = $(includedir)/libps

pslibinc_HEADERS = \
	libps/pslib.h \
	libps/pslib-mp.h \
	libps/psversion.h

install-exec-hook:
	$(mkinstalldirs) $(DESTDIR)$(pslibincdir)

EXTRA_DIST = libps/psversion.h.in
