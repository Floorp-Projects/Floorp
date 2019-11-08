#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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

ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
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
	@$(PERL) -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/import.pl \
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
# On Mac OS X ranlib needs to be rerun after static libs are moved.
ifeq ($(OS_TARGET),Darwin)
	find $(SOURCE_MD_DIR)/lib -name "*.a" -exec $(RANLIB) {} \;
endif

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
ifdef MOZ_DEBUG_SYMBOLS
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(INSTALL) -m 644 $(SHARED_LIBRARY:$(DLL_SUFFIX)=pdb) $(SOURCE_LIB_DIR)
endif
endif
endif
ifdef IMPORT_LIBRARY
	$(INSTALL) -m 775 $(IMPORT_LIBRARY) $(SOURCE_LIB_DIR)
endif
ifdef PROGRAM
	$(INSTALL) -m 775 $(PROGRAM) $(SOURCE_BIN_DIR)
ifdef MOZ_DEBUG_SYMBOLS
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(INSTALL) -m 644 $(PROGRAM:$(PROG_SUFFIX)=.pdb) $(SOURCE_BIN_DIR)
endif
endif
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

#######################################################################
# Double-Colon rules for populating the binary release model.         #
#######################################################################


release_clean::
	rm -rf $(SOURCE_XP_DIR)/release/$(RELEASE_MD_DIR)

release:: release_clean release_export release_classes release_policy release_md release_jars release_cpdistdir

release_cpdistdir::
	@echo "== cpdist.pl =="
	@$(PERL) -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/cpdist.pl \
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
	@$(PERL) -I$(CORE_DEPTH)/coreconf $(CORE_DEPTH)/coreconf/release.pl \
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

$(PROGRAM): $(OBJS) $(EXTRA_LIBS)
	@$(MAKE_OBJDIR)
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(MKPROG) $(subst /,\\,$(OBJS)) -Fe$@ -link $(LDFLAGS) $(XLDFLAGS) $(subst /,\\,$(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS))
ifdef MT
	if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$(MKPROG) -o $@ $(CFLAGS) $(OBJS) $(LDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
endif

get_objs:
	@echo $(OBJS)

$(LIBRARY): $(OBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(AR) $(subst /,\\,$(OBJS))
else
	$(AR) $(OBJS)
endif
	$(RANLIB) $@


ifeq ($(OS_TARGET),OS2)
$(IMPORT_LIBRARY): $(MAPFILE)
	rm -f $@
	$(IMPLIB) $@ $<
	$(RANLIB) $@
endif
ifeq ($(OS_ARCH),WINNT)
$(IMPORT_LIBRARY): $(LIBRARY)
	cp -f $< $@
endif

ifdef SHARED_LIBRARY_LIBS
ifdef BUILD_TREE
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(shell $(MAKE) -C $(dir) --no-print-directory get_objs))
else
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(addprefix $(dir)/,$(shell $(MAKE) -C $(dir) --no-print-directory get_objs)))
endif
endif

$(SHARED_LIBRARY): $(OBJS) $(RES) $(MAPFILE) $(SUB_SHLOBJS)
	@$(MAKE_OBJDIR)
	rm -f $@
ifeq ($(OS_TARGET)$(OS_RELEASE), AIX4.1)
	echo "#!" > $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	nm -B -C -g $(OBJS) \
	| awk '/ [T,D] / {print $$3}' \
	| sed -e 's/^\.//' \
	| sort -u >> $(OBJDIR)/lib$(LIBRARY_NAME)_syms
	$(LD) $(XCFLAGS) -o $@ $(OBJS) -bE:$(OBJDIR)/lib$(LIBRARY_NAME)_syms \
	-bM:SRE -bnoentry $(OS_LIBS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS)
else
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
ifdef NS_USE_GCC
	$(LINK_DLL) $(OBJS) $(SUB_SHLOBJS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS) $(LD_LIBS) $(RES)
else
	$(LINK_DLL) -MAP $(DLLBASE) $(subst /,\\,$(OBJS) $(SUB_SHLOBJS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS) $(LD_LIBS) $(RES))
ifdef MT
	if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;2; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
