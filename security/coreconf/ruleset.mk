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
#                                                                     #
# Parameters to this makefile (set these in this file):               #
#                                                                     #
# a)                                                                  #
#	TARGETS	-- the target to create                               #
#			(defaults to $LIBRARY $PROGRAM)               #
# b)                                                                  #
#	DIRS	-- subdirectories for make to recurse on              #
#			(the 'all' rule builds $TARGETS $DIRS)        #
# c)                                                                  #
#	CSRCS, CPPSRCS -- .c and .cpp files to compile                #
#			(used to define $OBJS)                        #
# d)                                                                  #
#	PROGRAM	-- the target program name to create from $OBJS       #
#			($OBJDIR automatically prepended to it)       #
# e)                                                                  #
#	LIBRARY	-- the target library name to create from $OBJS       #
#			($OBJDIR automatically prepended to it)       #
# f)                                                                  #
#	JSRCS	-- java source files to compile into class files      #
#			(if you don't specify this it will default    #
#			 to *.java)                                   #
# g)                                                                  #
#	PACKAGE	-- the package to put the .class files into           #
#			(e.g. netscape/applet)                        #
#			(NOTE: the default definition for this may be #
#                              overridden if "jdk.mk" is included)    #
# h)                                                                  #
#	JMC_EXPORT -- java files to be exported for use by JMC_GEN    #
#			(this is a list of Class names)               #
# i)                                                                  #
#	JRI_GEN	-- files to run through javah to generate headers     #
#                  and stubs                                          #
#			(output goes into the _jri sub-dir)           #
# j)                                                                  #
#	JMC_GEN	-- files to run through jmc to generate headers       #
#                  and stubs                                          #
#			(output goes into the _jmc sub-dir)           #
# k)                                                                  #
#	JNI_GEN	-- files to run through javah to generate headers     #
#			(output goes into the _jni sub-dir)           #
#                                                                     #
#######################################################################

#
#  At this time, the CPU_TAG value is actually assigned.
#

CPU_TAG =

#
# When the processor is NOT 386-based on Windows NT, override the
# value of $(CPU_TAG).
#
ifeq ($(OS_ARCH), WINNT)
	ifneq ($(CPU_ARCH),x386)
		CPU_TAG = _$(CPU_ARCH)
	endif
endif

#
# Always set CPU_TAG on Linux.
#
ifeq ($(OS_ARCH), Linux)
	CPU_TAG = _$(CPU_ARCH)
endif

#
#  At this time, the COMPILER_TAG value is actually assigned.
#

ifndef COMPILER_TAG
ifneq ($(DEFAULT_COMPILER), $(CC))
#
# Temporary define for the Client; to be removed when binary release is used
#
	ifdef MOZILLA_CLIENT
		COMPILER_TAG =
	else
		COMPILER_TAG = _$(CC)
	endif
else
	COMPILER_TAG =
endif
endif

#
#  At this time, a default value of $(CC) is assigned to MKPROG.
#

ifeq ($(MKPROG),)
	MKPROG = $(CC)
endif

#
# This makefile contains rules for building the following kinds of
# objects:
# - (1) LIBRARY: a static (archival) library
# - (2) SHARED_LIBRARY: a shared (dynamic link) library
# - (3) IMPORT_LIBRARY: an import library, used only on Windows
# - (4) PURE_LIBRARY: a library for Purify
# - (5) PROGRAM: an executable binary
#
# NOTE:  The names of libraries can be generated by simply specifying
# LIBRARY_NAME (and LIBRARY_VERSION in the case of non-static libraries).
#

