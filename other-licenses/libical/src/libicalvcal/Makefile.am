lib_LTLIBRARIES = libicalvcal.la

INCLUDES =			\
	-I/usr/include/objc	\
	-I$(top_builddir)	\
	-I$(top_srcdir)/src/libical	\
	-I$(top_builddir)/src/libical	\
	-I$(top_srcdir)/src/libicalss

libicalvcal_la_LDFLAGS = -version-info 0:0:0

libicalvcal_la_SOURCES =	\
	icalvcal.c		\
	vcc.y			\
	vobject.c		\
	vcaltmp.c

libicalvcalincludedir = $(includedir)/libicalvcal

libicalvcalinclude_HEADERS =	\
	icalvcal.h		\
	port.h			\
	vcc.h			\
	vobject.h		\
	vcaltmp.h

EXTRA_DIST =		\
	README.TXT	\
	vcaltest.c	\
	vctest.c
