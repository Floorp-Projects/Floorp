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
# Dont't use double-colon rules!                                      #
#######################################################################

ifndef HAVE_ALL_TARGET
all: libs
endif

autobuild:
ifeq ($(AUTOCLEAN),1)
	$(MAKE) clean
endif
	$(MAKE) all
	$(MAKE) install

platform:
	@echo $(OBJDIR_NAME)

ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
USE_NT_C_SYNTAX=1
endif

# For whatever reason, "." can't be handled using make conditionals.
# Based on automake's SUBDIRS "." handling.
ifdef DIRS
ifndef IGNORE_DIRS
ifneq (,$(filter .,$(DIRS)))
TARGETS = $(NULL)
ALL_TRASH = $(NULL)
endif

$(DIRS):
	$(IGNORE_ERROR)@if [ "$@" != "." ]; then \
		$(MAKE) -C $@ $(MAKECMDGOALS) ; \
	else \
		IGNORE_DIRS=1 $(MAKE) -C $@ $(MAKECMDGOALS) ; \
	fi
	@$(CLICK_STOPWATCH)
endif
endif

export: $(DIRS) private_export

release_export: $(DIRS)

libs program install: $(DIRS) $(TARGETS)
ifneq ($(LIBRARY),)
	$(INSTALL) -m 664 $(LIBRARY) $(SOURCE_LIB_DIR)
endif
ifneq ($(SHARED_LIBRARY),)
	$(INSTALL) -m 775 $(SHARED_LIBRARY) $(SOURCE_LIB_DIR)
ifdef MOZ_DEBUG_SYMBOLS
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(INSTALL) -m 644 $(SHARED_LIBRARY:$(DLL_SUFFIX)=pdb) $(SOURCE_LIB_DIR)
endif
endif
endif
ifneq ($(IMPORT_LIBRARY),)
	$(INSTALL) -m 775 $(IMPORT_LIBRARY) $(SOURCE_LIB_DIR)
endif
ifneq ($(PROGRAM),)
	$(INSTALL) -m 775 $(PROGRAM) $(SOURCE_BIN_DIR)
ifdef MOZ_DEBUG_SYMBOLS
ifeq (,$(filter-out _WIN%,$(NS_USE_GCC)_$(OS_TARGET)))
	$(INSTALL) -m 644 $(PROGRAM:$(PROG_SUFFIX)=.pdb) $(SOURCE_BIN_DIR)
endif
endif
endif
ifneq ($(PROGRAMS),)
	$(INSTALL) -m 775 $(PROGRAMS) $(SOURCE_BIN_DIR)
endif

check: $(DIRS)

clean clobber: $(DIRS)
ifneq (,$(ALL_TRASH))
	rm -rf $(ALL_TRASH)
endif

realclean clobber_all: $(DIRS)
	rm -rf $(wildcard *.OBJ) dist $(ALL_TRASH)

release_clean:
	rm -rf $(SOURCE_XP_DIR)/release/$(RELEASE_MD_DIR)

release: release_clean release_export release_md

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

release_md:: $(DIRS)
ifneq ($(MD_LIB_RELEASE_FILES),)
	$(INSTALL) -m 444 $(MD_LIB_RELEASE_FILES) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_LIB_DIR)
endif
ifneq ($(MD_BIN_RELEASE_FILES),)
	$(INSTALL) -m 555 $(MD_BIN_RELEASE_FILES) $(SOURCE_RELEASE_PREFIX)/$(SOURCE_RELEASE_BIN_DIR)
endif

alltags:
	rm -f TAGS
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs etags -a
	find . -name dist -prune -o \( -name '*.[hc]' -o -name '*.cp' -o -name '*.cpp' \) -print | xargs ctags -a

.SECONDEXPANSION:
%/d:
	@$(MAKE_OBJDIR)

define PROGRAM_template