ifdef LIBRARY_NAME
	ifeq ($(OS_ARCH), WINNT)
		#
		# Win16 requires library names conforming to the 8.3 rule.
		# other platforms do not.
		#
		LIBRARY        = $(OBJDIR)/$(LIBRARY_NAME).lib
		ifeq ($(OS_TARGET), WIN16)
			SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)16$(JDK_DEBUG_SUFFIX).dll
			IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)16$(JDK_DEBUG_SUFFIX).lib
		else
			SHARED_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32$(JDK_DEBUG_SUFFIX).dll
			IMPORT_LIBRARY = $(OBJDIR)/$(LIBRARY_NAME)$(LIBRARY_VERSION)32$(JDK_DEBUG_SUFFIX).lib
		endif
	else
		LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME).$(LIB_SUFFIX)
		ifeq ($(OS_ARCH)$(OS_RELEASE), AIX4.1)
			SHARED_LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)_shr$(JDK_DEBUG_SUFFIX).a
		else
			SHARED_LIBRARY = $(OBJDIR)/lib$(LIBRARY_NAME)$(LIBRARY_VERSION)$(JDK_DEBUG_SUFFIX).$(DLL_SUFFIX)
		endif

		ifdef HAVE_PURIFY
			ifdef DSO_BACKEND
				PURE_LIBRARY = $(OBJDIR)/purelib$(LIBRARY_NAME)$(LIBRARY_VERSION)$(JDK_DEBUG_SUFFIX).$(DLL_SUFFIX)
			else
				PURE_LIBRARY = $(OBJDIR)/purelib$(LIBRARY_NAME).$(LIB_SUFFIX)
			endif
		endif
	endif
endif

#
# Common rules used by lots of makefiles...
#

ifdef PROGRAM
	PROGRAM := $(addprefix $(OBJDIR)/, $(PROGRAM)$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX))
endif

ifdef PROGRAMS
	PROGRAMS := $(addprefix $(OBJDIR)/, $(PROGRAMS:%=%$(JDK_DEBUG_SUFFIX)$(PROG_SUFFIX)))
endif

ifndef TARGETS
	ifeq ($(OS_ARCH), WINNT)
		TARGETS = $(LIBRARY) $(SHARED_LIBRARY) $(IMPORT_LIBRARY) $(PROGRAM)
	else
		TARGETS = $(LIBRARY) $(SHARED_LIBRARY)
		ifdef HAVE_PURIFY
			TARGETS += $(PURE_LIBRARY)
		endif
		TARGETS += $(PROGRAM)
	endif
endif

ifndef OBJS
	SIMPLE_OBJS = $(JRI_STUB_CFILES) \
		$(addsuffix $(OBJ_SUFFIX), $(JMC_GEN)) \
		$(CSRCS:.c=$(OBJ_SUFFIX)) \
		$(CPPSRCS:.cpp=$(OBJ_SUFFIX)) \
		$(ASFILES:$(ASM_SUFFIX)=$(OBJ_SUFFIX))
	OBJS = $(addprefix $(OBJDIR)/$(PROG_PREFIX), $(SIMPLE_OBJS))
endif

ifeq ($(OS_TARGET), WIN16)
	comma   := ,
	empty   :=
	space   := $(empty) $(empty)
	W16OBJS := $(subst $(space),$(comma)$(space),$(strip $(OBJS)))
	W16TEMP  = $(OS_LIBS) $(EXTRA_LIBS)
	ifeq ($(strip $(W16TEMP)),)
		W16LIBS =
	else
		W16LIBS := library $(subst $(space),$(comma)$(space),$(strip $(W16TEMP)))
	endif
endif

ifeq ($(OS_ARCH),WINNT)
	ifneq ($(OS_TARGET), WIN16)
		OBJS += $(RES)
	endif
	MAKE_OBJDIR		= $(INSTALL) -D $(OBJDIR)
else
	define MAKE_OBJDIR
	if test ! -d $(@D); then rm -rf $(@D); $(NSINSTALL) -D $(@D); fi
	endef
endif

ifndef PACKAGE
	PACKAGE = .
endif

