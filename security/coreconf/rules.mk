#
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
#

#######################################################################
###                                                                 ###
###              R U L E S   O F   E N G A G E M E N T              ###
###                                                                 ###
#######################################################################

#######################################################################
# Double-Colon rules for utilizing the binary release model.          #
#######################################################################

all:: export libs 

ifeq ($(AUTOCLEAN),1)
autobuild:: clean export private_export libs program install
else
autobuild:: export private_export libs program install
endif

platform::
	@echo $(OBJDIR_NAME)

ifeq ($(OS_ARCH), WINNT)
USE_NT_C_SYNTAX=1
endif

ifdef XP_OS2_VACPP
USE_NT_C_SYNTAX=1
endif

#
# IMPORTS will always be associated with a component.  Therefore,
# the "import" rule will always change directory to the top-level
# of a component, and traverse the IMPORTS keyword from the
# "manifest.mn" file located at this level only.
#
# note: if there is a trailing slash, the component will be appended
#       (see import.pl - only used for xpheader.jar)

import::
	@echo "== import.pl =="
	@perl -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/import.pl \
		"RELEASE_TREE=$(RELEASE_TREE)"   \
		"IMPORTS=$(IMPORTS)"             \
		"VERSION=$(VERSION)" \
		"OS_ARCH=$(OS_ARCH)"             \
		"PLATFORM=$(PLATFORM)" \
		"OVERRIDE_IMPORT_CHECK=$(OVERRIDE_IMPORT_CHECK)"   \
		"ALLOW_VERSION_OVERRIDE=$(ALLOW_VERSION_OVERRIDE)" \
		"SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_XP_DIR)"   \
		"SOURCE_MD_DIR=$(SOURCE_MD_DIR)"      \
		"SOURCE_XP_DIR=$(SOURCE_XP_DIR)"      \
		"FILES=$(IMPORT_XPCLASS_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR)" \
		"$(IMPORT_XPCLASS_JAR)=$(IMPORT_XP_DIR)|$(IMPORT_XPCLASS_DIR)|"    \
		"$(XPHEADER_JAR)=$(IMPORT_XP_DIR)|$(SOURCE_XP_DIR)/public/|v" \
		"$(MDHEADER_JAR)=$(IMPORT_MD_DIR)|$(SOURCE_MD_DIR)/include|"        \
		"$(MDBINARY_JAR)=$(IMPORT_MD_DIR)|$(SOURCE_MD_DIR)|"

export:: 
	+$(LOOP_OVER_DIRS)

private_export::
	+$(LOOP_OVER_DIRS)

release_export::
	+$(LOOP_OVER_DIRS)

release_classes::
	+$(LOOP_OVER_DIRS)

libs program install:: $(TARGETS)
ifdef LIBRARY
	$(INSTALL) -m 664 $(LIBRARY) $(SOURCE_LIB_DIR)
endif
ifdef SHARED_LIBRARY
	$(INSTALL) -m 775 $(SHARED_LIBRARY) $(SOURCE_LIB_DIR)
ifeq ($(OS_ARCH),OpenVMS)
	$(INSTALL) -m 775 $(SHARED_LIBRARY:$(DLL_SUFFIX)=vms) $(SOURCE_LIB_DIR)
endif
endif
ifdef IMPORT_LIBRARY
	$(INSTALL) -m 775 $(IMPORT_LIBRARY) $(SOURCE_LIB_DIR)
endif
ifdef PROGRAM
	$(INSTALL) -m 775 $(PROGRAM) $(SOURCE_BIN_DIR)
endif
ifdef PROGRAMS
	$(INSTALL) -m 775 $(PROGRAMS) $(SOURCE_BIN_DIR)
endif
	+$(LOOP_OVER_DIRS)

tests::
	+$(LOOP_OVER_DIRS)

clean clobber::
	rm -rf $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

realclean clobber_all::
	rm -rf $(wildcard *.OBJ) dist $(ALL_TRASH)
	+$(LOOP_OVER_DIRS)

#ifdef ALL_PLATFORMS
#all_platforms:: $(NFSPWD)
#	@d=`$(NFSPWD)`;							\
#	if test ! -d LOGS; then rm -rf LOGS; mkdir LOGS; fi;		\
#	for h in $(PLATFORM_HOSTS); do					\
#		echo "On $$h: $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log";\
#		rsh $$h -n "(chdir $$d;					\
#			     $(MAKE) $(ALL_PLATFORMS) >& LOGS/$$h.log;	\
#			     echo DONE) &" 2>&1 > LOGS/$$h.pid &	\
#		sleep 1;						\
#	done
#
#$(NFSPWD):
#	cd $(@D); $(MAKE) $(@F)
#endif

#######################################################################
# Double-Colon rules for populating the binary release model.         #
#######################################################################


