## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS = 1.4

SUBDIRS = . gmodule gthread docs tests

configincludedir = $(pkglibdir)/include

bin_SCRIPTS=glib-config
BUILT_SOURCES=glib-config
glib-config: glib-config.in

INCLUDES = -DG_LOG_DOMAIN=g_log_domain_glib @GLIB_DEBUG_FLAGS@

EXTRA_DIST = \
	glib.m4			\
	glib.spec.in		\
	acglib.m4		\
	sanity_check		\
	README.win32		\
	glib.def		\
	makefile.msc		\
	giowin32.c		\
	glibconfig.h.win32.in	\
	config.h.win32

lib_LTLIBRARIES = libglib.la

libglib_la_SOURCES = \
	garray.c	\
	gcache.c	\
	gcompletion.c	\
	gdataset.c	\
	gdate.c         \
	gerror.c	\
	ghash.c		\
	ghook.c		\
	giochannel.c    \
	giounix.c       \
	glist.c		\
	gmain.c         \
	gmem.c		\
	gmessages.c	\
	gmutex.c	\
	gnode.c		\
	gprimes.c	\
	grel.c		\
	gscanner.c	\
	gslist.c	\
	gstrfuncs.c	\
	gstring.c	\
	gtimer.c	\
	gtree.c		\
	gutils.c

include_HEADERS = \
	glib.h

configinclude_DATA = \
	glibconfig.h

configure: configure.in acglib.m4		

BUILT_SOURCES = stamp-gc-h #note: not glibconfig.h
glibconfig.h: stamp-gc-h
	@:
stamp-gc-h: config.status
	CONFIG_FILES= CONFIG_HEADERS= CONFIG_OTHER=glibconfig.h ./config.status
	echo timestamp > stamp-gc-h

libglib_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE)

noinst_PROGRAMS = testglib testgdate testgdateparser
testglib_LDADD = libglib.la
testgdate_LDADD = libglib.la
testgdateparser_LDADD = libglib.la

m4datadir = $(datadir)/aclocal
m4data_DATA = glib.m4

.PHONY: files release sanity snapshot

files:
	@files=`ls $(DISTFILES) 2> /dev/null `; for p in $$files; do \
	  echo $$p; \
	done

release:
	rm -rf .deps */.deps
	cd docs && make glib.html
	$(MAKE) distcheck

sanity:
	./sanity_check $(VERSION)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)`date +"%y%m%d"`

dist-hook:
	cp glib.spec $(distdir)
