#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

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
