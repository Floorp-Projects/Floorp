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

TIMER_LIBS -ltimer_gtk_s

BASE_LIBS =          \
        -lpref       \
        $(ZLIB_LIBS) \
        -lmozreg        \
        -l$(MOZ_LIB_JS_PREFIX)js             \
		$(TIMER_LIBS) \
        -lxpcom      \
        -lsecfree    \
        -l$(MOZ_LIB_UTIL_PREFIX)util    \
        $(NULL)

MOZ_WIDGET_TOOLKIT_LDFLAGS=-lwidget_gtk

GECKO_LIBS =                  \
        -lraptorwebwidget     \
		$(MOZ_WIDGET_TOOLKIT_LDFLAGS) \
		-lraptorgfx 	\
        -lraptorhtmlpars      \
        -lexpat \
        -lxmltok \
        -ljsdom               \
        $(NULL)

JPEG_LIBS=-ljpeg
PNG_LIBS=-lpng

IMGLIB_LIBS =        \
        -limg        \
        $(JPEG_LIBS) \
        $(PNG_LIBS)  \
        $(NULL)
MOZ_LIB_DBM_PREFIX=moz

NETLIB_LIBS =         \
        -labouturl    \
        -lfileurl     \
        -lftpurl      \
        -lgophurl     \
        -lhttpurl     \
        -lremoturl    \
        -lsockstuburl \
        -lmimetype    \
        -lnetcache    \
        -lnetcnvts    \
        -lnetlib      \
        -lnetwork     \
        -lxp          \
        -ljsurl       \
        -l$(MOZ_LIB_DBM_PREFIX)dbm      \
        -lpwcac       \
        $(NULL)

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

CPPFLAGS = -DXP_UNIX -DMOZ_DLL_SUFFIX=\".so\"

libgtkmozilla_la_LIBADD = $(MOZILLA_LIBS) @GTK_LIBS@


