# Makefile for mailstone
# This should be run using 'gmake'
########################################################################

topsrcdir	= ..

ifndef INCLUDED_CONFIG_MK
include $(topsrcdir)/config/config.mk
endif

# you may want to force debug on the command line (default is release)
# BUILD_VARIANT=debug
# pass in extra compile arguments in CPPFLAGS

BINDIR		= ../bin

########################################################################

# what are we making

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

########################################################################
# mailclient itself

STONE		= $(OBJDIR)/mailclient$(EXE_SUFFIX)

STONESRCS	= bench.c client.c errexit.c main.c \
		parse.c sysdep.c timefunc.c \
		http.c imap4.c pop3.c smtp.c wmap.c 

STONEOBJS	= $(addprefix $(OBJDIR)/, $(STONESRCS:.c=.$(OBJ_SUFFIX)) )


mailclient:: $(STONE)

$(OBJDIR):
	mkdir -p $(OBJDIR)

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

# currently broken.  See ../Makefile for packaging
install: all
	$(CP) $(STONE) $(BINDIR)

# clean out everything that we created.
distclean:: clean
	$(RM) -rf $(OBJDIR)

# clean out the intermediate files, keep the executable
clean::
	$(RM) $(STONEOBJS)

debug::
	@$(ECHO) "\n===== [`date`] making debug...\n"
	$(MAKE) BUILD_VARIANT=debug OBJDIR_TAG=_DBG

release::
	@$(ECHO) "\n===== [`date`] making release...\n"
	$(MAKE) BUILD_VARIANT=release OBJDIR_TAG=_OPT

########################################################################
