# 
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Netscape Security Services for Java.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998-2000
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

#######################################################################
# Adjust variables for component library linkage on some platforms    #
#######################################################################

#
# AIX platforms
#

ifeq ($(OS_ARCH),AIX)
	LDOPTS += -blibpath:.:$(PWD)/$(SOURCE_LIB_DIR):/usr/lib/threads:/usr/lpp/xlC/lib:/usr/lib:/lib 
endif

#
# HP/UX platforms
#

ifeq ($(OS_ARCH), HP-UX)
	LDOPTS += -Wl,+s,+b,$(PWD)/$(SOURCE_LIB_DIR)
endif

#
# IRIX platforms
#

ifeq ($(OS_ARCH), IRIX)
	LDOPTS += -rpath $(PWD)/$(SOURCE_LIB_DIR)
endif

#
# OSF 1 platforms
#

ifeq ($(OS_ARCH), OSF1)
	LDOPTS += -rpath $(PWD)/$(SOURCE_LIB_DIR) -lpthread
endif

#
# Solaris platforms
#     NOTE:  Disable optimization on SunOS4.1.3
#

ifeq ($(OS_ARCH), SunOS)
	ifneq ($(OS_RELEASE), 4.1.3_U1)
		ifdef NS_USE_GCC
			LDOPTS += -Xlinker -R -Xlinker $(PWD)/$(SOURCE_LIB_DIR)
		else
			LDOPTS += -R $(PWD)/$(SOURCE_LIB_DIR)
		endif
	else
		OPTIMIZER =
	endif
endif

#
# Windows platforms
#

ifeq ($(OS_ARCH), WINNT)
	LDOPTS    += -NOLOGO -DEBUG -DEBUGTYPE:CV -INCREMENTAL:NO
endif