release_clean::
	rm -rf $(SOURCE_XP_DIR)/release/$(RELEASE_MD_DIR)

release:: release_clean release_export release_classes release_policy release_md release_jars release_cpdistdir

release_cpdistdir::
	@echo "== cpdist.pl =="
	@perl -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/cpdist.pl \
		"RELEASE_TREE=$(RELEASE_TREE)" \
		"CORE_DEPTH=$(CORE_DEPTH)" \
		"MODULE=${MODULE}" \
		"OS_ARCH=$(OS_ARCH)" \
		"RELEASE=$(RELEASE)" \
		"PLATFORM=$(PLATFORM)" \
		"RELEASE_VERSION=$(RELEASE_VERSION)" \
		"SOURCE_RELEASE_PREFIX=$(SOURCE_RELEASE_XP_DIR)" \
		"RELEASE_XP_DIR=$(RELEASE_XP_DIR)" \
		"RELEASE_MD_DIR=$(RELEASE_MD_DIR)" \
		"FILES=$(XPCLASS_JAR) $(XPCLASS_DBG_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR) XP_FILES MD_FILES" \
		"$(XPCLASS_JAR)=$(SOURCE_RELEASE_CLASSES_DIR)|x"\
		"$(XPCLASS_DBG_JAR)=$(SOURCE_RELEASE_CLASSES_DBG_DIR)|x"\
		"$(XPHEADER_JAR)=$(SOURCE_RELEASE_XPHEADERS_DIR)|x" \
		"$(MDHEADER_JAR)=$(SOURCE_RELEASE_MDHEADERS_DIR)|m" \
		"$(MDBINARY_JAR)=$(SOURCE_RELEASE_MD_DIR)|m" \
		"XP_FILES=$(XP_FILES)|xf" \
		"MD_FILES=$(MD_FILES)|mf"


# $(SOURCE_RELEASE_xxx_JAR) is a name like yyy.jar
# $(SOURCE_RELEASE_xx_DIR)  is a name like 

release_jars::
	@echo "== release.pl =="
	@perl -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/release.pl \
		"RELEASE_TREE=$(RELEASE_TREE)" \
		"PLATFORM=$(PLATFORM)" \
		"OS_ARCH=$(OS_ARCH)" \
		"RELEASE_VERSION=$(RELEASE_VERSION)" \
		"SOURCE_RELEASE_DIR=$(SOURCE_RELEASE_DIR)" \
		"FILES=$(XPCLASS_JAR) $(XPCLASS_DBG_JAR) $(XPHEADER_JAR) $(MDHEADER_JAR) $(MDBINARY_JAR)" \
		"$(XPCLASS_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_CLASSES_DIR)|b"\
		"$(XPCLASS_DBG_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_CLASSES_DBG_DIR)|b"\
		"$(XPHEADER_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_XPHEADERS_DIR)|a" \
		"$(MDHEADER_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_MDHEADERS_DIR)|a" \
		"$(MDBINARY_JAR)=$(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_MD_DIR)|bi"

# Rules for releasing classes.
# We have to do some REALLY gross stuff to deal with multiple classes in one
# file, as well as nested classes, which have a filename of the form
# ContainingClass$NestedClass.class.
# RELEASE_CLASSES simply performs a required patsubst on CLASSES
# RELEASE_CLASS_PATH is RELEASE_CLASSES with the path (in ns/dist) prepended
# RELEASE_NESTED is all the nested classes in RELEASE_CLASS_PATH.  We use a
#   foreach and wildcard to get all the files that start out like one of the
#   class files, then have a $.  So, for each class file, we look for file$*
# RELEASE_FILES is the combination of RELEASE_NESTED and the class files
#   specified by RELEASE_CLASSES which have .class appended to them.  Note that
#   the RELEASE_NESTED don't need to have .class appended because they were
#   read in from the wildcard as complete filenames.
#
# The _DBG versions are the debuggable ones.
ifneq ($(CLASSES),)

RELEASE_CLASSES := $(patsubst %,%,$(CLASSES))

ifdef BUILD_OPT
	RELEASE_CLASS_PATH := $(patsubst %,$(SOURCE_CLASSES_DIR)/$(PACKAGE)/%, $(RELEASE_CLASSES))
	RELEASE_NESTED := $(foreach file,$(RELEASE_CLASS_PATH),$(wildcard $(file)$$*))
	RELEASE_FILES := $(patsubst %,%.class,$(RELEASE_CLASS_PATH)) $(RELEASE_NESTED)
