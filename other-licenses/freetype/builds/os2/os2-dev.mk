#
# FreeType 2 configuration rules for OS/2 + GCC
#
#   Development version without optimizations.
#


# Copyright 1996-2000 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


ifndef TOP
  TOP := .
endif

SEP   := /

include $(TOP)/builds/os2/os2-def.mk
BUILD := $(TOP)/builds/devel

include $(TOP)/builds/compiler/gcc-dev.mk

# include linking instructions
include $(TOP)/builds/link_dos.mk


# EOF
