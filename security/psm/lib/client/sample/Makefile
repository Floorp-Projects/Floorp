#! gmake
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape security libraries.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1994-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

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

ifneq ($(OS_ARCH), WINNT)
ifeq ($(OS_ARCH), Linux)
# On linux, we link with libstdc++
CPLUSPLUSRUNTIME = -L /usr/lib -lstdc++ -lm
else
# libC, presumably, is what we must link with elsewhere
CPLUSPLUSRUNTIME = -lC -lm
endif
endif

ifeq ($(OS_ARCH), SunOS)
ifeq ($(OS_RELEASE), 5.5.1)
OS_LIBS += -ldl -lsocket -lnsl -lthread -lposix4
endif
ifeq ($(OS_RELEASE), 5.6)
OS_LIBS += -ldl -lsocket -lnsl -lthread -lposix4
endif
endif

ifeq ($(OS_ARCH), Linux)
ifdef USE_PTHREADS
# Replace OS_LIBS, because the order of libpthread, libdl, and libc are
# very important. Otherwise you get horrible crashes.
OS_LIBS = -lpthread -ldl -lc
endif
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

ifeq ($(OS_ARCH), WINNT)
LDFLAGS += /NODEFAULTLIB:library
endif

EXTRA_LIBS = \
	$(DIST)/lib/$(LIB_PREFIX)cmt.$(LIB_SUFFIX) \
	$(DIST)/lib/$(LIB_PREFIX)protocol.$(LIB_SUFFIX) \
	$(NULL)

ifeq ($(OS_ARCH), WINNT)
EXTRA_LIBS += 	wsock32.lib \
		winmm.lib \
		$(NULL)
endif

link:
	if test -f $(PROGRAM); then	\
		echo "rm $(PROGRAM)";	\
		rm $(PROGRAM);		\
	fi;				\
	$(MAKE)				\

build_sample:
ifneq ($(OS_ARCH),WINNT)
	cd $(CORE_DEPTH)/coreconf; $(MAKE)
endif
	cd $(CORE_DEPTH)/security; $(MAKE) import; $(MAKE) export
	cd ../../protocol; $(MAKE)
	cd ..; $(MAKE)
	$(MAKE)

