/****************************************************************************
 *
 * dlgwrap.c
 *
 *   Wrapper file for the 'dlg' library (body only)
 *
 * Copyright (C) 2020-2021 by
 * David Turner, Robert Wilhelm, and Werner Lemberg.
 *
 * This file is part of the FreeType project, and may only be used,
 * modified, and distributed under the terms of the FreeType project
 * license, LICENSE.TXT.  By continuing to use, modify, or distribute
 * this file you indicate that you have read the license and
 * understand and accept it fully.
 *
 */


  /* We have to duplicate these feature test macros from `dlg.c` */
  /* since `freetype.h` loads some affected standard headers.    */
#define _XOPEN_SOURCE
#define _POSIX_C_SOURCE 200809L

#include <freetype/freetype.h>


#ifdef FT_DEBUG_LOGGING
#include "dlg.c"
#else
  /* ANSI C doesn't like empty source files */
  typedef int  _dlg_dummy;
#endif


/* END */
