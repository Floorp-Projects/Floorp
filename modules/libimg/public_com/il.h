/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* -*- Mode: C; tab-width: 4 -*-
 *   il.h --- Exported image library interface
 *
 *   $Id: il.h,v 1.2 1999/04/19 22:41:11 pnunn%netscape.com Exp $
 */


/*
 *  global defines for image lib users
 */

#ifndef _IL_H
#define _IL_H

#include "ntypes.h"

/* attr values */
#define IL_ATTR_RDONLY	      0
#define IL_ATTR_RW	          1
#define IL_ATTR_TRANSPARENT	  2
#define IL_ATTR_HONOR_INDEX   4

#undef ABS
#define ABS(x)     (((x) < 0) ? -(x) : (x))

/* A fast but limited, perceptually-weighted color distance function */
#define IL_COLOR_DISTANCE(r1, r2, g1, g2, b1, b2)                             \
  ((ABS((g1) - (g2)) << 2) + (ABS((r1) - (r2)) << 1) + (ABS((b1) - (b2))))

/* We don't distinguish between colors that are "closer" together
   than this.  The appropriate setting is a subjective matter. */
#define IL_CLOSE_COLOR_THRESHOLD  6


#endif /* _IL_H */


