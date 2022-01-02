/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* We need this because Solaris' version of qsort is broken and
 * causes array bounds reads.
 */

#include <stdlib.h>
#include "nsAlgorithm.h"
#include "nsQuickSort.h"

extern "C" {

#if !defined(DEBUG) && (defined(__cplusplus) || defined(__gcc))
#  ifndef INLINE
#    define INLINE inline
#  endif
#else
#  define INLINE
#endif

typedef int cmp_t(const void*, const void*, void*);
static INLINE char* med3(char*, char*, char*, cmp_t*, void*);
static INLINE void swapfunc(char*, char*, int, int);

/*
 * Qsort routine from Bentley & McIlroy's "Engineering a Sort Function".
 */
#define swapcode(TYPE, parmi, parmj, n) \
  {                                     \
    long i = (n) / sizeof(TYPE);        \
    TYPE* pi = (TYPE*)(parmi);          \
    TYPE* pj = (TYPE*)(parmj);          \
    do {                                \
      TYPE t = *pi;                     \
      *pi++ = *pj;                      \
      *pj++ = t;                        \
    } while (--i > 0);                  \
  }

#define SWAPINIT(a, es)                                                    \
  swaptype = ((char*)a - (char*)0) % sizeof(long) || es % sizeof(long) ? 2 \
             : es == sizeof(long)                                      ? 0 \
                                                                       : 1;

static INLINE void swapfunc(char* a, char* b, int n, int swaptype) {
  if (swaptype <= 1) swapcode(long, a, b, n) else swapcode(char, a, b, n)
}

#define swap(a, b)             \
  if (swaptype == 0) {         \
    long t = *(long*)(a);      \
    *(long*)(a) = *(long*)(b); \
    *(long*)(b) = t;           \
  } else                       \
    swapfunc((char*)a, (char*)b, (int)es, swaptype)

#define vecswap(a, b, n) \
  if ((n) > 0) swapfunc((char*)a, (char*)b, (int)n, swaptype)

static INLINE char* med3(char* a, char* b, char* c, cmp_t* cmp, void* data) {
  return cmp(a, b, data) < 0
             ? (cmp(b, c, data) < 0 ? b : (cmp(a, c, data) < 0 ? c : a))
             : (cmp(b, c, data) > 0 ? b : (cmp(a, c, data) < 0 ? a : c));
}

void NS_QuickSort(void* a, unsigned int n, unsigned int es, cmp_t* cmp,
                  void* data) {
  char *pa, *pb, *pc, *pd, *pl, *pm, *pn;
  int d, r, swaptype;

loop:
  SWAPINIT(a, es);
  /* Use insertion sort when input is small */
  if (n < 7) {
    for (pm = (char*)a + es; pm < (char*)a + n * es; pm += es)
      for (pl = pm; pl > (char*)a && cmp(pl - es, pl, data) > 0; pl -= es)
        swap(pl, pl - es);
    return;
  }
  /* Choose pivot */
  pm = (char*)a + (n / 2) * es;
  if (n > 7) {
    pl = (char*)a;
    pn = (char*)a + (n - 1) * es;
    if (n > 40) {
      d = (n / 8) * es;
      pl = med3(pl, pl + d, pl + 2 * d, cmp, data);
      pm = med3(pm - d, pm, pm + d, cmp, data);
      pn = med3(pn - 2 * d, pn - d, pn, cmp, data);
    }
    pm = med3(pl, pm, pn, cmp, data);
  }
  swap(a, pm);
  pa = pb = (char*)a + es;

  pc = pd = (char*)a + (n - 1) * es;
  /* loop invariants:
   * [a, pa) = pivot
   * [pa, pb) < pivot
   * [pb, pc + es) unprocessed
   * [pc + es, pd + es) > pivot
   * [pd + es, pn) = pivot
   */
  for (;;) {
    while (pb <= pc && (r = cmp(pb, a, data)) <= 0) {
      if (r == 0) {
        swap(pa, pb);
        pa += es;
      }
      pb += es;
    }
    while (pb <= pc && (r = cmp(pc, a, data)) >= 0) {
      if (r == 0) {
        swap(pc, pd);
        pd -= es;
      }
      pc -= es;
    }
    if (pb > pc) break;
    swap(pb, pc);
    pb += es;
    pc -= es;
  }
  /* Move pivot values */
  pn = (char*)a + n * es;
  r = XPCOM_MIN(pa - (char*)a, pb - pa);
  vecswap(a, pb - r, r);
  r = XPCOM_MIN<size_t>(pd - pc, pn - pd - es);
  vecswap(pb, pn - r, r);
  /* Recursively process partitioned items */
  if ((r = pb - pa) > (int)es) NS_QuickSort(a, r / es, es, cmp, data);
  if ((r = pd - pc) > (int)es) {
    /* Iterate rather than recurse to save stack space */
    a = pn - r;
    n = r / es;
    goto loop;
  }
  /*		NS_QuickSort(pn - r, r / es, es, cmp, data);*/
}
}

#undef INLINE
#undef swapcode
#undef SWAPINIT
#undef swap
#undef vecswap
