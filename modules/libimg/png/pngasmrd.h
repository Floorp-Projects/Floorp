/* pngasmrd.h - assembler version of utilities to read a PNG file
 *
 * libpng 1.0.8 - July 24, 2000
 * For conditions of distribution and use, see copyright notice in png.h
 * Copyright (c) 1999, 2000 Glenn Randers-Pehrson
 *
 */

#ifndef PNGASMRD_H
#define PNGASMRD_H

#ifdef PNG_ASSEMBLER_CODE_SUPPORTED

/* Set this in the makefile for VC++ on Pentium, not in pngconf.h */
/* Platform must be Pentium.  Makefile must assemble and load pngvcrd.c .
 * MMX will be detected at run time and used if present.
 */
#ifdef PNG_USE_PNGVCRD
#  define PNG_HAVE_ASSEMBLER_COMBINE_ROW
#  define PNG_HAVE_ASSEMBLER_READ_INTERLACE
#  define PNG_HAVE_ASSEMBLER_READ_FILTER_ROW
#endif

/* Set this in the makefile for gcc/as on Pentium, not in pngconf.h */
/* Platform must be Pentium.  Makefile must assemble and load pnggccrd.c .
 * MMX will be detected at run time and used if present.
 */
#ifdef PNG_USE_PNGGCCRD
#  define PNG_HAVE_ASSEMBLER_COMBINE_ROW
#  define PNG_HAVE_ASSEMBLER_READ_INTERLACE
#  define PNG_HAVE_ASSEMBLER_READ_FILTER_ROW
#endif
/*
    GRR notes:
      - see pnggccrd.c for info about what is currently enabled
 */

#endif
#endif /* PNGASMRD_H */