else
	RELEASE_DBG_CLASS_PATH:= $(patsubst %,$(SOURCE_CLASSES_DBG_DIR)/$(PACKAGE)/%, $(RELEASE_CLASSES))
	RELEASE_DBG_NESTED := $(foreach file,$(RELEASE_DBG_CLASS_PATH),$(wildcard $(file)$$*))
	RELEASE_DBG_FILES := $(patsubst %,%.class,$(RELEASE_DBG_CLASS_PATH)) $(RELEASE_DBG_NESTED)
endif

# Substitute \$ for $ so the shell doesn't choke
ifdef BUILD_OPT
release_classes::
	$(INSTALL) -m 444 $(subst $$,\$$,$(RELEASE_FILES)) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_CLASSES_DIR)/$(PACKAGE)
else
release_classes::
	$(INSTALL) -m 444 $(subst $$,\$$,$(RELEASE_DBG_FILES)) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_CLASSES_DBG_DIR)/$(PACKAGE)
endif

endif

release_policy::
	+$(LOOP_OVER_DIRS)

ifndef NO_MD_RELEASE
    ifdef LIBRARY
        MD_LIB_RELEASE_FILES +=  $(LIBRARY)
    endif
    ifdef SHARED_LIBRARY
        MD_LIB_RELEASE_FILES +=  $(SHARED_LIBRARY)
    endif
    ifdef IMPORT_LIBRARY
        MD_LIB_RELEASE_FILES +=  $(IMPORT_LIBRARY)
    endif
    ifdef PROGRAM
        MD_BIN_RELEASE_FILES +=  $(PROGRAM)
    endif
    ifdef PROGRAMS
        MD_BIN_RELEASE_FILES +=  $(PROGRAMS)
    endif
endif

release_md::
ifneq ($(MD_LIB_RELEASE_FILES),)
	$(INSTALL) -m 444 $(MD_LIB_RELEASE_FILES) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)
endif
ifneq ($(MD_BIN_RELEASE_FILES),)
	$(INSTALL) -m 555 $(MD_BIN_RELEASE_FILES) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif
	+$(LOOP_OVER_DIRS)


alltags:
	rm -f TAGS
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs etags -a
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs ctags -a

ifdef XP_OS2_VACPP
# list of libs (such as -lnspr4) do not work for our compiler
# change it to be $(DIST)/lib/nspr4.lib
EXTRA_SHARED_LIBS := $(filter-out -L%,$(EXTRA_SHARED_LIBS))
EXTRA_SHARED_LIBS := $(patsubst -l%,$(DIST)/lib/%.$(LIB_SUFFIX),$(EXTRA_SHARED_LIBS))
endif

$(PROGRAM): $(OBJS) $(EXTRA_LIBS)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
	$(MKPROG) $(subst /,\\,$(OBJS)) -Fe$@ -link $(LDFLAGS) $(subst /,\\,$(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS))
else
ifdef XP_OS2_VACPP
	$(MKPROG) -Fe$@ $(CFLAGS) $(OBJS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
else
	$(MKPROG) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
endif
endif

get_objs:
	@echo $(OBJS)

$(LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
ifeq ($(OS_ARCH), WINNT)
	$(AR) $(subst /,\\,$(OBJS))
else
	$(AR) $(OBJS)
endif
	$(RANLIB) $@


ifeq ($(OS_ARCH),OS2)
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
	rm -f $@
	$(IMPLIB) $@ $(patsubst %.lib,%.dll.def,$@)
	$(RANLIB) $@
endif

ifdef SHARED_LIBRARY_LIBS
ifdef BUILD_TREE
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(shell $(MAKE) -C $(dir) --no-print-directory get_objs))
else
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(addprefix $(dir)/,$(shell $(MAKE) -C $(dir) --no-print-directory get_objs)))
endif
endif

$(SHARED_LIBRARY): $(OBJS) $(MAPFILE)
	@$(MAKE_OBJDIR)
	rm -f $@
ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
	echo "#!" > $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	nm -B -C -g $(OBJS) \
	| awk '/ [T,D] / {print $$3}' \
	| sed -e 's/^\.//' \
	| sort -u >> $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	$(LD) $(XCFLAGS) -o $@ $(OBJS) -bE:$(OBJDIR)/lib$(LIBRARY_NAME)_syms \
	-bM:SRE -bnoentry $(OS_LIBS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS)
else
ifeq ($(OS_ARCH), WINNT)
	$(LINK_DLL) -MAP $(DLLBASE) $(subst /,\\,$(OBJS) $(SUB_SHLOBJS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS) $(LD_LIBS))
else
ifeq ($(OS_ARCH),OS2)
	@cmd /C "echo LIBRARY $(notdir $(basename $(SHARED_LIBRARY))) INITINSTANCE TERMINSTANCE >$@.def"
	@cmd /C "echo PROTMODE >>$@.def"
	@cmd /C "echo CODE    LOADONCALL MOVEABLE DISCARDABLE >>$@.def"
	@cmd /C "echo DATA    PRELOAD MOVEABLE MULTIPLE NONSHARED >>$@.def"	
	@cmd /C "echo EXPORTS >>$@.def"
	$(FILTER) $(OBJS) >>$@.def
