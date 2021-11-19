# - Find the native UNGIF includes and library
#

# This module defines
#  UNGIF_INCLUDE_DIR, where to find gif.h, etc.
#  UNGIF_LIBRARIES, the libraries to link against to use UNGIF.
#  UNGIF_DEFINITIONS - You should ADD_DEFINITONS(${UNGIF_DEFINITIONS}) before compiling code that includes ungif library files.
#  UNGIF_FOUND, If false, do not try to use UNGIF.
# also defined, but not for general use are
#  UNGIF_LIBRARY, where to find the UNGIF library.
# None of the above will be defined unles zlib can be found.
# UNGIF depends on Zlib
INCLUDE(FindZLIB)

SET(UNGIF_FOUND "NO")

IF(ZLIB_FOUND)
  FIND_PATH(UNGIF_UNGIF_INCLUDE_DIR gif_lib.h
  ${MY_EXTRA_INCLUDE_DIR}
  /usr/local/include
  /usr/include
  )

  SET(UNGIF_NAMES ${UNGIF_NAMES} ungif libungif)
  FIND_LIBRARY(UNGIF_LIBRARY
    NAMES ${UNGIF_NAMES}
    PATHS ${MY_EXTRA_LIB_DIR} /usr/lib /usr/local/lib
  )

  IF (UNGIF_LIBRARY AND UNGIF_UNGIF_INCLUDE_DIR)
      # gif.h includes zlib.h. Sigh.
      SET(UNGIF_INCLUDE_DIR ${UNGIF_UNGIF_INCLUDE_DIR} ${ZLIB_INCLUDE_DIR} )
      SET(UNGIF_LIBRARIES ${UNGIF_LIBRARY} ${ZLIB_LIBRARY})
      SET(UNGIF_FOUND "YES")

      IF (CYGWIN)
        IF(BUILD_SHARED_LIBS)
           # No need to define UNGIF_USE_DLL here, because it's default for Cygwin.
        ELSE(BUILD_SHARED_LIBS)
          SET (UNGIF_DEFINITIONS -DPNG_STATIC)
        ENDIF(BUILD_SHARED_LIBS)
      ENDIF (CYGWIN)

  ENDIF (UNGIF_LIBRARY AND UNGIF_UNGIF_INCLUDE_DIR)

ENDIF(ZLIB_FOUND)

IF (UNGIF_FOUND)
  IF (NOT UNGIF_FIND_QUIETLY)
    MESSAGE(STATUS "Found UNGIF: ${UNGIF_LIBRARY}")
  ENDIF (NOT UNGIF_FIND_QUIETLY)
ELSE (UNGIF_FOUND)
  IF (UNGIF_FIND_REQUIRED)
    MESSAGE(FATAL_ERROR "Could not find UNGIF library")
  ENDIF (UNGIF_FIND_REQUIRED)
ENDIF (UNGIF_FOUND)

MARK_AS_ADVANCED(UNGIF_UNGIF_INCLUDE_DIR UNGIF_LIBRARY )
