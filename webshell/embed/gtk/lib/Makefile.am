## Process this file with automake to produce Makefile.in

lib_LTLIBRARIES = libgtkmozilla.la
include_HEADERS = gtkmozilla.h
libgtkmozilla_la_SOURCES = \
			gtkmozilla.cpp \
			gtkmozilla.h \
			GtkMozillaContainer.h \
			GtkMozillaContainer.cpp \
			GtkMozillaInputStream.h \
			GtkMozillaInputStream.cpp \
			nsNewTimer.cpp \
			nsSetupRegistry.cpp

MOZILLA_INCLUDES = -I@MOZILLA_DIR@/include -I@NSPR_DIR@/include

# Mozilla libs:
NULL = 
NSPR_LIBS       = -L@NSPR_DIR@/lib -lplds3 -lplc3 -lnspr3 -lpthread

BASE_LIBS =          \
        -lmozjs \
        -lxpcom      \
        $(NULL)

GECKO_LIBS =\
		-lraptorgfx \
		$(NULL)

JSJ_LIB =
TK_LIBS = 

ALL_MOZ_LIBS =                 \
        $(GECKO_LIBS)  \
        $(BASE_LIBS)   \
        $(JSJ_LIB)     \
        $(NSPR_LIBS)   \
        $(TK_LIBS)     \
        $(NULL)

MOZILLA_LIBS=-L$(MOZILLA_DIR)/lib  \
        $(ALL_MOZ_LIBS)

INCLUDES = $(MOZILLA_INCLUDES) @GTK_CFLAGS@ @DEBUG_FLAGS@

CPPFLAGS = -DXP_UNIX -DMOZ_DLL_SUFFIX=\".so\" -DNECKO -fno-rtti -fno-exceptions

libgtkmozilla_la_LIBADD = $(MOZILLA_LIBS) @GTK_LIBS@