ifndef $(1)_OBJS
ifdef LIBRARY_NAME
	$(1)_OBJS := $$(patsubst $$(PROG_PREFIX)%,%,$$(patsubst %$$(PROG_SUFFIX),%,$(1)))$$(OBJ_SUFFIX)
endif
ifdef PROGRAMS
	$(1)_OBJS := $$(patsubst $$(PROG_PREFIX)%,%,$$(patsubst %$$(PROG_SUFFIX),%,$(1)))$$(OBJ_SUFFIX)
endif
ifndef $(1)_OBJS
	$(1)_OBJS := $$(OBJS)
endif
endif

$(1): $$($(1)_OBJS) $$(EXTRA_LIBS) | $$$$(@D)/d
	rm -f $$@
ifeq (,$$(filter-out _WIN%,$$(NS_USE_GCC)_$$(OS_TARGET)))
	$$(MKPROG) $$($(1)_OBJS) -Fe$$@ -link $$(LDFLAGS) $$(XLDFLAGS) $$(EXTRA_LIBS) $$(EXTRA_SHARED_LIBS) $$(OS_LIBS)
ifdef MT
	if test -f $$@.manifest; then \
		$$(MT) -NOLOGO -MANIFEST $$@.manifest -OUTPUTRESOURCE:$$@\;1; \
		rm -f $$@.manifest; \
	fi
endif	# MSVC with manifest tool
else
	$$(MKPROG) -o $$@ $$(CFLAGS) $$($(1)_OBJS) $$(LDFLAGS) $$(EXTRA_LIBS) $$(EXTRA_SHARED_LIBS) $$(OS_LIBS)
endif
endef # PROGRAM_template

ifdef PROGRAM
$(eval $(call PROGRAM_template,$(PROGRAM)))
else
ifdef PROGRAMS
$(foreach prog,$(PROGRAMS),$(eval $(call PROGRAM_template,$(prog))))
endif
endif

get_objs:
	@echo $(OBJS)

$(LIBRARY): $(OBJS) | $$(@D)/d
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
$(IMPORT_LIBRARY): $(SHARED_LIBRARY)
endif

ifdef SHARED_LIBRARY_LIBS
ifdef BUILD_TREE
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(shell $(MAKE) -C $(dir) --no-print-directory get_objs))
else
SUB_SHLOBJS = $(foreach dir,$(SHARED_LIBRARY_DIRS),$(addprefix $(dir)/,$(shell $(MAKE) -C $(dir) --no-print-directory get_objs)))
endif
endif

$(SHARED_LIBRARY): $(OBJS) $(RES) $(MAPFILE) $(SUB_SHLOBJS) | $$(@D)/d
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
$(RES): $(RESNAME) | $$(@D)/d
# The resource compiler does not understand the -U option.
ifdef NS_USE_GCC
	$(RC) $(filter-out -U%,$(DEFINES)) $(INCLUDES:-I%=--include-dir %) -o $@ $<
else
	$(RC) $(filter-out -U%,$(DEFINES)) $(INCLUDES) -Fo$@ $<
endif
	@echo $(RES) finished
endif

$(MAPFILE): $(MAPFILE_SOURCE) | $$(@D)/d
	$(PROCESS_MAP_FILE)

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

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c | $$(@D)/d
ifdef USE_NT_C_SYNTAX
	$(CC) -Fo$@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $(call core_abspath,$<)
else
	$(CC) -o $@ -c $(CSTD) $(CFLAGS) $<
endif
endif

$(PROG_PREFIX)%$(OBJ_SUFFIX): %.c | $$(@D)/d
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
$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.s | $$(@D)/d
	$(AS) -o $@ $(ASFLAGS) -c $<
endif

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.asm | $$(@D)/d
	$(AS) -Fo$@ $(ASFLAGS) -c $<

$(OBJDIR)/$(PROG_PREFIX)%$(OBJ_SUFFIX): %.S | $$(@D)/d
	$(AS) -o $@ $(ASFLAGS) -c $<

