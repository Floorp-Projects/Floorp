/* ************************************************************************** */
/* *             For conditions of distribution and use,                    * */
/* *                see copyright notice in libmng.h                        * */
/* ************************************************************************** */
/* *                                                                        * */
/* * project   : libmng                                                     * */
/* * file      : mng_memory.h              copyright (c) 2000 G.Juyn        * */
/* * version   : 0.5.1                                                      * */
/* *                                                                        * */
/* * purpose   : Memory management (definition)                             * */
/* *                                                                        * */
/* * author    : G.Juyn                                                     * */
/* * web       : http://www.3-t.com                                         * */
/* * email     : mailto:info@3-t.com                                        * */
/* *                                                                        * */
/* * comment   : Definition of memory management functions                  * */
/* *                                                                        * */
/* * changes   : 0.5.1 - 05/08/2000 - G.Juyn                                * */
/* *             - changed strict-ANSI stuff                                * */
/* *                                                                        * */
/* ************************************************************************** */

#if defined(__BORLANDC__) && defined(MNG_STRICT_ANSI)
#pragma option -A                      /* force ANSI-C */
#endif

#ifndef _mng_memory_h_
#define _mng_memory_h_

#include "libmng.h"
#include "mng_data.h"
#include "mng_error.h"

/* ************************************************************************** */
/* *                                                                        * */
/* * Generic memory manager macros                                          * */
/* *                                                                        * */
/* ************************************************************************** */

#ifdef MNG_INTERNAL_MEMMNGMT
#define MNG_ALLOC(H,P,L)  { P = calloc (1, L); \
                            if (P == 0) { MNG_ERROR (H, MNG_OUTOFMEMORY) } }
#define MNG_ALLOCX(H,P,L) { P = calloc (1, L); }
#define MNG_FREE(H,P,L)   { if (P) { free (P); P = 0; } }
#define MNG_FREEX(H,P,L)  { if (P) free (P); }
#else
#define MNG_ALLOC(H,P,L)  { P = H->fMemalloc (L); \
                            if (P == 0) { MNG_ERROR (H, MNG_OUTOFMEMORY) } }
#define MNG_ALLOCX(H,P,L) { P = H->fMemalloc (L); }
#define MNG_FREE(H,P,L)   { if (P) { H->fMemfree (P, L); P = 0; } }
#define MNG_FREEX(H,P,L)  { if (P) { H->fMemfree (P, L); } }
#endif /* mng_internal_memmngmt */

#define MNG_COPY(S,D,L)   { memcpy (S, D, L); }

/* ************************************************************************** */

#endif /* _mng_memory_h_ */

/* ************************************************************************** */
/* * end of file                                                            * */
/* ************************************************************************** */
