/*
 *  primegen.c
 *
 * Generates random integers which are prime with a high degree of
 * probability using the Miller-Rabin probabilistic primality testing
 * algorithm.
 *
 * Usage:
 *    primegen <bits> [<num>]
 *
 *    <bits>   - number of significant bits each prime should have
 *    <num>    - number of primes to generate
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the MPI Arbitrary Precision Integer Arithmetic
 * library.
 *
 * The Initial Developer of the Original Code is Michael J. Fromberger.
 * Portions created by Michael J. Fromberger are 
 * Copyright (C) 1998, 1999, 2000 Michael J. Fromberger. 
 * All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable
 * instead of those above.  If you wish to allow use of your
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the GPL.
 *
 * $Id: primegen.c,v 1.2 2000/07/22 05:54:21 nelsonb%netscape.com Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <time.h>

#include "mpi.h"
#include "mplogic.h"
#include "mpprime.h"

#undef MACOS		/* define if running on a Macintosh */

#ifdef MACOS
#include <console.h>
#endif

#define NUM_TESTS 5  /* Number of Rabin-Miller iterations to test with */

unsigned long ntries;

mp_err mpp_sieve(mp_int *trial, const mp_digit *primes, unsigned int nPrimes, 
		 unsigned char *sieve, unsigned int nSieve)
{
  mp_err       res;
  mp_digit     rem;
  unsigned int ix;
  unsigned long offset;

  memset(sieve, 0, nSieve);

  for(ix = 0; ix < nPrimes; ix++) {
    mp_digit prime = primes[ix];
    unsigned int i;
    if((res = mp_mod_d(trial, prime, &rem)) != MP_OKAY) 
      return res;

    if (rem == 0) {
      offset = 0;
    } else {
      offset = prime - (rem / 2);
    }
    for (i = offset; i < nSieve ; i += prime) {
      sieve[i] = 1;
    }
  }

  return MP_OKAY;
}

mp_err mpp_make_prime(mp_int *start, unsigned int nBits, unsigned int strong)
{
  mp_digit      np;
  mp_err        res;
  int           i;
  mp_int        trial;
  mp_int        q;
  unsigned char sieve[32*1024];

  ARGCHK(start != 0, MP_BADARG);
  ARGCHK(nBits > 16, MP_RANGE);

  mp_init(&trial);
  mp_init(&q);
  if (strong) 
    --nBits;
  res = mpl_set_bit(start, nBits - 1, 1);  if (res != MP_OKAY) goto loser;
  res = mpl_set_bit(start,         0, 1);  if (res != MP_OKAY) goto loser;
  for (i = mpl_significant_bits(start) - 1; i >= nBits; --i) {
    res = mpl_set_bit(start, i, 0);  if (res != MP_OKAY) goto loser;
  }
  /* start sieveing with prime value of 3. */
  res = mpp_sieve(start, prime_tab + 1, prime_tab_size - 1, 
			 sieve, sizeof sieve);
  if (res != MP_OKAY) goto loser;
#ifdef DEBUG
  res = 0;
  for (i = 0; i < sizeof sieve; ++i) {
    if (!sieve[i])
      ++res;
  }
  fprintf(stderr,"sieve found %d potential primes.\n", res);
#define FPUTC(x,y) fputc(x,y)
#else
#define FPUTC(x,y) 
#endif
  res = MP_NO;
  for(i = 0; i < sizeof sieve; ++i) {
    if (sieve[i])	/* this number is composite */
      continue;
    res = mp_add_d(start, 2 * i, &trial); if (res != MP_OKAY) goto loser;
    FPUTC('.', stderr);
    /* run a Fermat test */
    res = mpp_fermat(&trial, 2);
    if (res != MP_OKAY) {
      if (res == MP_NO)
	continue;	/* was composite */
      goto loser;
    }
      
    FPUTC('+', stderr);
    /* If that passed, run some Miller-Rabin tests	*/
    res = mpp_pprime(&trial, NUM_TESTS);
    if (res != MP_OKAY) {
      if (res == MP_NO)
	continue;	/* was composite */
      goto loser;
    }
    FPUTC('!', stderr);

    if (!strong) 
      break;	/* success !! */

    /* At this point, we have strong evidence that our candidate
       is itself prime.  If we want a strong prime, we need now
       to test q = 2p + 1 for primality...
     */
    mp_mul_2(&trial, &q);
    mp_add_d(&q, 1, &q);

    /* Test q for small prime divisors ... */
    np = prime_tab_size;
    if (mpp_divis_primes(&q, &np) == MP_YES) { /* is composite */
      mp_clear(&q);
      continue;
    }

    /* And test with Fermat, as with its parent ... */
    res = mpp_fermat(&q, 2);
    if (res != MP_YES) {
      mp_clear(&q);
      if (res == MP_NO)
	continue;	/* was composite */
      goto loser;
    }

    /* And test with Miller-Rabin, as with its parent ... */
    res = mpp_pprime(&q, NUM_TESTS);
    if (res != MP_YES) {
      mp_clear(&q);
      if (res == MP_NO)
	continue;	/* was composite */
      goto loser;
    }

    /* If it passed, we've got a winner */
    mp_exch(&q, &trial);
    mp_clear(&q);
    break;

  } /* end of loop through sieved values */
  if (res == MP_YES)
    mp_exch(&trial, start);
loser:
  mp_clear(&trial);
  ntries += i;
  return res;
}