$(OBJDIR)/$(PROG_PREFIX)%: %.cpp | $$(@D)/d
ifdef USE_NT_C_SYNTAX
	$(CCC) -Fo$@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
ifdef NEED_ABSOLUTE_PATH
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $(call core_abspath,$<)
else
	$(CCC) -o $@ -c $(CXXSTD) $(CFLAGS) $(CXXFLAGS) $<
endif
endif

define compile_ccc_pattern_RULE

$$(OBJDIR)/$$(PROG_PREFIX)%$$(OBJ_SUFFIX): %.$(1) | $$$$(@D)/d
ifdef STRICT_CPLUSPLUS_SUFFIX
	echo "#line 1 \"$$<\"" | cat - $$< > $$(OBJDIR)/t_$$*.cc
	$$(CCC) -o $$@ -c $$(CXXSTD) $$(CFLAGS) $$(CXXFLAGS) $$(OBJDIR)/t_$$*.cc
	rm -f $$(OBJDIR)/t_$$*.cc
else
ifdef USE_NT_C_SYNTAX
	$$(CCC) -Fo$$@ -c $$(CXXSTD) $$(CFLAGS) $$(CXXFLAGS) $$(call core_abspath,$$<)
else
ifdef NEED_ABSOLUTE_PATH
	$$(CCC) -o $$@ -c $$(CXXSTD) $$(CFLAGS) $$(CXXFLAGS) $$(call core_abspath,$$<)
else
	$$(CCC) -o $$@ -c $$(CXXSTD) $$(CFLAGS) $$(CXXFLAGS) $$<
endif
endif
endif #STRICT_CPLUSPLUS_SUFFIX
endef # compile_ccc_pattern_RULE

$(eval $(call compile_ccc_pattern_RULE,cc))
$(eval $(call compile_ccc_pattern_RULE,cpp))

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

define copy_varlist_into_dir_RULE
ifdef $(2)
ifneq (,$$(strip $$($(2))))
$(3)/%: %
	$$(INSTALL) -m 444 $$^ $(3)

$(1): $$(addprefix $(3)/,$$($(2))) | $(3)/d

.NOTPARALLEL: $(1)
endif
else
$(1):
endif
endef # copy_varlist_into_dir_RULE

# export rule
$(eval $(call copy_varlist_into_dir_RULE,export,EXPORTS,$(SOURCE_XPHEADERS_DIR)))

# private_export rule
$(eval $(call copy_varlist_into_dir_RULE,private_export,PRIVATE_EXPORTS,$(SOURCE_XPPRIVATE_DIR)))

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

check: $(REGRESSION_SPEC) | $(TESTS_DIR)/d
	cd $(PLATFORM); \
	../$(SOURCE_MD_DIR)/bin/regress$(PROG_SUFFIX) specfile=../$(REGRESSION_SPEC) progress $(EXTRA_REGRESS_OPTIONS)
ifneq ($(BUILD_OPT),)
	$(NSINSTALL) -m 664 $(PLATFORM)/$(REGDATE).sum $(TESTS_DIR); \
	$(NSINSTALL) -m 664 $(PLATFORM)/$(REGDATE).htm $(TESTS_DIR); \
	echo "Please now make sure your results files are copied to $(TESTS_DIR), "; \
	echo "then run 'reporter specfile=$(RESULTS_DIR)/rptspec'"
endif
else
check:
	@echo "Error: you didn't specify REGRESSION_SPEC in your manifest.mn file!"
endif

# release_export rule
$(eval $(call copy_varlist_into_dir_RULE,release_export,EXPORTS,$(SOURCE_RELEASE_XP_DIR)/include))

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
.SUFFIXES: .out .a .ln .o .obj .c .cc .C .cpp .y .l .s .S .h .sh .i .pl .html .asm .dep

#
# Fake targets.  Always run these rules, even if a file/directory with that
# name already exists.
#
.PHONY: all all_platforms alltags boot clean clobber clobber_all export install libs program realclean release $(DIRS)

