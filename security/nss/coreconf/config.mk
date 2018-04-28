#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Configuration information for building in the "Core Components" source module

#######################################################################
# [1.0] Master "Core Components" source and release <architecture>    #
#       tags                                                          #
#######################################################################
ifndef MK_ARCH
include $(CORE_DEPTH)/coreconf/arch.mk
endif

#######################################################################
# [2.0] Master "Core Components" default command macros               #
#       (NOTE: may be overridden in $(OS_TARGET)$(OS_RELEASE).mk)     #
#######################################################################
ifndef MK_COMMAND
include $(CORE_DEPTH)/coreconf/command.mk
endif

#######################################################################
# [3.0] Master "Core Components" <architecture>-specific macros       #
#       (dependent upon <architecture> tags)                          #
#                                                                     #
#       We are moving towards just having a $(OS_TARGET).mk file      #
#       as opposed to multiple $(OS_TARGET)$(OS_RELEASE).mk files,    #
#       one for each OS release.                                      #
#######################################################################

TARGET_OSES = FreeBSD BSD_OS NetBSD OpenUNIX OS2 QNX Darwin BeOS OpenBSD \
              AIX RISCOS WINNT WIN95 Linux Android

ifeq (,$(filter-out $(TARGET_OSES),$(OS_TARGET)))
include $(CORE_DEPTH)/coreconf/$(OS_TARGET).mk
else
ifeq ($(OS_TARGET),SunOS)
include $(CORE_DEPTH)/coreconf/SunOS5.mk
else
include $(CORE_DEPTH)/coreconf/$(OS_TARGET)$(OS_RELEASE).mk
endif
endif

#######################################################################
# [4.0] Master "Core Components" source and release <platform> tags   #
#       (dependent upon <architecture> tags)                          #
#######################################################################
PLATFORM = $(OBJDIR_NAME)

#######################################################################
# [5.0] Master "Core Components" release <tree> tags                  #
#       (dependent upon <architecture> tags)                          #
#######################################################################
ifndef MK_TREE
include $(CORE_DEPTH)/coreconf/tree.mk
endif

#######################################################################
# [6.0] Master "Core Components" source and release <component> tags  #
#       NOTE:  A component is also called a module or a subsystem.    #
#       (dependent upon $(MODULE) being defined on the                #
#        command line, as an environment variable, or in individual   #
#        makefiles, or more appropriately, manifest.mn)               #
#######################################################################
ifndef MK_MODULE
include $(CORE_DEPTH)/coreconf/module.mk
endif

#######################################################################
# [7.0] Master "Core Components" release <version> tags               #
#       (dependent upon $(MODULE) being defined on the                #
#        command line, as an environment variable, or in individual   #
#        makefiles, or more appropriately, manifest.mn)               #
#######################################################################
ifndef MK_VERSION
include $(CORE_DEPTH)/coreconf/version.mk
endif

#######################################################################
# [8.0] Master "Core Components" macros to figure out                 #
#       binary code location                                          #
#       (dependent upon <platform> tags)                              #
#######################################################################
ifndef MK_LOCATION
include $(CORE_DEPTH)/coreconf/location.mk
endif

#######################################################################
# [9.0] Master "Core Components" <component>-specific source path     #
#       (dependent upon <user_source_tree>, <source_component>,       #
#        <version>, and <platform> tags)                              #
#######################################################################
ifndef MK_SOURCE
include $(CORE_DEPTH)/coreconf/source.mk
endif

#######################################################################
# [10.0] Master "Core Components" include switch for support header   #
#        files                                                        #
#        (dependent upon <tree>, <component>, <version>,              #
#         and <platform> tags)                                        #
#######################################################################
ifndef MK_HEADERS
include $(CORE_DEPTH)/coreconf/headers.mk
endif

#######################################################################
# [11.0] Master "Core Components" for computing program prefixes      #
#######################################################################
ifndef MK_PREFIX
include $(CORE_DEPTH)/coreconf/prefix.mk
endif

#######################################################################
# [12.0] Master "Core Components" for computing program suffixes      #
#        (dependent upon <architecture> tags)                         #
#######################################################################
ifndef MK_SUFFIX
include $(CORE_DEPTH)/coreconf/suffix.mk
endif

#######################################################################
# [13.0] Master "Core Components" for defining JDK                    #
#        (dependent upon <architecture>, <source>, and <suffix>  tags)#
#######################################################################
ifdef NS_USE_JDK
include $(CORE_DEPTH)/coreconf/jdk.mk
endif

#######################################################################
# [14.0] Master "Core Components" rule set                            #
#######################################################################
ifndef MK_RULESET
include $(CORE_DEPTH)/coreconf/ruleset.mk
endif

#######################################################################
# [15.0] Dependencies.
#######################################################################

-include $(MKDEPENDENCIES)

#######################################################################
# [16.0] Global environ ment defines
#######################################################################

ifdef NSS_ALLOW_UNSUPPORTED_CRITICAL
DEFINES += -DNSS_ALLOW_UNSUPPORTED_CRITICAL
endif

ifdef BUILD_LIBPKIX_TESTS
DEFINES += -DBUILD_LIBPKIX_TESTS
endif

ifdef NSS_DISABLE_LIBPKIX
DEFINES += -DNSS_DISABLE_LIBPKIX
endif

ifdef NSS_DISABLE_DBM
DEFINES += -DNSS_DISABLE_DBM
endif

ifdef NSS_DISABLE_CHACHAPOLY
DEFINES += -DNSS_DISABLE_CHACHAPOLY
endif

ifdef NSS_PKIX_NO_LDAP
DEFINES += -DNSS_PKIX_NO_LDAP
endif

# FIPS support requires startup tests to be executed at load time of shared modules.
# For performance reasons, these tests are disabled by default.
# When compiling binaries that must support FIPS mode,
# you should define NSS_FORCE_FIPS
#
# NSS_NO_INIT_SUPPORT is always defined on platforms that don't support
# executing the startup tests at library load time.
ifndef NSS_FORCE_FIPS
DEFINES += -DNSS_NO_INIT_SUPPORT
endif

ifdef NSS_SEED_ONLY_DEV_URANDOM
DEFINES += -DSEED_ONLY_DEV_URANDOM
endif

# Avoid building object leak test code for optimized library
ifndef BUILD_OPT
ifdef PKIX_OBJECT_LEAK_TEST
DEFINES += -DPKIX_OBJECT_LEAK_TEST
endif
endif

# This allows all library and tools code to use the util function
# implementations directly from libnssutil3, rather than the wrappers
# in libnss3 which are present for binary compatibility only
DEFINES += -DUSE_UTIL_DIRECTLY
USE_UTIL_DIRECTLY = 1

# Build with NO_NSPR_10_SUPPORT to avoid using obsolete NSPR features
DEFINES += -DNO_NSPR_10_SUPPORT

# Hide old, deprecated, TLS cipher suite names when building NSS
DEFINES += -DSSL_DISABLE_DEPRECATED_CIPHER_SUITE_NAMES
