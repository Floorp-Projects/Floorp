#!gmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 

DEPTH=../..

LIBRARY_NAME = privacy

CSRCS = privacy.c 

ifdef MOZ_TRANSACTION_RECEIPTS
DEFINES += -DTRANSACTION_RECEIPTS
CSRCS += receipt.c
endif

CPPSRCS =				\
	$(NULL)

MODULE = privacy

EXPORTS = \
	$(NULL)

REQUIRES = privacy network layer pref style lay rdf nspr util img js htmldlgs

include $(DEPTH)/config/config.mk

TARGETS = $(LIBRARY)

include $(DEPTH)/config/rules.mk
