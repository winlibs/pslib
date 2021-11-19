## Process this file with automake to produce Makefile.in

INCLUDES = -I@srcdir@/../include -I./include $(LIBHNJ_CFLAGS) $(PNG_INCLUDEDIR) $(JPEG_INCLUDEDIR) $(GIF_INCLUDEDIR) $(TIFF_INCLUDEDIR) -DPACKAGE_LOCALE_DIR=\""$(datadir)/locale"\" -DPACKAGE_DATA_DIR=\""$(pkgdatadir)"\"

noinst_HEADERS = \
	ps_list.h \
	ps_intern.h \
	ps_memory.h \
	ps_error.h \
	ght_hash_table.h \
	hyphen.h \
	hnjalloc.h \
	bmp.h \
	ps_inputenc.h \
	ps_fontenc.h \
	ps_strbuf.h

lib_LTLIBRARIES = libps.la

libps_la_LDFLAGS = -version-info @LIBPS_VERSION_INFO@ $(LIBHNJ_LIBS) $(PNG_LIBDIR) $(JPEG_LIBDIR) $(GIF_LIBDIR) $(TIFF_LIBDIR)

libps_la_SOURCES = \
	pslib.c \
	ps_memory.c \
	ps_memprof.c \
	ps_error.c \
	ps_afm.c \
	ps_list.c \
	ps_util.c \
	hash_table.c \
	hash_functions.c \
	hyphen.c \
	hnjalloc.c \
	bmp.c \
	ps_inputenc.c \
	ps_fontenc.c \
	ps_strbuf.c

libps_la_LIBADD = $(LIBHNJ_LIBS) $(PNG_LIBS) $(JPEG_LIBS) $(GIF_LIBS) $(TIFF_LIBS)

BUILD_LIBS = -lm

#clean-am:
#	rm -rf CMakeFiles cmake_install.cmake
