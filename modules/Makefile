#! gmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.


DEPTH		= ..

include $(DEPTH)/config/config.mk

DIRS		= \
		  libhook \
		  libreg \
		  libpref \
		  libutil \
		  libimg \
		  libfont \
		  rdf \
		  xml \
		  schedulr \
		  li \
		  progress \
		  plugin \
		  $(NULL)

ifndef MOZ_NATIVE_ZLIB
DIRS		+= zlib
endif

ifndef MOZ_SECURITY
DIRS		+= security
endif

ifdef MOZ_OJI
DIRS		+= oji
endif

ifdef MOZ_EDITOR

ifdef JAVA_OR_NSJVM	# change later to JAVA_OR_OJI
DIRS		+= edtplug
endif

DIRS		+= spellchk

endif # MOZ_EDITOR

ifeq ($(OS_ARCH),OS2)
DIRS		+= spellchk
endif

include $(DEPTH)/config/rules.mk
