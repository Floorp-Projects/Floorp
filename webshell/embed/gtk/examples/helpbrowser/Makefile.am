## Process this file with automake to produce Makefile.in

SUBDIRS = help_data

bin_PROGRAMS = helpbrowser
helpbrowser_SOURCES = \
	    helpbrowser.c

helpbrowser_LDADD = ../../lib/libgtkmozilla.la  @GTK_LIBS@

INCLUDES = \
	-I$(top_srcdir)/lib \
	@DEBUG_FLAGS@ \
	@GTK_CFLAGS@ 

HELPDATA = -DHELPDATADIR=\"`cd "$(srcdir)/help_data" && pwd`\"

CPPFLAGS = $(HELPDATA)

LDFLAGS = --export-dynamic