ALL_TRASH :=	$(TARGETS) $(OBJS) $(OBJDIR) LOGS TAGS $(GARBAGE) \
		$(NOSUCHFILE) $(JDK_HEADER_CFILES) $(JDK_STUB_CFILES) \
		$(JRI_HEADER_CFILES) $(JRI_STUB_CFILES) $(JNI_HEADERS) $(JMC_STUBS) \
		$(JMC_HEADERS) $(JMC_EXPORT_FILES) so_locations \
		_gen _jmc _jri _jni _stubs \
		$(wildcard $(JAVA_DESTPATH)/$(PACKAGE)/*.class)

ifdef JDIRS
	ALL_TRASH += $(addprefix $(JAVA_DESTPATH)/,$(JDIRS))
endif

ifdef NSBUILDROOT
	JDK_GEN_DIR  = $(SOURCE_XP_DIR)/_gen
	JMC_GEN_DIR  = $(SOURCE_XP_DIR)/_jmc
	JNI_GEN_DIR  = $(SOURCE_XP_DIR)/_jni
	JRI_GEN_DIR  = $(SOURCE_XP_DIR)/_jri
	JDK_STUB_DIR = $(SOURCE_XP_DIR)/_stubs
else
	JDK_GEN_DIR  = _gen
	JMC_GEN_DIR  = _jmc
	JNI_GEN_DIR  = _jni
	JRI_GEN_DIR  = _jri
	JDK_STUB_DIR = _stubs
endif

#
# If this is an "official" build, try to build everything.
# I.e., don't exit on errors.
#

ifdef BUILD_OFFICIAL
	EXIT_ON_ERROR		= +e
	CLICK_STOPWATCH		= date
else
	EXIT_ON_ERROR		= -e
	CLICK_STOPWATCH		= true
endif

ifdef REQUIRES
ifeq ($(OS_TARGET),WIN16)
	INCLUDES        += -I$(SOURCE_XP_DIR)/public/win16
else
	MODULE_INCLUDES := $(addprefix -I$(SOURCE_XP_DIR)/public/, $(REQUIRES))
	INCLUDES        += $(MODULE_INCLUDES)
	ifeq ($(MODULE), sectools)
		PRIVATE_INCLUDES := $(addprefix -I$(SOURCE_XP_DIR)/private/, $(REQUIRES))
		INCLUDES         += $(PRIVATE_INCLUDES)
	endif
endif
endif

ifdef SYSTEM_INCL_DIR
	YOPT = -Y$(SYSTEM_INCL_DIR)
endif

ifdef DIRS
	LOOP_OVER_DIRS		=						\
		@for directory in $(DIRS); do					\
			if test -d $$directory; then				\
				set $(EXIT_ON_ERROR);				\
				echo "cd $$directory; $(MAKE) $@";		\
				$(MAKE) -C $$directory $@;			\
				set +e;						\
			else							\
				echo "Skipping non-directory $$directory...";	\
			fi;							\
			$(CLICK_STOPWATCH);					\
		done
endif



# special stuff for tests rule in rules.mk

ifneq ($(OS_ARCH),WINNT)
	REGDATE = $(subst \ ,, $(shell perl  $(CORE_DEPTH)/$(MODULE)/scripts/now))
else
	REGCOREDEPTH = $(subst \\,/,$(CORE_DEPTH))
	REGDATE = $(subst \ ,, $(shell perl  $(CORE_DEPTH)/$(MODULE)/scripts/now))
endif

#
# export control policy patcher program and arguments
#

PLCYPATCH     = $(SOURCE_BIN_DIR)/plcypatch$(PROG_SUFFIX)

DOMESTIC_POLICY = -us
EXPORT_POLICY   = -ex
FRANCE_POLICY   = -fr

ifeq ($(POLICY), domestic)
	PLCYPATCH_ARGS = $(DOMESTIC_POLICY)
else
	ifeq ($(POLICY), export)
		PLCYPATCH_ARGS = $(EXPORT_POLICY)
	else
		ifeq ($(POLICY), france)
			PLCYPATCH_ARGS = $(FRANCE_POLICY)
		else
			PLCYPATCH_ARGS =
		endif
	endif
endif
