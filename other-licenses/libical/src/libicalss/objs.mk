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
# Copyright (C) 2000 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

ICALSS_SRC_LCSRCS = \
		icalclassify.c \
		icaldirset.c \
		icalfileset.c \
		icalgauge.c \
		icalmessage.c \
		icalset.c \
		icalspanlist.c \
		icalsslexer.c \
		icalssyacc.c \
		$(NULL)

ICALSS_SRC_LEXPORTS = icalss.h

ICALSS_SRC_CSRCS := $(addprefix $(topsrcdir)/other-licenses/libical/src/libicalss/, $(ICALSS_SRC_LCSRCS))
ICALSS_SRC_EXPORTS := $(addprefix $(topsrcdir)/other-licenses/libical/src/libicalss/, $(ICALSS_SRC_LEXPORTS))
