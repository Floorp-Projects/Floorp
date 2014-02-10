#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#
#  Override TARGETS variable so that only static libraries
#  are specifed as dependencies within rules.mk.
#

TARGETS        = $(LIBRARY)
SHARED_LIBRARY =
IMPORT_LIBRARY =
PROGRAM        =

ifdef NSS_PKIX_NO_LDAP
LDAP_HEADERS =
LDAP_CSRCS =
else
LDAP_HEADERS = \
	pkix_pl_ldapt.h \
	pkix_pl_ldapcertstore.h \
	pkix_pl_ldapresponse.h \
	pkix_pl_ldaprequest.h \
	pkix_pl_ldapdefaultclient.h \
 	$(NULL)
 
LDAP_CSRCS = \
	pkix_pl_ldaptemplates.c \
	pkix_pl_ldapcertstore.c \
	pkix_pl_ldapresponse.c \
	pkix_pl_ldaprequest.c \
	pkix_pl_ldapdefaultclient.c \
 	$(NULL)
endif
