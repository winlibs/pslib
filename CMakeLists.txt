PROJECT(pslib C)
option(ENABLE_BMP   "Build with BMP support" ON)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")

if(ENABLE_BMP)
else(ENABLE_BMP)
	set(DISABLE_BMP 1)
endif(ENABLE_BMP)

if(CMAKE_COMPILER_IS_GNUCC)
  add_definitions(
    -Wall -Wpointer-arith -W
    ${PSLIB_EXTRA_GCC_FLAGS}
  )
endif(CMAKE_COMPILER_IS_GNUCC)

ADD_DEFINITIONS(-DHAVE_CONFIG_H)

SET(PACKAGE pslib)
SET(PACKAGE_NAME pslib)

# set MY_EXTRA_XXX_DIR to the directory where libpng, ligjpeg
# and libungif is located
SET(MY_EXTRA_INCLUDE_DIR f:/php-dev/dev/php_build/include)
SET(MY_EXTRA_LIB_DIR f:/php-dev/dev/php_build/lib)

FIND_PACKAGE(ZLIB)
FIND_PACKAGE(PNG)
FIND_PACKAGE(JPEG)
FIND_PACKAGE(UNGIF)
FIND_PACKAGE(TIFF)

IF(PNG_FOUND)
	INCLUDE_DIRECTORIES(${PNG_INCLUDE_DIR})
	SET(HAVE_LIBPNG 1)
ENDIF(PNG_FOUND)

IF(JPEG_FOUND)
	INCLUDE_DIRECTORIES(${JPEG_INCLUDE_DIR})
	SET(HAVE_LIBJPEG 1)
ENDIF(JPEG_FOUND)

IF(UNGIF_FOUND)
	INCLUDE_DIRECTORIES(${UNGIF_INCLUDE_DIR})
	SET(HAVE_LIBGIF 1)
ENDIF(UNGIF_FOUND)

IF(TIFF_FOUND)
	INCLUDE_DIRECTORIES(${TIFF_INCLUDE_DIR})
	SET(HAVE_LIBTIFF 1)
ENDIF(TIFF_FOUND)

#check system for includes
include(CheckIncludeFile)
check_include_file("fcntl.h"           HAVE_FCNTL_H)
check_include_file("unistd.h"          HAVE_UNISTD_H)
check_include_file("inttypes.h"        HAVE_INTTYPES_H)
check_include_file("stdarg.h"          HAVE_STDARG_H)
check_include_file("stddef.h"          HAVE_STDDEF_H)
check_include_file("stdint.h"          HAVE_STDINT_H)
check_include_file("stdlib.h"          HAVE_STDLIB_H)
check_include_file("string.h"          HAVE_STRING_H)
check_include_file("strings.h"         HAVE_STRINGS_H)
check_include_file("byteswap.h"        HAVE_BYTESWAP_H)
check_include_file("sys/time.h"        HAVE_SYS_TIME_H)
check_include_file("sys/types.h"       HAVE_SYS_TYPES_H)

#Functions
INCLUDE(CheckFunctionExists)

#Big or little endian ?
INCLUDE(TestBigEndian)
test_big_endian(WORDS_BIGENDIAN)

SUBDIRS( src )
SET(SOURCES
	src/pslib.c
	src/ps_memory.c
	src/ps_memprof.c
	src/ps_error.c
	src/ps_afm.c
	src/ps_list.c
	src/ps_util.c
	src/hash_table.c
	src/hash_functions.c
	src/hyphen.c
	src/hnjalloc.c
	src/bmp.c
	src/ps_inputenc.c
	src/ps_fontenc.c
	src/ps_strbuf.c
	src/bmp.h
	src/ght_hash_table.h
	src/hnjalloc.h
	src/hyphen.h
	src/ps_error.h
	src/ps_fontenc.h
	src/ps_inputenc.h
	src/ps_intern.h
	src/ps_list.h
	src/ps_memory.h
	src/ps_strbuf.h
	include/libps/pslib-mp.h
	include/libps/pslib.h
  )

INCLUDE_DIRECTORIES( include . )

#packaging
SET(CPACK_PACKAGE_NAME pslib)
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Create PostScript documents")
SET(CPACK_PACKAGE_VENDOR "pslib")
SET(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_CURRENT_SOURCE_DIR}/README")
SET(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/COPYING")
SET(CPACK_PACKAGE_VERSION_MAJOR "0")
SET(CPACK_PACKAGE_VERSION_MINOR "4")
SET(CPACK_PACKAGE_VERSION_PATCH "6")
SET(CPACK_PACKAGE_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}")

SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "/usr")
SET(CPACK_GENERATOR "TGZ")
SET(CPACK_SOURCE_GENERATOR "TGZ")

CONFIGURE_FILE(${CMAKE_SOURCE_DIR}/cmakeconfig.h.in ${CMAKE_SOURCE_DIR}/config.h)

set(BUILD_SHARED_LIBS On)

if (WIN32)
	SET(PS_LIB pslib)
	ADD_DEFINITIONS(-DWIN32 -D_WIN32 -DMSWIN32 -DBGDWIN32)
	ADD_DEFINITIONS(-D_CRT_SECURE_NO_DEPRECATE)
	ADD_DEFINITIONS(-DPSLIB_EXPORTS)
else(WIN32)
	SET(PS_LIB ps)
endif(WIN32)

ADD_LIBRARY(${PS_LIB} ${SOURCES})

IF(WIN32)
#	SET_TARGET_PROPERTIES(${PS_LIB} PROPERTIES LINK_FLAGS "/NODEFAULTLIB:msvcrt.lib")
ENDIF(WIN32)

TARGET_LINK_LIBRARIES(${PS_LIB} ${PNG_LIBRARIES} ${JPEG_LIBRARIES} ${TIFF_LIBRARIES} ${UNGIF_LIBRARIES})
