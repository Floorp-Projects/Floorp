## Process this file with automake to produce Makefile.in

bin_PROGRAMS = simplebrowser
simplebrowser_SOURCES = \
	    simplebrowser.c

simplebrowser_LDADD = ../../lib/libgtkmozilla.la  @GTK_LIBS@

INCLUDES = \
	-I$(top_srcdir)/lib \
	@DEBUG_FLAGS@ \
	@GTK_CFLAGS@ 

LDFLAGS = --export-dynamic
