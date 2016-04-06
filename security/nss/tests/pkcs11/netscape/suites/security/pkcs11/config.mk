#
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Adjust specific variables for all platforms                         #
#######################################################################
OS_CFLAGS += -DNSPR20=1

#######################################################################
# Set the LDFLAGS value to encompass all normal link options, all     #
# library names, and all special system linking options               #
#######################################################################

LDFLAGS = $(LDOPTS) $(LIBSECMOD) $(LIBHASH) $(LIBCERT) $(LIBKEY) \
$(LIBCRYPTO) $(LIBSECUTIL) $(LIBDBM) $(LIBPLC) $(LIBPLDS) $(LIBPR) \
$(LIBSECTOOLS) $(DLLSYSTEM)

# These are the libraries from the PKCS #5 suite:
#LDFLAGS = $(LDOPTS) $(LIBSECTOOLS) $(LIBSSL) $(LIBPKCS7) $(LIBCERT) $(LIBKEY) $(LIBSECMOD) $(LIBCRYPTO) $(LIBSECUTIL) $(LIBSECMOD) $(LIBSSL) $(LIBPKCS7) $(LIBCERT) $(LIBKEY) $(LIBCRYPTO) $(LIBSECUTIL) $(LIBHASH) $(LIBDBM) $(LIBPLDS) $(LIBPLC) $(LIBPR) $(DLLSYSTEM)

#######################################################################
# Set the TARGETS value to build one executable from each object file #
#######################################################################

# TARGETS = $(OBJS:$(OBJ_SUFFIX)=$(PROG_SUFFIX))

