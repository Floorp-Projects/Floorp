# Makefile for mailstone
# This should be run using 'gmake'

########################################################################

PRODUCT = mailstone

ARCH := $(shell uname -s)

# These normally get overridden on the command line from ../Makefile
BUILD_VARIANT=debug
ifeq ($(BUILD_VARIANT), debug)
	BUILD_TYPE = DEBUG
	OBJDIR_TAG = _DBG
else
	BUILD_TYPE = RELEASE
	OBJDIR_TAG = _OPT
endif

ifeq ("$(OBJDIR)", "")
ifeq ($(ARCH), WINNT)
	NSARCH := WIN32
else
	NSARCH := $(shell ../nsarch)
endif
	OBJDIR = ../build/$(NSARCH)$(OBJDIR_TAG).OBJ
	PKGDIR = ../build/package/$(NSARCH)$(OBJDIR_TAG).OBJ
endif


########################################################################

# setup OS specific compilers and options

CC		= cc
AR		= ar
INCLUDES	= -I$(OBJDIR)
REL_OS_CFLAGS	= -O
REL_OS_LFLAGS	=
DBG_OS_CFLAGS	= -g -D_DEBUG
DBG_OS_LFLAGS	=
LIBS		= -lm
OBJ_SUFFIX	= o
LIB_SUFFIX	= a
EXE_SUFFIX	=
LIBGD_DIR	= gd1.3
GNUPLOT_DIR	= gnuplot-3.7
PERL_DIR	= perl5.005_03
PERL_REV	= 5.00503
PERL5_IMPORT	= /share/builds/components/perl5
ECHO		= /bin/echo

