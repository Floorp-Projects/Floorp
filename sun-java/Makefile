#!gmake
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

DIRS		= md-include $(AWT11DIRS) classsrc javah jtools include \
		  runtime md awt mmedia netscape jpegwrap zipwrap bn jdbc

#
# We need to build only console for stand alone java
#
ifdef MOZ_JAVA

ifeq ($(STAND_ALONE_JAVA),1)
DIRS		= javah $(AWT11DIRS) classsrc include md-include md \
		  runtime awt mmedia jpegwrap zipwrap bn jdbc netscape/console
endif
ifeq ($(AWT_11),1)
AWT11DIRS	= classsrc11
endif

else  # MOZ_JAVA
DIRS = stubs
endif # MOZ_JAVA

include $(DEPTH)/config/rules.mk

OTHER_DIRS	= javac javadoc doc javap

LOOP_OVER_OTHER_DIRS =						\
	@for d in $(OTHER_DIRS); do				\
		if test -d $$d; then				\
			set -e;					\
			echo "cd $$d; $(MAKE) $@";		\
			cd $$d; $(MAKE) $@; cd ..;		\
			set +e;					\
		else						\
			echo "Skipping non-directory $$d...";	\
		fi;						\
	done

.PHONY: $(OTHER_DIRS)

$(OTHER_DIRS)::
	@if test -d $@; then				\
		set -e;					\
		echo "cd $@; $(MAKE)";			\
		cd $@; $(MAKE);				\
		set +e;					\
	else						\
		echo "Skipping non-directory $@...";	\
	fi

other_dirs::
	+$(LOOP_OVER_OTHER_DIRS)
