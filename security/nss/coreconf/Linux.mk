#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

include $(CORE_DEPTH)/coreconf/UNIX.mk

#
# The default implementation strategy for Linux is now pthreads
#
ifneq ($(OS_TARGET),Android)
	USE_PTHREADS = 1
endif

ifeq ($(USE_PTHREADS),1)
	IMPL_STRATEGY = _PTH
endif

CC			= gcc
CCC			= g++
RANLIB			= ranlib

DEFAULT_COMPILER = gcc

ifeq ($(OS_TARGET),Android)
ifndef ANDROID_NDK
	$(error Must set ANDROID_NDK to the path to the android NDK first)
endif
ifndef ANDROID_TOOLCHAIN_VERSION
	$(error Must set ANDROID_TOOLCHAIN_VERSION to the requested version number)
endif
	ANDROID_PREFIX=$(OS_TEST)-linux-androideabi
	ANDROID_TARGET=$(ANDROID_PREFIX)-$(ANDROID_TOOLCHAIN_VERSION)
	# should autodetect which linux we are on, currently android only
	# supports linux-x86 prebuilts
	ANDROID_TOOLCHAIN=$(ANDROID_NDK)/toolchains/$(ANDROID_TARGET)/prebuilt/linux-x86
	ANDROID_SYSROOT=$(ANDROID_NDK)/platforms/android-$(OS_TARGET_RELEASE)/arch-$(OS_TEST)
	ANDROID_CC=$(ANDROID_TOOLCHAIN)/bin/$(ANDROID_PREFIX)-gcc
	ANDROID_CCC=$(ANDROID_TOOLCHAIN)/bin/$(ANDROID_PREFIX)-g++
        NSS_DISABLE_GTESTS=1
# internal tools need to be built with the native compiler
ifndef INTERNAL_TOOLS
	CC = $(ANDROID_CC) --sysroot=$(ANDROID_SYSROOT)
	CCC = $(ANDROID_CCC) --sysroot=$(ANDROID_SYSROOT)
	DEFAULT_COMPILER=$(ANDROID_PREFIX)-gcc
	ARCHFLAG = --sysroot=$(ANDROID_SYSROOT)
	DEFINES += -DNO_SYSINFO -DNO_FORK_CHECK -DANDROID
	CROSS_COMPILE = 1
endif
endif
ifeq (,$(filter-out ppc64 ppc64le,$(OS_TEST)))
	CPU_ARCH	= ppc
ifeq ($(USE_64),1)
	ARCHFLAG	= -m64
endif
else
ifeq ($(OS_TEST),alpha)
        OS_REL_CFLAGS   = -D_ALPHA_
	CPU_ARCH	= alpha
else
ifeq ($(OS_TEST),x86_64)
ifeq ($(USE_64),1)
	CPU_ARCH	= x86_64
	ARCHFLAG	= -m64
else
ifeq ($(USE_X32),1)
	CPU_ARCH	= x86_64
	ARCHFLAG	= -mx32
	64BIT_TAG	= _x32
else
	OS_REL_CFLAGS	= -Di386
	CPU_ARCH	= x86
	ARCHFLAG	= -m32
endif
endif
else
ifeq ($(OS_TEST),sparc64)
	CPU_ARCH        = sparc
else
ifeq (,$(filter-out arm% sa110,$(OS_TEST)))
	CPU_ARCH        = arm
else
ifeq (,$(filter-out parisc%,$(OS_TEST)))
	CPU_ARCH        = hppa
else
ifeq (,$(filter-out i%86,$(OS_TEST)))
	OS_REL_CFLAGS	= -Di386
	CPU_ARCH	= x86
else
ifeq ($(OS_TEST),sh4a)
	CPU_ARCH        = sh4
else
# $(OS_TEST) == m68k, ppc, ia64, sparc, s390, s390x, mips, sh3, sh4
	CPU_ARCH	= $(OS_TEST)
endif
endif
endif
endif
endif
endif
endif
endif


ifneq ($(OS_TARGET),Android)
LIBC_TAG		= _glibc
endif

ifeq ($(OS_RELEASE),2.0)
	OS_REL_CFLAGS	+= -DLINUX2_0
	MKSHLIB		= $(CC) -shared -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so) $(RPATH)
	ifdef MAPFILE
		MKSHLIB += -Wl,--version-script,$(MAPFILE)
	endif
	PROCESS_MAP_FILE = grep -v ';-' $< | \
         sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@
endif

ifdef BUILD_OPT
ifeq (11,$(ALLOW_OPT_CODE_SIZE)$(OPT_CODE_SIZE))
	OPTIMIZER = -Os
else
	OPTIMIZER = -O2
endif
ifdef MOZ_DEBUG_SYMBOLS
	ifdef MOZ_DEBUG_FLAGS
		OPTIMIZER += $(MOZ_DEBUG_FLAGS)
	else
		OPTIMIZER += -gdwarf-2
	endif
endif
endif

ifndef COMPILER_TAG
COMPILER_TAG = _$(shell $(CC) -? 2>&1 >/dev/null | sed -e 's/:.*//;1q')
CCC_COMPILER_TAG = _$(shell $(CCC) -? 2>&1 >/dev/null | sed -e 's/:.*//;1q')
endif

ifeq ($(USE_PTHREADS),1)
OS_PTHREAD = -lpthread 
endif

OS_CFLAGS		= $(DSO_CFLAGS) $(OS_REL_CFLAGS) $(ARCHFLAG) -Wall -pipe -ffunction-sections -fdata-sections -DLINUX -Dlinux -DHAVE_STRERROR
OS_LIBS			= $(OS_PTHREAD) -ldl -lc

