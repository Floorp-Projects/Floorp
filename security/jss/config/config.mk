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
# The Original Code is the Netscape Security Services for Java.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
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

#
# Configuration information unique to the "sectools" component
#


#######################################################################
#  Local "sectools" component library link options                    #
#######################################################################

include $(CORE_DEPTH)/$(MODULE)/config/linkage.mk

#######################################################################
#  Local "sectools" component STATIC system library names             #
#######################################################################

include $(CORE_DEPTH)/$(MODULE)/config/static.mk

#######################################################################
#  Local "sectools" component DYNAMIC system library names            #
#######################################################################

include $(CORE_DEPTH)/$(MODULE)/config/dynamic.mk

# Stricter semantic checking for SunOS compiler. This catches calling
# undeclared functions, a major headache during debugging.
ifeq ($(OS_ARCH), SunOS)
    OS_CFLAGS += -v
endif

# Add symbolic binding values to MKSHLIB and LINK_DLL to
# encompass special link options for dynamic libraries

ifeq ($(OS_ARCH), AIX)
MKSHLIB += -bsymbolic
endif
ifeq ($(OS_ARCH), HP-UX)
MKSHLIB += -B symbolic
endif
ifeq ($(OS_ARCH), IRIX)
MKSHLIB += -B symbolic
endif
ifeq ($(OS_ARCH), Linux)
MKSHLIB += -Wl,-Bsymbolic
endif
ifeq ($(OS_ARCH), OSF1)
MKSHLIB += -B symbolic
endif
ifeq ($(OS_ARCH), SunOS)
MKSHLIB += -B symbolic
endif
ifeq ($(OS_ARCH), WINNT)
LINK_DLL += -LIBPATH:$(SOURCE_LIB_DIR)
LINK_DLL += -LIBPATH:$(JAVA_HOME)/$(JAVA_LIBDIR)
LINK_DLL += $(foreach file,$(LD_LIBS),-DEFAULTLIB:"$(notdir $(file))")
endif