ifdef SUB_SHLOBJS
	@echo Number of words in OBJ list = $(words $(SUB_SHLOBJS))
	@echo If above number is over 100, need to reedit coreconf/rules.mk
	-$(FILTER) $(wordlist 1,20,$(SUB_SHLOBJS)) >>$@.def
	-$(FILTER) $(wordlist 21,40,$(SUB_SHLOBJS)) >>$@.def
	-$(FILTER) $(wordlist 41,60,$(SUB_SHLOBJS)) >>$@.def
	-$(FILTER) $(wordlist 61,80,$(SUB_SHLOBJS)) >>$@.def
	-$(FILTER) $(wordlist 81,100,$(SUB_SHLOBJS)) >>$@.def
endif
endif #OS2
ifdef XP_OS2_VACPP
	$(MKSHLIB) $(DLLFLAGS) $(LDFLAGS) $(OBJS) $(SUB_SHLOBJS) $(LD_LIBS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $@.def
else
	$(MKSHLIB) -o $@ $(OBJS) $(SUB_SHLOBJS) $(LD_LIBS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS)
endif
	chmod +x $@
ifeq ($(OS_ARCH),OpenVMS)
	@echo "`translate $@`" > $(@:$(DLL_SUFFIX)=vms)
endif
endif
endif

ifeq ($(OS_ARCH), WINNT)
$(RES): $(RESNAME)
	@$(MAKE_OBJDIR)
# The resource compiler does not understand the -U option.
	$(RC) $(filter-out -U%,$(DEFINES)) $(INCLUDES) -Fo$@ $<
	@echo $(RES) finished
endif

$(MAPFILE): $(LIBRARY_NAME).def
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),SunOS)
	grep -v ';-' $(LIBRARY_NAME).def | \
	 sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@
endif
ifeq ($(OS_ARCH),Linux)
	grep -v ';-' $(LIBRARY_NAME).def | \
	 sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@
endif
ifeq ($(OS_ARCH),AIX)
	grep -v ';+' $(LIBRARY_NAME).def | grep -v ';-' | \
		sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' > $@
endif
ifeq ($(OS_ARCH), HP-UX)
	grep -v ';+' $(LIBRARY_NAME).def | grep -v ';-' | \
	 sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,+e ,' > $@
endif
ifeq ($(OS_ARCH), OSF1)
	grep -v ';+' $(LIBRARY_NAME).def | grep -v ';-' | \
 sed -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,,' -e 's,^,-exported_symbol ,' > $@
endif



