#
# FreeType 2 base layer compilation rules for VMS
#


# Copyright 2001 by
# David Turner, Robert Wilhelm, and Werner Lemberg.
#
# This file is part of the FreeType project, and may only be used, modified,
# and distributed under the terms of the FreeType project license,
# LICENSE.TXT.  By continuing to use, modify, or distribute this file you
# indicate that you have read the license and understand and accept it
# fully.


CFLAGS=$(COMP_FLAGS)$(DEBUG)/include=([--.builds.vms],[--.include],[--.src.base])

OBJS=ftbase.obj,ftinit.obj,ftglyph.obj,ftdebug.obj,ftmm.obj

all : $(OBJS)
        library [--.lib]freetype.olb $(OBJS)

# EOF
