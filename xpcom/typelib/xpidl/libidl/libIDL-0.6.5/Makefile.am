# -*- mode: makefile -*-
#
# libIDL Makefile.am
#
INCLUDES		= -DYYDEBUG=1 -DYYERROR_VERBOSE=1 \
			  -DIDL_LIBRARY @GLIB_CFLAGS@ \
			  -DG_LOG_DOMAIN=\"libIDL\"
YFLAGS			= -d -v 2>/dev/null
LFLAGS			= @LFLAGS@
LIBS			= @GLIB_LIBS@

lexer.c: $(srcdir)/lexer.l
	@rm -f $@
	$(LEX) $(LFLAGS) -t $< | sed -e 's/yy/__IDL_/g' >$@

stamp-parser: $(srcdir)/parser.y
	$(YACC) $(YFLAGS) $<
	touch $@

parser.c: stamp-parser
	test -f y.tab.c && mv -f y.tab.c parser.c || touch $@

parser.h: stamp-parser
	test -f y.tab.h && mv -f y.tab.h parser.h || touch $@

IDL.h: IDL.h.new
	@:

MOSTLYCLEANFILES	= parser.output y.output
DISTCLEANFILES		= IDL.h

EXTRA_DIST		= BUGS parser.y lexer.l \
			  Makefile.msc README.win32 \
			  libIDL.def libIDLConf.sh.in \
			  stamp-parser

info_TEXINFOS		= libIDL.texi
pkginclude_HEADERS	= IDL.h
lib_LTLIBRARIES		= libIDL.la
noinst_PROGRAMS		= tstidl

bin_SCRIPTS		= libIDL-config

BUILT_SOURCES		= lexer.c parser.c parser.h IDL.h stamp-parser

libIDL_la_SOURCES	= parser.c parser.h lexer.c \
			  ns.c util.c util.h rename.h
libIDL_la_LDFLAGS	= -version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
			  -release $(LT_RELEASE)

m4datadir		= $(datadir)/aclocal
m4data_DATA		= libIDL.m4

tstidl_SOURCES		= tstidl.c
tstidl_LDFLAGS		= -static
tstidl_LDADD		= libIDL.la

confexecdir		= $(libdir)
confexec_DATA		= libIDLConf.sh

## We create libIDLConf.sh here and not from configure because we want
## to get the paths expanded correctly.  Macros like srcdir are given
## the value NONE in configure if the user doesn't specify them (this
## is an autoconf feature, not a bug).

libIDLConf.sh: libIDLConf.sh.in Makefile
## Use sed and then mv to avoid problems if the user interrupts.
	sed -e 's?\@LIBIDL_LIBDIR\@?$(LIBIDL_LIBDIR)?g' \
		-e 's?\@LIBIDL_INCLUDEDIR\@?$(LIBIDL_INCLUDEDIR)?g' \
		-e 's?\@LIBIDL_LIBS\@?$(LIBIDL_LIBS)?g' \
		< $(srcdir)/libIDLConf.sh.in > libIDLConf.tmp \
		&& mv libIDLConf.tmp libIDLConf.sh
