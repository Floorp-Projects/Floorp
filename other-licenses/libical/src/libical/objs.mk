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

ICAL_SRC_LCSRCS = \
                icalarray.c \
                icalattendee.c \
                icalcomponent.c \
                icalderivedparameter.c \
                icalderivedproperty.c \
                icalderivedvalue.c \
                icalduration.c \
                icalenums.c \
                icalerror.c \
                icallangbind.c \
                icallexer.c \
                icalmemory.c \
                icalmime.c \
                icalparameter.c \
                icalparser.c \
                icalperiod.c \
                icalproperty.c \
                icalrecur.c \
                icalrestriction.c \
                icaltime.c \
                icaltimezone.c \
                icaltypes.c \
                icalvalue.c \
                icalyacc.c \
                pvl.c \
                sspm.c \
                caldate.c \
		$(NULL)

ICAL_SRC_LEXPORTS = ical.h

ICAL_SRC_CSRCS := $(addprefix $(topsrcdir)/other-licenses/libical/src/libical/, $(ICAL_SRC_LCSRCS))
ICAL_SRC_EXPORTS := $(addprefix $(topsrcdir)/other-licenses/libical/src/libical/, $(ICAL_SRC_LEXPORTS))