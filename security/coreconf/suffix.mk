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
# Master "Core Components" suffixes                                   #
#######################################################################

#
# Object suffixes   (OS2 and WIN% override this)
#
ifndef OBJ_SUFFIX
    OBJ_SUFFIX = .o
endif

#
# Assembler source suffixes (OS2 and WIN% override this)
#
ifndef ASM_SUFFIX
    ASM_SUFFIX = .s
endif

#
# Library suffixes
#
STATIC_LIB_EXTENSION =

ifndef DYNAMIC_LIB_EXTENSION
    DYNAMIC_LIB_EXTENSION =
endif


ifndef STATIC_LIB_SUFFIX
    STATIC_LIB_SUFFIX = .$(LIB_SUFFIX)
endif


ifndef DYNAMIC_LIB_SUFFIX
    DYNAMIC_LIB_SUFFIX = .$(DLL_SUFFIX)
endif

# WIN% overridese this
ifndef IMPORT_LIB_SUFFIX
    IMPORT_LIB_SUFFIX = 
endif


ifndef STATIC_LIB_SUFFIX_FOR_LINKING
    STATIC_LIB_SUFFIX_FOR_LINKING = $(STATIC_LIB_SUFFIX)
endif


# WIN% overridese this
ifndef DYNAMIC_LIB_SUFFIX_FOR_LINKING
    DYNAMIC_LIB_SUFFIX_FOR_LINKING = $(DYNAMIC_LIB_SUFFIX)
endif

#
# Program suffixes (OS2 and WIN% override this)
#

ifndef PROG_SUFFIX
    PROG_SUFFIX =
endif

MK_SUFFIX = included