int main(int argc, char *argv[])
{
  unsigned char *raw;
  char          *out;
  int		rawlen, bits, outlen, ngen, ix, jx;
  int           g_strong = 0;
  mp_int	testval;
  mp_err	res;
  clock_t	start, end;

#ifdef MACOS
  argc = ccommand(&argv);
#endif

  /* We'll just use the C library's rand() for now, although this
     won't be good enough for cryptographic purposes */
  if((out = getenv("SEED")) == NULL) {
    srand((unsigned int)time(NULL));
  } else {
    srand((unsigned int)atoi(out));
  }

  if(argc < 2) {
    fprintf(stderr, "Usage: %s <bits> [<count> [strong]]\n", argv[0]);
    return 1;
  }
	
  if((bits = abs(atoi(argv[1]))) < CHAR_BIT) {
    fprintf(stderr, "%s: please request at least %d bits.\n",
	    argv[0], CHAR_BIT);
    return 1;
  }

  /* If optional third argument is given, use that as the number of
     primes to generate; otherwise generate one prime only.
   */
  if(argc < 3) {
    ngen = 1;
  } else {
    ngen = abs(atoi(argv[2]));
  }

  /* If fourth argument is given, and is the word "strong", we'll 
     generate strong (Sophie Germain) primes. 
   */
  if(argc > 3 && strcmp(argv[3], "strong") == 0)
    g_strong = 1;

  /* testval - candidate being tested; ntries - number tried so far */
  if ((res = mp_init(&testval)) != MP_OKAY) {
    fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));
    return 1;
  }
  
  if(g_strong) {
    printf("Requested %d strong prime values of at least %d bits.\n", 
	   ngen, bits);
  } else {
    printf("Requested %d prime values of at least %d bits.\n", ngen, bits);
  }

  rawlen = (bits / CHAR_BIT) + ((bits % CHAR_BIT) ? 1 : 0) + 1;

  if((raw = calloc(rawlen, sizeof(unsigned char))) == NULL) {
    fprintf(stderr, "%s: out of memory, sorry.\n", argv[0]);
    return 1;
  }

  /* This loop is one for each prime we need to generate */
  for(jx = 0; jx < ngen; jx++) {

    raw[0] = 0;  /* sign is positive */

    /*	Pack the initializer with random bytes	*/
    for(ix = 1; ix < rawlen; ix++) 
      raw[ix] = (rand() * rand()) & UCHAR_MAX;

    raw[1] |= 0x80;             /* set high-order bit of test value     */
    if(g_strong)
      raw[rawlen - 1] |= 3;     /* set low-order two bits of test value */
    else
      raw[rawlen - 1] |= 1;     /* set low-order bit of test value      */

    /* Make an mp_int out of the initializer */
    mp_read_raw(&testval, (char *)raw, rawlen);

    /* Initialize candidate counter */
    ntries = 0;

    start = clock(); /* time generation for this prime */
    do {
      res = mpp_make_prime(&testval, bits, g_strong);
      if (res != MP_NO)
	break;
      /* This code works whether digits are 16 or 32 bits */
      res = mp_add_d(&testval, 32 * 1024, &testval);
      res = mp_add_d(&testval, 32 * 1024, &testval);
      FPUTC(',', stderr);
    } while (1);
    end = clock();

    if (res != MP_YES) {
      break;
    }
    FPUTC('\n', stderr);
    printf("After %d tests, the following value is still probably prime:\n",
	   NUM_TESTS);
    outlen = mp_radix_size(&testval, 10);
    out = calloc(outlen, sizeof(unsigned char));
    mp_toradix(&testval, (char *)out, 10);
    printf("10: %s\n", out);
    mp_toradix(&testval, (char *)out, 16);
    printf("16: %s\n\n", out);
    free(out);
    
    printf("Number of candidates tried: %d\n", ntries);
    printf("This computation took %ld clock ticks (%.2f seconds)\n",
	   (end - start), ((double)(end - start) / CLOCKS_PER_SEC));
    
    FPUTC('\n', stderr);
  } /* end of loop to generate all requested primes */
  
  if(res != MP_OKAY) 
    fprintf(stderr, "%s: error: %s\n", argv[0], mp_strerror(res));

  free(raw);
  mp_clear(&testval);	
  
  return 0;
}
