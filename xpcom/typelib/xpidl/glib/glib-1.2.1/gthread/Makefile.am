## Process this file with automake to produce Makefile.in

INCLUDES = -I$(top_srcdir) -I$(top_srcdir)/gthread -DG_LOG_DOMAIN=\"GThread\"

EXTRA_DIST =				\
		gthread-posix.c		\
		gthread-solaris.c	\
		gthread-nspr.c		\
		gthread-none.c		\
		gthread.def

libglib = $(top_builddir)/libglib.la # -lglib

lib_LTLIBRARIES = libgthread.la

libgthread_la_SOURCES = gthread.c
libgthread_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-release $(LT_RELEASE)

libgthread_la_LIBADD = @G_THREAD_LIBS@

noinst_PROGRAMS = testgthread
testgthread_LDADD = ../libglib.la libgthread.la 
