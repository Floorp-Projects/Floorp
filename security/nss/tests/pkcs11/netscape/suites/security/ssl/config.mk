#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# Set the LDFLAGS value to encompass all normal link options, all     #
# library names, and all special system linking options               #
#######################################################################

LDFLAGS = \
	$(DYNAMIC_LIB_PATH) \
	$(LDOPTS) \
	$(LIBSECTOOLS) \
	$(LIBSSL) \
	$(LIBPKCS7) \
	$(LIBCERT) \
	$(LIBKEY) \
	$(LIBSECMOD) \
	$(LIBCRYPTO) \
	$(LIBSECUTIL) \
	$(LIBHASH) \
	$(LIBDBM) \
	$(LIBPLC) \
	$(LIBPLDS) \
	$(LIBPR) \
	$(DLLSYSTEM)

#######################################################################
# Adjust specific variables for all platforms                         #
#######################################################################
 


