lib_LTLIBRARIES = libicalss.la

YFLAGS =-d -v -p ss
LFLAGS = -Pss 
LEX_OUTPUT_ROOT = lex.ss

all: icalss.h

INCLUDES =			\
	-I$(top_srcdir)/src	\
	-I$(top_builddir)/src	\
	-I$(top_srcdir)/src/libical	\
	-I$(top_builddir)/src/libical

libicalss_la_LDFLAGS = -version-info 0:0:0

libicalss_la_SOURCES =		\
	icalcalendar.h		\
	icalclassify.c		\
	icalclassify.h		\
	icalgauge.c		\
	icalgauge.h		\
	icalgaugeimpl.h		\
	icaldirset.c		\
	icaldirset.h		\
	icaldirsetimpl.h	\
	icalfileset.c		\
	icalfileset.h		\
	icalfilesetimpl.h	\
	icalset.c		\
	icalset.h		\
	icalsslexer.l		\
	icalssyacc.h		\
	icalssyacc.y		\
	icalspanlist.c		\
	icalspanlist.h		\
	icalmessage.c		\
	icalmessage.h		\
	icalcstpclient.c        \
	icalcstpclient.h        \
	icalcstpserver.c        \
	icalcstpserver.h 	\
	icalcstp.c		\
	icalcstp.h

libicalssincludedir = $(includedir)

COMBINEDHEADERS = \
        $(srcdir)/icalgauge.h \
        $(srcdir)/icalset.h \
        $(srcdir)/icalfileset.h \
        $(srcdir)/icaldirset.h \
        $(srcdir)/icalcalendar.h  \
        $(srcdir)/icalclassify.h \
	$(srcdir)/icalspanlist.h	\
	$(srcdir)/icalmessage.h \
	$(srcdir)/icalcstp.h \
	$(srcdir)/icalcstpclient.h \
	$(srcdir)/icalcstpserver.h


icalss.h: $(COMBINEDHEADERS)
	cat $(COMBINEDHEADERS) | egrep -v "#include.*\"ical" \
	| egrep -v "#include.*\"pvl\.h\"" > icalss.h


libicalssinclude_HEADERS = icalss.h

CONFIG_CLEAN_FILES = y.output

