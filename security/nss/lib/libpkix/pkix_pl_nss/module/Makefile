#! gmake
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

#######################################################################
# (1) Include initial platform-independent assignments (MANDATORY).   #
#######################################################################

include manifest.mn

#######################################################################
# (2) Include "global" configuration information. (OPTIONAL)          #
#######################################################################

include $(CORE_DEPTH)/coreconf/config.mk

#######################################################################
# (3) Include "component" configuration information. (OPTIONAL)       #
#######################################################################



#######################################################################
# (4) Include "local" platform-dependent assignments (OPTIONAL).      #
#######################################################################

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

#######################################################################
# (5) Execute "global" rules. (OPTIONAL)                              #
#######################################################################

include $(CORE_DEPTH)/coreconf/rules.mk

#######################################################################
# (6) Execute "component" rules. (OPTIONAL)                           #
#######################################################################



#######################################################################
# (7) Execute "local" rules. (OPTIONAL).                              #
#######################################################################