ifeq ($(ARCH), WINNT)
	CC	= cl
	OSDEFS	= -DWIN32 -D_WIN32
	LIBS	= wsock32.lib libcmt.lib msvcrt.lib
	REL_OS_CFLAGS =
	REL_OS_LINKFLAGS = /link
	DBG_OS_CFLAGS = -Od -Zi
	DBG_OS_LINKFLAGS = /link /debug:full
	OBJ_SUFFIX = obj
	LIB_SUFFIX = .lib
	DLL_SUFFIX = .dll
	EXE_SUFFIX = .exe
	PERL_OS = MSWin32-x86
	# build perl manually, install to c:\perl.  Then build everything else
	#  cd win32 && nmake && nmake install
	PERL5_IMPORT	= c:/perl/$(PERL_REV)/
	PERL_FILES = $(PERL_DIR)/Artistic
	PERL_BIN_FILES = \
	  $(PERL5_IMPORT)/bin/$(PERL_OS)/perl$(EXE_SUFFIX)
	  $(PERL5_IMPORT)/bin/$(PERL_OS)/perl$(DLL_SUFFIX) \
	PERL_LIB_FILES = $(PERL5_IMPORT)/lib/*.pm $(PERL5_IMPORT)/lib/*.pl
	PERL_LIB_OS_FILES = $(PERL5_IMPORT)/lib/$(PERL_OS)/*.pm
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/WINNT-perl5/perl.exe \
#	  $(PERL5_IMPORT)/WINNT-perl5/perl300.dll
endif
ifeq ($(ARCH), IRIX64)
	ARCH	= IRIX
endif
ifeq ($(ARCH), IRIX)
	# MIPSpro Compilers: Version 7.2.1
	CC	= /usr/bin/cc -n32
	REL_OS_CFLAGS = -fullwarn
	DBG_OS_CFLAGS = -fullwarn
	OSDEFS	= -D__IRIX__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/IRIX-perl5/perl5.005_02/perl
	LIBS	= -lm -lpthread
	# OS specific flags for perl Configure
	PERL_OS_CONFIGURE = -Dnm=/usr/bin/nm -Dar=/usr/bin/ar
	PERL_OS = IP27-irix
endif
ifeq ($(ARCH), OSF1)
	# DEC C V5.6-071 on Digital UNIX V4.0(D) (Rev. 878)
	CC	= /usr/bin/cc
	REL_OS_CFLAGS = -warnprotos -verbose -newc -std1 -pthread -w0 -readonly_strings
	DBG_OS_CFLAGS = -warnprotos -verbose -newc -std1 -pthread -w0 -readonly_strings
	OSDEFS	= -D__OSF1__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48_R
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/OSF1-perl5/perl5.005_2/perl
	LIBS	= -lm -lpthread
	PERL_OS = alpha-dec_osf
endif
ifeq ($(ARCH), AIX)
	CC	= /usr/bin/xlc_r
	REL_OS_CFLAGS = -qro -qroconst -qfullpath -qsrcmsg #-qflag=I:W
	DBG_OS_CFLAGS = -qro -qroconst -g -qfullpath -qsrcmsg #-qflag=I:W
	OSDEFS	= -D__AIX__ -DHAVE_SELECT_H -D_THREAD_SAFE -DUSE_PTHREADS -DUSE_LRAND48_R
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/AIX-perl5/perl5.005_2/perl
	LIBS	= -lm #-lpthread
	PERL_OS = aix
endif
ifeq ($(ARCH), HP-UX)
	CC	= /usr/bin/cc
	# old flags: -Ae +DA1.0 +ESlit
	REL_OS_CFLAGS = +DAportable +DS2.0 -Ae +ESlit
	DBG_OS_CFLAGS = +Z +DAportable +DS2.0 -g -Ae +ESlit
	OSDEFS	= -D__HPUX__ -DUSE_PTHREADS -DUSE_LRAND48
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/HPUX-perl5/perl5.005_02/perl
	LIBS	= -lm -lpthread
	PERL_OS = PA-RISC2.0
endif
ifeq ($(ARCH), SunOS)
	# Sun Workshop Compilers 5.0
	CC	= /tools/ns/workshop-5.0/bin/cc
	REL_OS_CFLAGS = -mt -xstrconst -v -O
	DBG_OS_CFLAGS = -mt -xstrconst -v -g -xs
	OSDEFS	= -D__SOLARIS__ -DHAVE_SELECT_H -DHAVE_WAIT_H \
	 -DXP_UNIX -D_REENTRANT \
	 -DUSE_PTHREADS -DUSE_GETHOSTBYNAME_R -DUSE_GETPROTOBYNAME_R -DUSE_LRAND48
	LIBS    = -lm -lnsl -lsocket -lpthread
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/SOLARIS-perl5/perl5.005_02/perl
	PERL_OS = sun4-solaris
endif
ifeq ($(ARCH), Linux)
	# Linux 2.1 kernels and above
	CC	= /usr/bin/gcc   # gcc 2.7.2.3
	REL_OS_CFLAGS = -O -g -Wall
	DBG_OS_CFLAGS = -O1 -g -Wall
	OSDEFS	= -D__LINUX__ -DHAVE_SELECT_H -DHAVE_WAIT_H -DUSE_PTHREADS -DUSE_LRAND48
#	PERL_FILES = $(PERL5_IMPORT)/artistic $(PERL5_IMPORT)/Linux2.1-perl5/perl
	LIBS	= -lm -lpthread
	# Must explicitly enable interpretation of \n
	# works for /bin/echo, sh:echo, or pdksh:echo. NOT tcsh:echo
	ECHO	= /bin/echo -e
	PERL_OS = i686-linux
endif

ifeq ($(BUILD_TYPE), DEBUG)
	OS_CFLAGS = $(DBG_OS_CFLAGS) -D_DEBUG
	OS_LINKFLAGS = $(DBG_OS_LINKFLAGS)
else
	OS_CFLAGS = $(REL_OS_CFLAGS)
	OS_LINKFLAGS = $(REL_OS_CFLAGS)
endif

CPPFLAGS	= 
CFLAGS		= $(OS_CFLAGS)
###DEFINES		= -DHAVE_CONFIG_H $(OSDEFS)
DEFINES		= $(OSDEFS)
LDFLAGS         =

BINDIR		= ../bin
CP		= cp
RM		= rm -f

COMPILE         = $(CC) $(CFLAGS) $(DEFINES) $(CPPFLAGS) $(INCLUDES)

GENRANDOBJS	= genrand.$(OBJ_SUFFIX)

########################################################################

# what are we making

###all:: $(OBJDIR) libgd gnuplot mailclient
all:: $(OBJDIR) mailclient

########################################################################

.SUFFIXES: .c .$(OBJ_SUFFIX)

.SUFFIXES: .c .$(OBJ_SUFFIX)

.c.$(OBJ_SUFFIX):
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(OBJDIR)/$@
else
	$(COMPILE) -c $< -o $(OBJDIR)/$@
endif

$(OBJDIR)/%.$(OBJ_SUFFIX): %.c
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(OBJDIR)/$*.$(OBJ_SUFFIX)
else	
	$(COMPILE) -c $< -o $(OBJDIR)/$*.$(OBJ_SUFFIX)
endif

$(OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(OBJDIR)
else
	[ -d $(OBJDIR) ] || mkdir -p $(OBJDIR)
endif

########################################################################

# optional component libgd

ifneq ("$(LIBGD_DIR)", "")
LIBGD_OBJDIR	= $(OBJDIR)/gd
LIBGD		= $(LIBGD_OBJDIR)/libgd.$(LIB_SUFFIX)
GDDEMO		= $(LIBGD_OBJDIR)/gddemo$(EXE_SUFFIX)
GIFTOGD		= $(LIBGD_OBJDIR)/giftogd$(EXE_SUFFIX)
WEBGIF		= $(LIBGD_OBJDIR)/webgif$(EXE_SUFFIX)

#LIBPATH	+= -L$(LIBGD_OBJDIR)
#INCLUDES	+= -I./$(LIBGD_DIR)

LIBGD_SRCS	= gd.c gdfontt.c gdfonts.c gdfontmb.c gdfontl.c gdfontg.c
LIBGD_SRCS2	= $(addprefix $(LIBGD_DIR)/, $(LIBGD_SRCS))

LIBGD_OBJS	= $(addprefix $(LIBGD_OBJDIR)/, $(LIBGD_SRCS:.c=.$(OBJ_SUFFIX)))

LIBGD_ALL	= $(LIBGD_OBJDIR) $(LIBGD) $(GDDEMO) $(GIFTOGD) $(WEBGIF)

libgd:: $(LIBGD_ALL)

$(LIBGD_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(LIBGD_OBJDIR)
else
	[ -d $(LIBGD_OBJDIR) ] || mkdir -p $(LIBGD_OBJDIR)
endif

$(LIBGD): $(LIBGD_OBJS)
	@$(ECHO) "\n===== [`date`] making libgd...\n"
	$(AR) rc $(LIBGD) $(LIBGD_OBJS)
	cp $(LIBGD_DIR)/gd.h $(LIBGD_OBJDIR)
	cp $(LIBGD_DIR)/demoin.gif $(LIBGD_OBJDIR)
	cp $(LIBGD_DIR)/readme.txt $(LIBGD_OBJDIR)/gd.txt
	cp $(LIBGD_DIR)/index.html $(LIBGD_OBJDIR)/gd.html

$(GDDEMO): $(LIBGD) $(LIBGD_OBJDIR)/gddemo.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/gddemo.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(GDDEMO)

$(GIFTOGD): $(LIBGD) $(LIBGD_OBJDIR)/giftogd.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/giftogd.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(GIFTOGD)

$(WEBGIF): $(LIBGD) $(LIBGD_OBJDIR)/webgif.$(OBJ_SUFFIX)
	$(COMPILE) $(LIBGD_OBJDIR)/webgif.$(OBJ_SUFFIX) $(LIBPATH) $(LIBGD) $(LIBS) $(OS_LINKFLAGS) -o $(WEBGIF)

distclean::
	$(RM) $(LIBGD) $(LIBGD_OBJS)

$(LIBGD_OBJDIR)/%.$(OBJ_SUFFIX): $(LIBGD_DIR)/%.c
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(LIBGD_OBJDIR)/$*.$(OBJ_SUFFIX)
else	
	$(COMPILE) -c $< -o $(LIBGD_OBJDIR)/$*.$(OBJ_SUFFIX)
endif

endif

########################################################################

# optional component gnuplot (can use libgd)

ifneq ("$(GNUPLOT_DIR)", "")
GNUPLOT		= gnuplot$(EXESUFFIX)
GNUPLOT_OBJDIR	= $(OBJDIR)/gnuplot
GNUPLOT_TARGET	= $(GNUPLOT_OBJDIR)/$(GNUPLOT)
GNUPLOT_HELP	= $(GNUPLOT_OBJDIR)/gnuplot.gih
GNUPLOT_CONFIG_H = $(GNUPLOT_OBJDIR)/config.h
ifneq ("$(LIBGD_DIR)", "")
  GNUPLOT_CONFIG_OPTS = --prefix=/opt/mailstone --with-gd=../../$(LIBGD_OBJDIR) --with-png=no --without-linux-vga
endif

GNUPLOT_ALL	= $(GNUPLOT_OBJDIR) $(GNUPLOT_TARGET) $(GNUPLOT_HELP)

gnuplot:: $(GNUPLOT_ALL)

$(GNUPLOT_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(GNUPLOT_OBJDIR)
else
	[ -d $(GNUPLOT_OBJDIR) ] || mkdir -p $(GNUPLOT_OBJDIR)
endif

$(GNUPLOT_TARGET): $(GNUPLOT_CONFIG_H)
	@$(ECHO) "\n===== [`date`] making gnuplot...\n"
	(cd $(GNUPLOT_OBJDIR); $(MAKE) MAKE=$(MAKE) all)

$(GNUPLOT_HELP):
	cp $(GNUPLOT_DIR)/docs/gnuplot.1 $(GNUPLOT_OBJDIR)
	cp $(GNUPLOT_DIR)/Copyright $(GNUPLOT_OBJDIR)
	cp $(GNUPLOT_OBJDIR)/docs/gnuplot.gih $(GNUPLOT_HELP)

$(GNUPLOT_CONFIG_H):
	@$(ECHO) "\n===== [`date`] making gnuplot config.h...\n"
	(cd $(GNUPLOT_OBJDIR); CC="$(CC) $(CFLAGS)" ../../../src/$(GNUPLOT_DIR)/configure $(GNUPLOT_CONFIG_OPTS))

distclean::
	[ ! -f $(GNUPLOT_OBJDIR)/Makefile ] || \
	  (cd $(GNUPLOT_OBJDIR); $(MAKE) MAKE=$(MAKE) distclean)
	$(RM) $(GNUPLOT_TARGET) $(GNUPLOT_CONFIG_H)

$(GNUPLOT_OBJDIR)/%.$(OBJ_SUFFIX): $(GNUPLOT_DIR)/%.c
ifeq ($(ARCH), WINNT)
	$(COMPILE) -c -MT $< -Fo$(GNUPLOT_OBJDIR)/$*.$(OBJ_SUFFIX)
else	
	$(COMPILE) -c $< -o $(GNUPLOT_OBJDIR)/$*.$(OBJ_SUFFIX)
endif

endif

########################################################################

# mailclient itself

STONE		= $(OBJDIR)/mailclient$(EXE_SUFFIX)

STONESRCS	= bench.c client.c errexit.c http.c imap4.c main.c \
		  parse.c pop3.c smtp.c sysdep.c timefunc.c wmap.c

STONEOBJS	= $(addprefix $(OBJDIR)/, $(STONESRCS:.c=.$(OBJ_SUFFIX)) )

# mailclient currently depends on config.h as created by gnuplot
###mailclient:: gnuplot $(STONE)
mailclient:: $(STONE)

$(STONE): $(STONEOBJS) Makefile
	@$(ECHO) "\n===== [`date`] making $(STONE)...\n"
	$(COMPILE) $(STONEOBJS) $(LIBPATH) $(LIBS) $(OS_LINKFLAGS) -o $(STONE)

$(OBJDIR)/bench.$(OBJ_SUFFIX): bench.c bench.h sysdep.h Makefile

$(OBJDIR)/client.$(OBJ_SUFFIX): client.c bench.h sysdep.h Makefile

$(OBJDIR)/errexit.$(OBJ_SUFFIX): errexit.c bench.h sysdep.h Makefile

$(OBJDIR)/http.$(OBJ_SUFFIX): http.c bench.h pish.h sysdep.h Makefile

$(OBJDIR)/imap4.$(OBJ_SUFFIX): imap4.c bench.h pish.h sysdep.h Makefile

$(OBJDIR)/main.$(OBJ_SUFFIX): main.c bench.h sysdep.h Makefile

$(OBJDIR)/parse.$(OBJ_SUFFIX): parse.c bench.h sysdep.h Makefile

$(OBJDIR)/pop3.$(OBJ_SUFFIX): pop3.c bench.h pish.h sysdep.h Makefile

$(OBJDIR)/smtp.$(OBJ_SUFFIX): smtp.c bench.h pish.h sysdep.h Makefile

$(OBJDIR)/stats.$(OBJ_SUFFIX): stats.c bench.h sysdep.h Makefile

$(OBJDIR)/sysdep.$(OBJ_SUFFIX): sysdep.c bench.h sysdep.h Makefile

$(OBJDIR)/timefunc.$(OBJ_SUFFIX): timefunc.c bench.h sysdep.h Makefile

$(OBJDIR)/wmap.$(OBJ_SUFFIX): wmap.c bench.h sysdep.h Makefile

install: all
	$(CP) $(STONE) $(BINDIR)

distclean:: clean
	$(RM) -rf $(OBJDIR)

clean::
	$(RM) $(STONEOBJS) $(STONE)

# use the higher-level makefile for most builds

debug::
	@$(ECHO) "\n===== [`date`] making debug...\n"
	(cd ..; $(MAKE) debug)

release::
	@$(ECHO) "\n===== [`date`] making release...\n"
	(cd ..; $(MAKE) release)

########################################################################

# optional component perl

PERL_OBJDIR	= $(OBJDIR)/perl
PERL_PKGDIR	= $(PKGDIR)/perl

###package:: $(PERL_PKGDIR) package-perl

$(PERL_PKGDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(PERL_PKGDIR)
	mkdir $(PERL_PKGDIR)/bin
	mkdir $(PERL_PKGDIR)/lib
	mkdir $(PERL_PKGDIR)/lib/$(PERL_REV)
	mkdir $(PERL_PKGDIR)/lib/$(PERL_REV)/$(PERL_OS)
else
	[ -d $(PERL_PKGDIR)/bin ] || mkdir -p $(PERL_PKGDIR)/bin
	[ -d $(PERL_PKGDIR)/lib ] || mkdir -p $(PERL_PKGDIR)/lib
# subdirs created by perl install
endif

ifeq ("$(PERL_FILES)", "")

# building our own perl
###all:: perl
all:: 

PERL_TARGET	= $(PERL_OBJDIR)/perl
PERL_SRCSTAMP	= $(PERL_OBJDIR)/perl_src.stamp
PERL_TESTSTAMP	= $(PERL_OBJDIR)/perl_test.stamp
PERL_PKGSTAMP	= $(PERL_OBJDIR)/perl_pkg.stamp
PERL_CONFIG_H	= $(PERL_OBJDIR)/config.h
PERL_FINAL_PKGDIR = /opt/mailstone/perl
PERL_ADMIN	= mailstone+perl@netscape.com
PERL_PAGER	= /bin/more

perl:: $(PERL_OBJDIR) $(PERL_TARGET) $(PERL_TESTSTAMP)

$(PERL_OBJDIR):
ifeq ($(ARCH), WINNT)
	mkdir $(PERL_OBJDIR)
else
	[ -d $(PERL_OBJDIR) ] || mkdir -p $(PERL_OBJDIR)
endif

$(PERL_TARGET): $(PERL_CONFIG_H)
	@$(ECHO) "\n===== [`date`] making perl...\n"
	( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) )
	@$(ECHO) "\n===== [`date`] making perl done.\n"

$(PERL_CONFIG_H): $(PERL_OBJDIR)/config.over
	@$(ECHO) "\n===== [`date`] making perl config.h...\n"
	(\
	cd $(PERL_OBJDIR); \
	rm -f config.sh makedir makedepend makeaperl config.h cflags \
	      Policy.sh Makefile writemain perl.exp perlmain.c makefile; \
	$(ECHO) MAKE=$(MAKE) MAKEFLAGS=$(MAKEFLAGS); \
	MAKEFLAGS= ; export MAKEFLAGS; \
	sh Configure -Dprefix=$(PERL_FINAL_PKGDIR) \
	       -Uinstallusrbinperl -Uusethreads -Uusedl \
	       -Dcc="$(CC)" -Dmake=$(MAKE) \
	       -Dcf_email=$(PERL_ADMIN) \
	       -Dperladmin=$(PERL_ADMIN) -Dpager=$(PERL_PAGER) $(PERL_OS_CONFIGURE) -de ; \
	)

$(PERL_OBJDIR)/config.over: $(PERL_SRCSTAMP)
	@$(ECHO) "\n===== [`date`] making perl config.over...\n"
	(\
	perl_pkgdir=`pwd`/$(PERL_PKGDIR); \
	cd $(PERL_OBJDIR); \
	$(ECHO) "\
installprefix=$${perl_pkgdir}\n\
$(ECHO) \"overriding tmp install dir from \$$prefix to \$$installprefix\"\n\
test -d \$$installprefix || mkdir \$$installprefix\n\
test -d \$$installprefix/bin || mkdir \$$installprefix/bin\n\
installarchlib=\`$(ECHO) \$$installarchlib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installbin=\`$(ECHO) \$$installbin | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installman1dir=\`$(ECHO) \$$installman1dir | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installman3dir=\`$(ECHO) \$$installman3dir | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installprivlib=\`$(ECHO) \$$installprivlib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installscript=\`$(ECHO) \$$installscript | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installsitelib=\`$(ECHO) \$$installsitelib | sed \"s!\$$prefix!\$$installprefix!\"\`\n\
installsitearch=\`$(ECHO) \$$installsitearch | sed \"s!\$$prefix!\$$installprefix!\"\`" \
> config.over;\
	)

$(PERL_SRCSTAMP): $(PERL_DIR)
	@$(ECHO) "\n===== [`date`] making perl src links from $(PERL_DIR) to $(PERL_OBJDIR)...\n"
	-rm -f $(PERL_SRCSTAMP)
	[ -d $(PERL_OBJDIR) ] || mkdir -p $(PERL_OBJDIR)
	-(\
	perl_dir=`pwd`/$(PERL_DIR); \
	cd $(PERL_OBJDIR); \
	for i in `(cd $${perl_dir} && find . -type d -print)` ; do \
	  $(ECHO) "linking dir $$i ..."; \
	  [ -d $$i ] || mkdir $$i; \
	  for j in `(cd $${perl_dir}/$$i; echo *)` ; do \
	    [ -f $${perl_dir}/$$i/$$j -o -h $${perl_dir}/$$i/$$j ] && ( \
	    /bin/true || $(ECHO) " $$i/$$j"; \
	    ln -s $${perl_dir}/$$i/$$j $$i/$$j); \
	  done; \
	done; \
	)
	touch $(PERL_SRCSTAMP)
	@$(ECHO) "\n===== [`date`] making perl src links done...\n"

$(PERL_TESTSTAMP): $(PERL_TARGET)
	@$(ECHO) "\n===== [`date`] making perl test-notty...\n"
	-rm -f $(PERL_TESTSTAMP)
	-( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) test-notty )
	touch $(PERL_TESTSTAMP)
	@$(ECHO) "\n===== [`date`] making perl test-notty done.\n"

package-perl:: $(PERL_PKGSTAMP)

$(PERL_PKGSTAMP):: $(PERL_TESTSTAMP)
	@$(ECHO) "\n===== [`date`] making perl package ...\n"
	-rm -f $(PERL_PKGSTAMP)
	cp $(PERL_OBJDIR)/Artistic $(PERL_PKGDIR)
	( cd $(PERL_OBJDIR); $(MAKE) MAKE=$(MAKE) install )
	-rm -rf $(PERL_PKGDIR)/lib/$(PERL_REV)/*/CORE/
	touch $(PERL_PKGSTAMP)
	@$(ECHO) "\n===== [`date`] making perl package done.\n"

else # PERL_FILES

# importing a perl
package-perl:: $(PERL_PKGDIR) $(PERL_TARGET)

$(PERL_TARGET):	$(PERL_FILES) $(PERL_BIN_FILES) $(PERL_LIB_FILES)
	cp $(PERL_FILES) $(PERL_PKGDIR)/
	cp $(PERL_BIN_FILES) $(PERL_PKGDIR)/bin/
	cp $(PERL_LIB_FILES) $(PERL_PKGDIR)/lib/$(PERL_REV)/
	cp $(PERL_LIB_OS_FILES) $(PERL_PKGDIR)/lib/$(PERL_REV)/$(PERL_OS)/

endif # PERL_FILES