$(OBJDIR)/$(PROG_PREFIX)%$(PROG_SUFFIX): $(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq ($(OS_ARCH),WINNT)
	$(MKPROG) $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) -Fe$@ -link \
	$(LDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
else
	$(MKPROG) -o $@ $(OBJDIR)/$(PROG_PREFIX)$*$(OBJ_SUFFIX) \
	$(LDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
endif

WCCFLAGS1 := $(subst /,\\,$(CFLAGS))
WCCFLAGS2 := $(subst -I,-i=,$(WCCFLAGS1))
WCCFLAGS3 := $(subst -D,-d,$(WCCFLAGS2))

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c
	@$(MAKE_OBJDIR)
ifdef USE_NT_C_SYNTAX
ifdef XP_OS2_VACPP
	$(CC) -Fo$@ -c $(CFLAGS) $(subst /,\\,$(shell pwd)\\$<)
else
	$(CC) -Fo$@ -c $(CFLAGS) $(subst /,\\,$<)
endif
else
	$(CC) -o $@ -c $(CFLAGS) $<
endif

$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c
ifdef USE_NT_C_SYNTAX
	$(CC) -Fo$@ -c $(CFLAGS) $(subst /,\\,$<)
else
	$(CC) -o $@ -c $(CFLAGS) $<
endif

ifneq ($(OS_ARCH), WINNT)
$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.s
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) -c $<
endif

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.asm
	@$(MAKE_OBJDIR)
	$(AS) -Fo$@ $(ASFLAGS) -c $<

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.S
	@$(MAKE_OBJDIR)
	$(AS) -o $@ $(ASFLAGS) -c $<

$(OBJDIR)/$(PROG_PREFIX)%: %.cpp
	@$(MAKE_OBJDIR)
ifdef USE_NT_C_SYNTAX
	$(CCC) -Fo$@ -c $(CFLAGS) $<
else
	$(CCC) -o $@ -c $(CFLAGS) $<
endif

#
# Please keep the next two rules in sync.
#
$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cc
	@$(MAKE_OBJDIR)
	$(CCC) -o $@ -c $(CFLAGS) $<

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cpp
	@$(MAKE_OBJDIR)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$<\"" | cat - $< > $(OBJDIR)/t_$*.cc
	$(CCC) -o $@ -c $(CFLAGS) $(OBJDIR)/t_$*.cc
	rm -f $(OBJDIR)/t_$*.cc
else
ifdef USE_NT_C_SYNTAX
	$(CCC) -Fo$@ -c $(CFLAGS) $<
else
	$(CCC) -o $@ -c $(CFLAGS) $<
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
	$(CCC) -C -E $(CFLAGS) $< > $*.i

%.i: %.c
ifeq ($(OS_ARCH),WINNT)
	$(CC) -C /P $(CFLAGS) $< 
else
	$(CC) -C -E $(CFLAGS) $< > $*.i
endif

ifneq ($(OS_ARCH), WINNT)
%.i: %.s
	$(CC) -C -E $(CFLAGS) $< > $*.i
endif

%: %.pl
	rm -f $@; cp $*.pl $@; chmod +x $@

%: %.sh
	rm -f $@; cp $*.sh $@; chmod +x $@

ifdef DIRS
$(DIRS)::
	@if test -d $@; then				\
		set $(EXIT_ON_ERROR);			\
		echo "cd $@; $(MAKE)";			\
		cd $@; $(MAKE);				\
		set +e;					\
	else						\
		echo "Skipping non-directory $@...";	\
	fi;						\
	$(CLICK_STOPWATCH)
endif

################################################################################
# Bunch of things that extend the 'export' rule (in order):
################################################################################

$(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE) $(JMCSRCDIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		rm -rf $@;	    \
		$(NSINSTALL) -D $@; \
	fi

################################################################################
## IDL_GEN

ifneq ($(IDL_GEN),)

#export::
#	$(IDL2JAVA) $(IDL_GEN)

#all:: export

#clobber::
#	rm -f $(IDL_GEN:.idl=.class)	# XXX wrong!

endif

################################################################################
### JSRCS -- for compiling java files
###
###          NOTE:  For backwards compatibility, if $(NETLIBDEPTH) is defined,
###                 replace $(CORE_DEPTH) with $(NETLIBDEPTH).
###

ifneq ($(JSRCS),)
ifneq ($(JAVAC),)
ifdef NETLIBDEPTH
	CORE_DEPTH := $(NETLIBDEPTH)
endif

JAVA_EXPORT_SRCS=$(shell perl $(CORE_DEPTH)/coreconf/outofdate.pl $(PERLARG)	-d $(JAVA_DESTPATH)/$(PACKAGE) $(JSRCS) $(PRIVATE_JSRCS))

export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
ifneq ($(JAVA_EXPORT_SRCS),)
	$(JAVAC) $(JAVA_EXPORT_SRCS)
endif

all:: export

clobber::
	rm -f $(SOURCE_XP_DIR)/classes/$(PACKAGE)/*.class

endif
endif

#
# JDIRS -- like JSRCS, except you can give a list of directories and it will
# compile all the out-of-date java files in those directories.
#
# NOTE: recursing through these can speed things up, but they also cause
# some builds to run out of memory
#
# NOTE:  For backwards compatibility, if $(NETLIBDEPTH) is defined,
#        replace $(CORE_DEPTH) with $(NETLIBDEPTH).
#
ifdef JDIRS
ifneq ($(JAVAC),)
ifdef NETLIBDEPTH
	CORE_DEPTH := $(NETLIBDEPTH)
endif

# !!!!! THIS WILL CRASH SHMSDOS.EXE !!!!!
# shmsdos does not support shell variables. It will crash when it tries
# to parse the '=' character. A solution is to rewrite outofdate.pl so it
# takes the Javac command as an argument and executes the command itself,
# instead of returning a list of files.
export:: $(JAVA_DESTPATH) $(JAVA_DESTPATH)/$(PACKAGE)
	@echo "!!! THIS COMMAND IS BROKEN ON WINDOWS--SEE rules.mk FOR DETAILS !!!"
	return -1
	@for d in $(JDIRS); do							\
		if test -d $$d; then						\
			set $(EXIT_ON_ERROR);					\
			files=`echo $$d/*.java`;				\
			list=`perl $(CORE_DEPTH)/coreconf/outofdate.pl $(PERLARG)	\
				    -d $(JAVA_DESTPATH)/$(PACKAGE) $$files`;	\
			if test "$${list}x" != "x"; then			\
			    echo Building all java files in $$d;		\
			    echo $(JAVAC) $$list;				\
			    $(JAVAC) $$list;					\
			fi;							\
			set +e;							\
		else								\
			echo "Skipping non-directory $$d...";			\
		fi;								\
		$(CLICK_STOPWATCH);						\
	done
endif
endif

#
# JDK_GEN -- for generating "old style" native methods 
#
# Generate JDK Headers and Stubs into the '_gen' and '_stubs' directory
#
# NOTE:  For backwards compatibility, if $(NETLIBDEPTH) is defined,
#        replace $(CORE_DEPTH) with $(NETLIBDEPTH).
#
ifneq ($(JDK_GEN),)
ifneq ($(JAVAH),)
ifdef NSBUILDROOT
	INCLUDES += -I$(JDK_GEN_DIR) -I$(SOURCE_XP_DIR)
else
	INCLUDES += -I$(JDK_GEN_DIR)
endif

ifdef NETLIBDEPTH
	CORE_DEPTH := $(NETLIBDEPTH)
endif

JDK_PACKAGE_CLASSES	:= $(JDK_GEN)
JDK_PATH_CLASSES	:= $(subst .,/,$(JDK_PACKAGE_CLASSES))
JDK_HEADER_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_STUB_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JDK_PATH_CLASSES))
JDK_HEADER_CFILES	:= $(patsubst %,$(JDK_GEN_DIR)/%.h,$(JDK_GEN))
JDK_STUB_CFILES		:= $(patsubst %,$(JDK_STUB_DIR)/%.c,$(JDK_GEN))

$(JDK_HEADER_CFILES): $(JDK_HEADER_CLASSFILES)
$(JDK_STUB_CFILES): $(JDK_STUB_CLASSFILES)

export::
	@echo Generating/Updating JDK headers 
	$(JAVAH) -d $(JDK_GEN_DIR) $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs
	$(JAVAH) -stubs -d $(JDK_STUB_DIR) $(JDK_PACKAGE_CLASSES)
ifndef NO_MAC_JAVA_SHIT
	@if test ! -d $(CORE_DEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	fi
	@echo Generating/Updating JDK headers for the Mac
	$(JAVAH) -mac -d $(CORE_DEPTH)/lib/mac/Java/_gen $(JDK_PACKAGE_CLASSES)
	@echo Generating/Updating JDK stubs for the Mac
	$(JAVAH) -mac -stubs -d $(CORE_DEPTH)/lib/mac/Java/_stubs $(JDK_PACKAGE_CLASSES)
endif
endif
endif

#
# JRI_GEN -- for generating "old style" JRI native methods
#
# Generate JRI Headers and Stubs into the 'jri' directory
#
# NOTE:  For backwards compatibility, if $(NETLIBDEPTH) is defined,
#        replace $(CORE_DEPTH) with $(NETLIBDEPTH).
#
ifneq ($(JRI_GEN),)
ifneq ($(JAVAH),)
ifdef NSBUILDROOT
	INCLUDES += -I$(JRI_GEN_DIR) -I$(SOURCE_XP_DIR)
else
	INCLUDES += -I$(JRI_GEN_DIR)
endif

ifdef NETLIBDEPTH
	CORE_DEPTH := $(NETLIBDEPTH)
endif

JRI_PACKAGE_CLASSES	:= $(JRI_GEN)
JRI_PATH_CLASSES	:= $(subst .,/,$(JRI_PACKAGE_CLASSES))
JRI_HEADER_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_STUB_CLASSFILES	:= $(patsubst %,$(JAVA_DESTPATH)/%.class,$(JRI_PATH_CLASSES))
JRI_HEADER_CFILES	:= $(patsubst %,$(JRI_GEN_DIR)/%.h,$(JRI_GEN))
JRI_STUB_CFILES		:= $(patsubst %,$(JRI_GEN_DIR)/%.c,$(JRI_GEN))

$(JRI_HEADER_CFILES): $(JRI_HEADER_CLASSFILES)
$(JRI_STUB_CFILES): $(JRI_STUB_CLASSFILES)

export::
	@echo Generating/Updating JRI headers 
	$(JAVAH) -jri -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs
	$(JAVAH) -jri -stubs -d $(JRI_GEN_DIR) $(JRI_PACKAGE_CLASSES)
ifndef NO_MAC_JAVA_SHIT
	@if test ! -d $(CORE_DEPTH)/lib/mac/Java/; then						\
		echo "!!! You need to have a ns/lib/mac/Java directory checked out.";		\
		echo "!!! This allows us to automatically update generated files for the mac.";	\
		echo "!!! If you see any modified files there, please check them in.";		\
	fi
	@echo Generating/Updating JRI headers for the Mac
	$(JAVAH) -jri -mac -d $(CORE_DEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
	@echo Generating/Updating JRI stubs for the Mac
	$(JAVAH) -jri -mac -stubs -d $(CORE_DEPTH)/lib/mac/Java/_jri $(JRI_PACKAGE_CLASSES)
endif
endif
endif

#
# JNI_GEN -- for generating JNI native methods
#
# Generate JNI Headers into the 'jni' directory
#
ifneq ($(JNI_GEN),)
ifneq ($(JAVAH),)
JNI_HEADERS		:= $(patsubst %,$(JNI_GEN_DIR)/%.h,$(JNI_GEN))

export::
	@if test ! -d $(JNI_GEN_DIR); then						\
		echo $(JAVAH) -jni -d $(JNI_GEN_DIR) $(JNI_GEN);			\
		$(JAVAH) -jni -d $(JNI_GEN_DIR) $(JNI_GEN);				\
	else										\
		echo "Checking for out of date header files" ;                          \
		perl $(CORE_DEPTH)/coreconf/jniregen.pl $(PERLARG)			\
		 -d $(JAVA_DESTPATH) -j "$(JAVAH) -jni -d $(JNI_GEN_DIR)" $(JNI_GEN);\
	fi
endif
endif

#
# JMC_EXPORT -- for declaring which java classes are to be exported for jmc
#
ifneq ($(JMC_EXPORT),)
JMC_EXPORT_PATHS	:= $(subst .,/,$(JMC_EXPORT))
JMC_EXPORT_FILES	:= $(patsubst %,$(JAVA_DESTPATH)/$(PACKAGE)/%.class,$(JMC_EXPORT_PATHS))

#
# We're doing NSINSTALL -t here (copy mode) because calling INSTALL will pick up 
# your NSDISTMODE and make links relative to the current directory. This is a
# problem because the source isn't in the current directory:
#
export:: $(JMC_EXPORT_FILES) $(JMCSRCDIR)
	$(NSINSTALL) -t -m 444 $(JMC_EXPORT_FILES) $(JMCSRCDIR)
endif

#
# JMC_GEN -- for generating java modules
#
# Provide default export & install rules when using JMC_GEN
#
ifneq ($(JMC_GEN),)
ifneq ($(JMC),)
	INCLUDES    += -I$(JMC_GEN_DIR) -I.
	JMC_HEADERS := $(patsubst %,$(JMC_GEN_DIR)/%.h,$(JMC_GEN))
	JMC_STUBS   := $(patsubst %,$(JMC_GEN_DIR)/%.c,$(JMC_GEN))
	JMC_OBJS    := $(patsubst %,$(OBJDIR)/%$(OBJ_SUFFIX),$(JMC_GEN))

$(JMC_GEN_DIR)/M%.h: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -interface $(JMC_GEN_FLAGS) $(?F:.class=)

$(JMC_GEN_DIR)/M%.c: $(JMCSRCDIR)/%.class
	$(JMC) -d $(JMC_GEN_DIR) -module $(JMC_GEN_FLAGS) $(?F:.class=)

$(OBJDIR)/M%$(OBJ_SUFFIX): $(JMC_GEN_DIR)/M%.h $(JMC_GEN_DIR)/M%.c
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $(JMC_GEN_DIR)/M$*.c

export:: $(JMC_HEADERS) $(JMC_STUBS)
endif
endif

#
# Copy each element of EXPORTS to $(SOURCE_XP_DIR)/public/$(MODULE)/
#
PUBLIC_EXPORT_DIR = $(SOURCE_XP_DIR)/public/$(MODULE)

ifneq ($(EXPORTS),)
$(PUBLIC_EXPORT_DIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

export:: $(PUBLIC_EXPORT_DIR) 

export:: $(EXPORTS) 
	$(INSTALL) -m 444 $^ $(PUBLIC_EXPORT_DIR)

export:: $(BUILT_SRCS)
endif

# Duplicate export rule for private exports, with different directories

PRIVATE_EXPORT_DIR = $(SOURCE_XP_DIR)/private/$(MODULE)

ifneq ($(PRIVATE_EXPORTS),)
$(PRIVATE_EXPORT_DIR)::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

private_export:: $(PRIVATE_EXPORT_DIR)

private_export:: $(PRIVATE_EXPORTS) 
	$(INSTALL) -m 444 $^ $(PRIVATE_EXPORT_DIR)
else
private_export:: 
	@echo There are no private exports.;
endif

##########################################################################
###   RULES FOR RUNNING REGRESSION SUITE TESTS
###   REQUIRES 'REGRESSION_SPEC' TO BE SET TO THE NAME OF A REGRESSION SPECFILE
###   AND RESULTS_SUBDIR TO BE SET TO SOMETHING LIKE SECURITY/PKCS5
##########################################################################

TESTS_DIR = $(RESULTS_DIR)/$(RESULTS_SUBDIR)/$(OS_CONFIG)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)

ifneq ($(REGRESSION_SPEC),)
tests:: $(REGRESSION_SPEC) 
	cd $(PLATFORM); \
	../$(SOURCE_MD_DIR)/bin/regress$(PROG_SUFFIX) specfile=../$(REGRESSION_SPEC) progress $(EXTRA_REGRESS_OPTIONS); \
	if test ! -d $(TESTS_DIR); then \
		echo Creating $(TESTS_DIR);   \
		$(NSINSTALL) -D $(TESTS_DIR); \
	fi
ifneq ($(BUILD_OPT),)
	$(NSINSTALL) -m 664 $(PLATFORM)/$(REGDATE).sum $(TESTS_DIR); \
	$(NSINSTALL) -m 664 $(PLATFORM)/$(REGDATE).htm $(TESTS_DIR); \
	echo "Please now make sure your results files are copied to $(TESTS_DIR), "; \
	echo "then run 'reporter specfile=$(RESULTS_DIR)/rptspec'"
endif
else
tests:: 
	@echo Error: you didn't specify REGRESSION_SPEC in your manifest.mn file!;
endif


# Duplicate export rule for releases, with different directories

ifneq ($(EXPORTS),)
$(SOURCE_RELEASE_XP_DIR)/include::
	@if test ! -d $@; then	    \
		echo Creating $@;   \
		$(NSINSTALL) -D $@; \
	fi

release_export:: $(EXPORTS) $(SOURCE_RELEASE_XP_DIR)/include
	$(INSTALL) -m 444 $(EXPORTS) $(SOURCE_RELEASE_XP_DIR)/include
endif




################################################################################

-include $(DEPENDENCIES)

ifneq (,$(filter-out OS2 WINNT,$(OS_ARCH)))
# Can't use sed because of its 4000-char line length limit, so resort to perl
.DEFAULT:
	@perl -e '                                                            \
	    open(MD, "< $(DEPENDENCIES)");                                    \
	    while (<MD>) {                                                    \
		if (m@ \.*/*$< @) {                                           \
		    $$found = 1;                                              \
		    last;                                                     \
		}                                                             \
	    }                                                                 \
	    if ($$found) {                                                    \
		print "Removing stale dependency $< from $(DEPENDENCIES)\n";  \
		seek(MD, 0, 0);                                               \
		$$tmpname = "$(OBJDIR)/fix.md" . $$$$;                        \
		open(TMD, "> " . $$tmpname);                                  \
		while (<MD>) {                                                \
		    s@ \.*/*$< @ @;                                           \
		    if (!print TMD "$$_") {                                   \
			unlink(($$tmpname));                                  \
			exit(1);                                              \
		    }                                                         \
		}                                                             \
		close(TMD);                                                   \
		if (!rename($$tmpname, "$(DEPENDENCIES)")) {                  \
		    unlink(($$tmpname));                                      \
		}                                                             \
	    } elsif ("$<" ne "$(DEPENDENCIES)") {                             \
		print "$(MAKE): *** No rule to make target $<.  Stop.\n";     \
		exit(1);                                                      \
	    }'
endif

#############################################################################
# X dependency system
#############################################################################

ifdef MKDEPENDENCIES

# For Windows, $(MKDEPENDENCIES) must be -included before including rules.mk

$(MKDEPENDENCIES)::
	@$(MAKE_OBJDIR)
	touch $(MKDEPENDENCIES) 
	chmod u+w $(MKDEPENDENCIES) 
#on NT, the preceeding touch command creates a read-only file !?!?!
#which is why we have to explicitly chmod it.
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'$(OBJ_SUFFIX)' -f$(MKDEPENDENCIES) \
$(NOMD_CFLAGS) $(YOPT) $(CSRCS) $(CPPSRCS) $(ASFILES)

$(MKDEPEND):: $(MKDEPEND_DIR)/*.c $(MKDEPEND_DIR)/*.h
	cd $(MKDEPEND_DIR); $(MAKE)

ifdef OBJS
depend:: $(MKDEPEND) $(MKDEPENDENCIES)
else
depend::
endif
	+$(LOOP_OVER_DIRS)

dependclean::
	rm -f $(MKDEPENDENCIES)
	+$(LOOP_OVER_DIRS)

#-include $(NSINSTALL_DIR)/$(OBJDIR)/depend.mk

else
depend::
endif

################################################################################
# Special gmake rules.
################################################################################

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:
.SUFFIXES: .out .a .ln .o .obj .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html .asm

#
# Don't delete these files if we get killed.
#
.PRECIOUS: .java $(JDK_HEADERS) $(JDK_STUBS) $(JRI_HEADERS) $(JRI_STUBS) $(JMC_HEADERS) $(JMC_STUBS) $(JNI_HEADERS)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot clean clobber clobber_all export install libs program realclean release $(OBJDIR) $(DIRS)

