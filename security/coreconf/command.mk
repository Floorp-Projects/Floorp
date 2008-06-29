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
# The Original Code is the Netscape security libraries.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1994-2000
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
# Master "Core Components" default command macros;                    #
# can be overridden in <arch>.mk                                      #
#######################################################################

AS            = $(CC)
ASFLAGS      += $(CFLAGS)
CCF           = $(CC) $(CFLAGS)
LINK_DLL      = $(LINK) $(OS_DLLFLAGS) $(DLLFLAGS)
LINK_EXE      = $(LINK) $(OS_LFLAGS) $(LFLAGS)
CFLAGS        = $(OPTIMIZER) $(OS_CFLAGS) $(XP_DEFINE) $(DEFINES) $(INCLUDES) \
		$(XCFLAGS)
PERL          = perl
RANLIB        = echo
TAR           = /bin/tar
#
# For purify
#
NOMD_CFLAGS  += $(OPTIMIZER) $(NOMD_OS_CFLAGS) $(XP_DEFINE) $(DEFINES) \
		$(INCLUDES) $(XCFLAGS)

# Optimization of code for size
# OPT_CODE_SIZE
# =1: The code can be optimized for size.
#     The code is actually optimized for size only if ALLOW_OPT_CODE_SIZE=1
#     in a given source code directory (in manifest.mn)
# =0: Never optimize the code for size.
#
# Default value = 0 
# Can be overridden from the make command line.
ifndef OPT_CODE_SIZE
OPT_CODE_SIZE = 0
endif

MK_COMMAND = included
