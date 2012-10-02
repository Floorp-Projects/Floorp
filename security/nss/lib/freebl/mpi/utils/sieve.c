/*
 * sieve.c
 *
 * Finds prime numbers using the Sieve of Eratosthenes
 *
 * This implementation uses a bitmap to represent all odd integers in a
 * given range.  We iterate over this bitmap, crossing off the
 * multiples of each prime we find.  At the end, all the remaining set
 * bits correspond to prime integers.
 *
 * Here, we make two passes -- once we have generated a sieve-ful of
 * primes, we copy them out, reset the sieve using the highest
 * generated prime from the first pass as a base.  Then we cross out
 * all the multiples of all the primes we found the first time through,
 * and re-sieve.  In this way, we get double use of the memory we
 * allocated for the sieve the first time though.  Since we also
 * implicitly ignore multiples of 2, this amounts to 4 times the
 * values.
 *
 * This could (and probably will) be generalized to re-use the sieve a
 * few more times.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* $Id: sieve.c,v 1.4 2012/04/25 14:49:54 gerv%gerv.net Exp $ */

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

typedef unsigned char  byte;

typedef struct {
  int   size;
  byte *bits;
  long  base;
  int   next;
  int   nbits;
} sieve;

void sieve_init(sieve *sp, long base, int nbits);
void sieve_grow(sieve *sp, int nbits);
long sieve_next(sieve *sp);
void sieve_reset(sieve *sp, long base);
void sieve_cross(sieve *sp, long val);
void sieve_clear(sieve *sp);

#define S_ISSET(S, B)  (((S)->bits[(B)/CHAR_BIT]>>((B)%CHAR_BIT))&1)
#define S_SET(S, B)    ((S)->bits[(B)/CHAR_BIT]|=(1<<((B)%CHAR_BIT)))
#define S_CLR(S, B)    ((S)->bits[(B)/CHAR_BIT]&=~(1<<((B)%CHAR_BIT)))
#define S_VAL(S, B)    ((S)->base+(2*(B)))
#define S_BIT(S, V)    (((V)-((S)->base))/2)

int main(int argc, char *argv[])
{
  sieve   s;
  long    pr, *p;
  int     c, ix, cur = 0;

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <width>\n", argv[0]);
    return 1;
  }

  c = atoi(argv[1]);
  if(c < 0) c = -c;

  fprintf(stderr, "%s: sieving to %d positions\n", argv[0], c);

  sieve_init(&s, 3, c);

  c = 0;
  while((pr = sieve_next(&s)) > 0) {
    ++c;
  }

  p = calloc(c, sizeof(long));
  if(!p) {
    fprintf(stderr, "%s: out of memory after first half\n", argv[0]);
    sieve_clear(&s);
    exit(1);
  }

  fprintf(stderr, "%s: half done ... \n", argv[0]);

  for(ix = 0; ix < s.nbits; ix++) {
    if(S_ISSET(&s, ix)) {
      p[cur] = S_VAL(&s, ix);
      printf("%ld\n", p[cur]);
      ++cur;
    }
  }

  sieve_reset(&s, p[cur - 1]);
  fprintf(stderr, "%s: crossing off %d found primes ... \n", argv[0], cur);
  for(ix = 0; ix < cur; ix++) {
    sieve_cross(&s, p[ix]);
    if(!(ix % 1000))
      fputc('.', stderr);
  }
  fputc('\n', stderr);

  free(p);

  fprintf(stderr, "%s: sieving again from %ld ... \n", argv[0], p[cur - 1]);
  c = 0;
  while((pr = sieve_next(&s)) > 0) {
    ++c;
  }
  
  fprintf(stderr, "%s: done!\n", argv[0]);
  for(ix = 0; ix < s.nbits; ix++) {
    if(S_ISSET(&s, ix)) {
      printf("%ld\n", S_VAL(&s, ix));
    }
  }

  sieve_clear(&s);

  return 0;
}

void sieve_init(sieve *sp, long base, int nbits)
{
  sp->size = (nbits / CHAR_BIT);

  if(nbits % CHAR_BIT)
    ++sp->size;

  sp->bits = calloc(sp->size, sizeof(byte));
  memset(sp->bits, UCHAR_MAX, sp->size);
  if(!(base & 1))
    ++base;
  sp->base = base;
  
  sp->next = 0;
  sp->nbits = sp->size * CHAR_BIT;
}

void sieve_grow(sieve *sp, int nbits)
{
  int  ns = (nbits / CHAR_BIT);

  if(nbits % CHAR_BIT)
    ++ns;

  if(ns > sp->size) {
    byte   *tmp;
    int     ix;

    tmp = calloc(ns, sizeof(byte));
    if(tmp == NULL) {
      fprintf(stderr, "Error: out of memory in sieve_grow\n");
      return;
    }

    memcpy(tmp, sp->bits, sp->size);
    for(ix = sp->size; ix < ns; ix++) {
      tmp[ix] = UCHAR_MAX;
    }

    free(sp->bits);
    sp->bits = tmp;
    sp->size = ns;

    sp->nbits = sp->size * CHAR_BIT;
  }
}

long sieve_next(sieve *sp)
{
  long out;
  int  ix = 0;
  long val;

  if(sp->next > sp->nbits)
    return -1;

  out = S_VAL(sp, sp->next);
#ifdef DEBUG
  fprintf(stderr, "Sieving %ld\n", out);
#endif

  /* Sieve out all multiples of the current prime */
  val = out;
  while(ix < sp->nbits) {
    val += out;
    ix = S_BIT(sp, val);
    if((val & 1) && ix < sp->nbits) { /* && S_ISSET(sp, ix)) { */
      S_CLR(sp, ix);
#ifdef DEBUG
      fprintf(stderr, "Crossing out %ld (bit %d)\n", val, ix);
#endif
    }
  }

  /* Scan ahead to the next prime */
  ++sp->next;
  while(sp->next < sp->nbits && !S_ISSET(sp, sp->next))
    ++sp->next;

  return out;
}

void sieve_cross(sieve *sp, long val)
{
  int  ix = 0;
  long cur = val;

  while(cur < sp->base)
    cur += val;

  ix = S_BIT(sp, cur);
  while(ix < sp->nbits) {
    if(cur & 1) 
      S_CLR(sp, ix);
    cur += val;
    ix = S_BIT(sp, cur);
  }
}

void sieve_reset(sieve *sp, long base)
{
  memset(sp->bits, UCHAR_MAX, sp->size);
  sp->base = base;
  sp->next = 0;
}

void sieve_clear(sieve *sp)
{
  if(sp->bits) 
    free(sp->bits);

  sp->bits = NULL;
}
