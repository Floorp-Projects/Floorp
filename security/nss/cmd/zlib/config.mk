#
#           CONFIDENTIAL AND PROPRIETARY SOURCE CODE OF
#              NETSCAPE COMMUNICATIONS CORPORATION
# Copyright ¿ 1996, 1997 Netscape Communications Corporation.  All Rights
# Reserved.  Use of this Source Code is subject to the terms of the
# applicable license agreement from Netscape Communications Corporation.
# The copyright notice(s) in this Source Code does not indicate actual or
# intended publication of this Source Code.
#

#
#  Currently, override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

TARGETS        = $(LIBRARY)
SHARED_LIBRARY =
IMPORT_LIBRARY =
PURE_LIBRARY   =
PROGRAM        =

#######################################################################
# Set the LDFLAGS value to encompass all normal link options, all     #
# library names, and all special system linking options               #
#######################################################################

SECTOOLS_LIBS = $(LDOPTS) $(LIBSSL) $(LIBPKCS7) $(LIBCERT) $(LIBKEY) $(LIBSECMOD) $(LIBCRYPTO) $(LIBSECUTIL) $(LIBSECMOD) $(LIBSSL) $(LIBPKCS7) $(LIBCERT) $(LIBKEY) $(LIBCRYPTO) $(LIBSECUTIL) $(LIBHASH) $(LIBDBM) $(DLLPLC) $(DLLPLDS) $(DLLPR) $(DLLSYSTEM)

ifeq ($(OS_ARCH),AIX)
	OS_LIBS += $(SECTOOLS_LIBS)
endif

ifeq ($(OS_ARCH),NCR)
	OS_LIBS += $(SECTOOLS_LIBS)
endif

ifeq ($(OS_ARCH),SCO_SV)
	OS_LIBS += $(SECTOOLS_LIBS)
endif

LDFLAGS += $(SECTOOLS_LIBS)




