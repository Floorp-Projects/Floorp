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
			nsSetupRegistry.cpp

MOZILLA_INCLUDES = -I@MOZILLA_DIR@/dist/include

# Mozilla libs:
NULL = 
NSPR_LIBS       = -lplds3 -lplc3 -lnspr3 -lpthread

MOZ_LIB_JS_PREFIX=moz
MOZ_LIB_UTIL_PREFIX=moz

ZLIB_LIBS = -lz

TIMER_LIBS = -ltimer_s

BASE_LIBS =          \
        -lmozreg        \
        -lmozjs \
		$(TIMER_LIBS) \
        -lxpcom      \
        -lmozutil \
        $(NULL)

GECKO_LIBS =\
		-lraptorgfx \
		-ljsdom \
		-ljsurl \
		$(NULL)

IMGLIB_LIBS =        \
        -limg        \
        $(NULL)

NETLIB_LIBS=-lneckoutil_s

JSJ_LIB =
TK_LIBS = 

ALL_MOZ_LIBS =                 \
        $(GECKO_LIBS)  \
        $(NETLIB_LIBS) \
        $(IMGLIB_LIBS) \
        $(BASE_LIBS)   \
        $(JSJ_LIB)     \
        $(NSPR_LIBS)   \
        $(TK_LIBS)     \
        $(NULL)

MOZILLA_LIBS=-L$(MOZILLA_DIR)/dist/lib  \
        $(ALL_MOZ_LIBS)

INCLUDES = $(MOZILLA_INCLUDES) @GTK_CFLAGS@ @DEBUG_FLAGS@

CPPFLAGS = -DXP_UNIX -DMOZ_DLL_SUFFIX=\".so\" -DNECKO

libgtkmozilla_la_LIBADD = $(MOZILLA_LIBS) @GTK_LIBS@