endif
else
	$(MKSHLIB) -o $@ $(OBJS) $(SUB_SHLOBJS) $(LD_LIBS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
	chmod +x $@
endif
endif

ifeq (,$(filter-out WIN%,$(OS_TARGET)))
$(RES): $(RESNAME)
	@$(MAKE_OBJDIR)
# The resource compiler does not understand the -U option.
ifdef NS_USE_GCC
	$(RC) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) -o $@ $<
else
	$(RC) $(filter-out -U%,$(DEFINES)) $(INCLUDES) -Fo$@ $<
endif
	@echo $(RES) finished
endif

$(MAPFILE): $(MAPFILE_SOURCE)
	@$(MAKE_OBJDIR)
	$(PROCESS_MAP_FILE)


$(OBJDIR)/$(PROG_PREFIX)%$(PROG_SUFFIX): $(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX)
	@$(MAKE_OBJDIR)
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(MKPROG) $< -Fe$@ -link \
	$(LDFLAGS) $(XLDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
ifdef MT
	if test -f $@.manifest; then \
		$(MT) -NOLOGO -MANIFEST $@.manifest -OUTPUTRESOURCE:$@\;1; \
		rm -f $@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$(MKPROG) -o $@ $(CFLAGS) $< \
	$(LDFLAGS) $(EXTRA_LIBS) $(EXTRA_SHARED_LIBS) $(OS_LIBS)
endif

WCCFLAGS1 := $(subst /,\\,$(CFLAGS))
WCCFLAGS2 := $(subst -I,-i=,$(WCCFLAGS1))
WCCFLAGS3 := $(subst -D,-d,$(WCCFLAGS2))

# Translate source filenames to absolute paths. This is required for
# debuggers under Windows & OS/2 to find source files automatically

ifeq (,$(filter-out OS2 AIX,$(OS_TARGET)))
# OS/2 and AIX
NEED_ABSOLUTE_PATH := 1
PWD := $(shell pwd)

else
# Windows
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
NEED_ABSOLUTE_PATH := 1
# CURDIR is always an absolute path. If it doesn't start with a /, it's a
# Windows path meaning we're running under MINGW make (as opposed to MSYS
# make), or pymake. In both cases, it's preferable to use a Windows path,
# so use $(CURDIR) as is.
ifeq (,$(filter /%,$(CURDIR)))
PWD := $(CURDIR)
else
PWD := $(shell pwd)
ifeq (,$(findstring ;,$(PATH)))
ifndef USE_MSYS
PWD := $(subst \,/,$(shell cygpath -w $(PWD)))
endif
endif
endif

else
# everything else
PWD := $(shell pwd)
endif
endif

# The quotes allow absolute paths to contain spaces.
core_abspath = '$(if $(findstring :,$(1)),$(1),$(if $(filter /%,$(1)),$(1),$(PWD)/$(1)))'

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c
	@$(MAKE_OBJDIR)
ifdef USE_NT_C_SYNTAX
	$(CC) -Fo$@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $<
endif
endif

$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c
ifdef USE_NT_C_SYNTAX
	$(CC) -Fo$@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $<
endif
endif

ifneq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
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
	$(CCC) -Fo$@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $<
endif
endif

#
# Please keep the next two rules in sync.
#
$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cc
	$(MAKE_OBJDIR)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$<\"" | cat - $< > $(OBJDIR)/t_$*.cc
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(OBJDIR)/t_$*.cc
	rm -f $(OBJDIR)/t_$*.cc
else
ifdef USE_NT_C_SYNTAX
	$(CCC) -Fo$@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $<
endif
endif
endif #STRICT_CPLUSPLUS_SUFFIX

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.cpp
	@$(MAKE_OBJDIR)
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$<\"" | cat - $< > $(OBJDIR)/t_$*.cc
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(OBJDIR)/t_$*.cc
	rm -f $(OBJDIR)/t_$*.cc
else
ifdef USE_NT_C_SYNTAX
	$(CCC) -Fo$@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $<
endif
endif
endif #STRICT_CPLUSPLUS_SUFFIX

%.i: %.cpp
	$(CCC) -C -E $(CFLAGS) $(CXXFLAGS) $< > $@

%.i: %.c
ifeq (,$(filter-out WIN%,$(OS_TARGET)))
	$(CC) -C /P $(CFLAGS) $< 
else
	$(CC) -C -E $(CFLAGS) $< > $@
endif

ifneq (,$(filter-out WIN%,$(OS_TARGET)))
%.i: %.s
	$(CC) -C -E $(CFLAGS) $< > $@
endif

%: %.pl
	rm -f $@; cp $< $@; chmod +x $@

%: %.sh
	rm -f $@; cp $< $@; chmod +x $@

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

JAVA_EXPORT_SRCS=$(shell $(PERL) $(CORE_DEPTH)/coreconf/outofdate.pl $(PERLARG)	-d $(JAVA_DESTPATH)/$(PACKAGE) $(JSRCS) $(PRIVATE_JSRCS))

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
			list=`$(PERL) $(CORE_DEPTH)/coreconf/outofdate.pl $(PERLARG)	\
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
		$(PERL) $(CORE_DEPTH)/coreconf/jniregen.pl $(PERLARG)			\
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

$(OBJDIR)/M%$(OBJ_SUFFIX): $(JMC_GEN_DIR)/M%.c $(JMC_GEN_DIR)/M%.h
	@$(MAKE_OBJDIR)
	$(CC) -o $@ -c $(CFLAGS) $<

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

TESTS_DIR = $(RESULTS_DIR)/$(RESULTS_SUBDIR)/$(OS_TARGET)$(OS_RELEASE)$(CPU_TAG)$(COMPILER_TAG)$(IMPL_STRATEGY)

ifneq ($(REGRESSION_SPEC),)

ifneq ($(BUILD_OPT),)
REGDATE = $(subst \ ,, $(shell $(PERL)  $(CORE_DEPTH)/$(MODULE)/scripts/now))
endif

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

release_export:: $(SOURCE_RELEASE_XP_DIR)/include

release_export:: $(EXPORTS)
	$(INSTALL) -m 444 $^ $(SOURCE_RELEASE_XP_DIR)/include
endif




################################################################################

-include $(DEPENDENCIES)

ifneq (,$(filter-out OS2 WIN%,$(OS_TARGET)))
# Can't use sed because of its 4000-char line length limit, so resort to perl
PERL_DEPENDENCIES_PROGRAM =                                                   \
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
	    }

.DEFAULT:
	@$(PERL) -e '$(PERL_DEPENDENCIES_PROGRAM)'
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
#on NT, the preceding touch command creates a read-only file !?!?!
#which is why we have to explicitly chmod it.
	$(MKDEPEND) -p$(OBJDIR_NAME)/ -o'$(OBJ_SUFFIX)' -f$(MKDEPENDENCIES) \
$(NOMD_CFLAGS) $(YOPT) $(CSRCS) $(CPPSRCS) $(ASFILES)

$(MKDEPEND):: $(MKDEPEND_DIR)/*.c $(MKDEPEND_DIR)/*.h
	$(MAKE) -C $(MKDEPEND_DIR)

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

#
# HACK ALERT
#
# The only purpose of this rule is to pass Mozilla's Tinderbox depend
# builds (http://tinderbox.mozilla.org/showbuilds.cgi).  Mozilla's
# Tinderbox builds NSS continuously as part of the Mozilla client.
# Because NSS's make depend is not implemented, whenever we change
# an NSS header file, the depend build does not recompile the NSS
# files that depend on the header.
#
# This rule makes all the objects depend on a dummy header file.
# Check in a change to this dummy header file to force the depend
# build to recompile everything.
#
# This rule should be removed when make depend is implemented.
#

DUMMY_DEPEND = $(CORE_DEPTH)/coreconf/coreconf.dep

$(filter $(OBJDIR)/%$(OBJ_SUFFIX),$(OBJS)): $(OBJDIR)/%$(OBJ_SUFFIX): $(DUMMY_DEPEND)

# END OF HACK

################################################################################
# Special gmake rules.
################################################################################

#
# Re-define the list of default suffixes, so gmake won't have to churn through
# hundreds of built-in suffix rules for stuff we don't need.
#
.SUFFIXES:
.SUFFIXES: .out .a .ln .o .obj .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .class .java .html .asm .dep

#
# Don't delete these files if we get killed.
#
.PRECIOUS: .java $(JDK_HEADERS) $(JDK_STUBS) $(JRI_HEADERS) $(JRI_STUBS) $(JMC_HEADERS) $(JMC_STUBS) $(JNI_HEADERS)

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot clean clobber clobber_all export install libs program realclean release $(OBJDIR)

