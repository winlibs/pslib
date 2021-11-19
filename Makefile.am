## Process this file with automake to produce Makefile.in

SUBDIRS = data include src po $(DOCDIR)

spec = $(PACKAGE).spec

EXTRA_DIST = $(spec) $(spec).in autogen.sh libps.pc.in intltool-extract.in intltool-merge.in intltool-update.in README.WIN32 CMakeLists.txt cmakeconfig.h.in cmake

#all-local: $(spec)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libps.pc

rpm: $(distdir).tar.gz
	rpm -ta $(distdir).tar.gz

clean-am:
	rm -rf CMakeFiles cmake_install.cmake