# This tests to see if enabling the warning is possible before
# setting an option to disable it.
disable_warning=$(shell $(CC) -x c -E -Werror -W$(1) /dev/null >/dev/null 2>&1 && echo -Wno-$(1))

ifeq ($(COMPILER_TAG),_clang)
  # -Qunused-arguments : clang objects to arguments that it doesn't understand
  #    and fixing this would require rearchitecture
  OS_CFLAGS += -Qunused-arguments
  # -Wno-parentheses-equality : because clang warns about macro expansions
  OS_CFLAGS += $(call disable_warning,parentheses-equality)
  ifdef BUILD_OPT
    # clang is unable to handle glib's expansion of strcmp and similar for optimized
    # builds, so ignore the resulting errors.
    # See https://llvm.org/bugs/show_bug.cgi?id=20144
    OS_CFLAGS += $(call disable_warning,array-bounds)
    OS_CFLAGS += $(call disable_warning,unevaluated-expression)
  endif
endif

ifndef NSS_ENABLE_WERROR
  ifneq ($(OS_TARGET),Android)
    # Android lollipop generates the following warning:
    # error: call to 'sprintf' declared with attribute warning:
    #   sprintf is often misused; please use snprintf [-Werror]
    # So, just suppress -Werror entirely on Android
    NSS_ENABLE_WERROR = 0
    $(warning !!! OS_TARGET is Android, disabling -Werror)
  else
    ifeq ($(COMPILER_TAG),_clang)
      # Clang reports its version as an older gcc, but it's OK
      NSS_ENABLE_WERROR = 1
    else
      NSS_ENABLE_WERROR := $(shell \
        [ `$(CC) -dumpversion | cut -f 1 -d . -` -eq 4 -a \
          `$(CC) -dumpversion | cut -f 2 -d . -` -ge 8 -o \
          `$(CC) -dumpversion | cut -f 1 -d . -` -ge 5 ] && \
        echo 1 || echo 0)
      ifneq ($(NSS_ENABLE_WERROR),1)
        $(warning !!! Unable to find gcc 4.8 or greater, disabling -Werror)
      endif
    endif
  endif
  export NSS_ENABLE_WERROR
endif

ifeq (1,$(NSS_ENABLE_WERROR))
  OS_CFLAGS += -Werror
else
  # Old versions of gcc (< 4.8) don't support #pragma diagnostic in functions.
  # Use this to disable use of that #pragma and the warnings it suppresses.
  OS_CFLAGS += -DNSS_NO_GCC48
endif

ifdef USE_PTHREADS
	DEFINES		+= -D_REENTRANT
endif

ARCH			= linux

DSO_CFLAGS		= -fPIC
DSO_LDOPTS		= -shared $(ARCHFLAG) -Wl,--gc-sections
# The linker on Red Hat Linux 7.2 and RHEL 2.1 (GNU ld version 2.11.90.0.8)
# incorrectly reports undefined references in the libraries we link with, so
# we don't use -z defs there.
ZDEFS_FLAG		= -Wl,-z,defs
DSO_LDOPTS		+= $(if $(findstring 2.11.90.0.8,$(shell ld -v)),,$(ZDEFS_FLAG))
LDFLAGS			+= $(ARCHFLAG)

# On Maemo, we need to use the -rpath-link flag for even the standard system
# library directories.
ifdef _SBOX_DIR
LDFLAGS			+= -Wl,-rpath-link,/usr/lib:/lib
endif

# INCLUDES += -I/usr/include -Y/usr/include/linux
G++INCLUDES		= -I/usr/include/g++

#
# Always set CPU_TAG on Linux.
#
CPU_TAG = _$(CPU_ARCH)

#
# On Linux 2.6 or later, build libfreebl3.so with no NSPR and libnssutil3.so
# dependencies by default.  Set FREEBL_NO_DEPEND to 0 in the environment to
# override this.
#
ifneq ($(OS_TARGET),Android)
ifeq (2.6,$(firstword $(sort 2.6 $(OS_RELEASE))))
ifndef FREEBL_NO_DEPEND
FREEBL_NO_DEPEND = 1
FREEBL_LOWHASH = 1
endif
endif
endif

USE_SYSTEM_ZLIB = 1
ZLIB_LIBS = -lz

# The -rpath '$$ORIGIN' linker option instructs this library to search for its
# dependencies in the same directory where it resides.
ifeq ($(BUILD_SUN_PKG), 1)
ifeq ($(USE_64), 1)
RPATH = -Wl,-rpath,'$$ORIGIN:/opt/sun/private/lib64:/opt/sun/private/lib'
else
RPATH = -Wl,-rpath,'$$ORIGIN:/opt/sun/private/lib'
endif
endif

OS_REL_CFLAGS   += -DLINUX2_1
MKSHLIB         = $(CC) $(DSO_LDOPTS) -Wl,-soname -Wl,$(@:$(OBJDIR)/%.so=%.so) $(RPATH)

ifdef MAPFILE
	MKSHLIB += -Wl,--version-script,$(MAPFILE)
endif
PROCESS_MAP_FILE = grep -v ';-' $< | \
        sed -e 's,;+,,' -e 's; DATA ;;' -e 's,;;,,' -e 's,;.*,;,' > $@

ifeq ($(OS_RELEASE),2.4)
DEFINES += -DNO_FORK_CHECK
endif
