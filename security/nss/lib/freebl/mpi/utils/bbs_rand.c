/*
 *  Blum, Blum & Shub PRNG using the MPI library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "bbs_rand.h"

#define SEED     1
#define MODULUS  2

/* This modulus is the product of two randomly generated 512-bit
   prime integers, each of which is congruent to 3 (mod 4).          */
static char *bbs_modulus = 
"75A2A6E1D27393B86562B9CE7279A8403CB4258A637DAB5233465373E37837383EDC"
"332282B8575927BC4172CE8C147B4894050EE9D2BDEED355C121037270CA2570D127"
"7D2390CD1002263326635CC6B259148DE3A1A03201980A925E395E646A5E9164B0EC"
"28559EBA58C87447245ADD0651EDA507056A1129E3A3E16E903D64B437";

static int    bbs_init = 0;  /* flag set when library is initialized */
static mp_int bbs_state;     /* the current state of the generator   */

/* Suggested size of random seed data */
int           bbs_seed_size = (sizeof(bbs_modulus) / 2);

void         bbs_srand(unsigned char *data, int len)
{
  if((bbs_init & SEED) == 0) {
    mp_init(&bbs_state);
    bbs_init |= SEED;
  }

  mp_read_raw(&bbs_state, (char *)data, len);

} /* end bbs_srand() */

unsigned int bbs_rand(void)
{
  static mp_int   modulus;
  unsigned int    result = 0, ix;

  if((bbs_init & MODULUS) == 0) {
    mp_init(&modulus);
    mp_read_radix(&modulus, bbs_modulus, 16);
    bbs_init |= MODULUS;
  }

  for(ix = 0; ix < sizeof(unsigned int); ix++) {
    mp_digit   d;

    mp_sqrmod(&bbs_state, &modulus, &bbs_state);
    d = DIGIT(&bbs_state, 0);

    result = (result << CHAR_BIT) | (d & UCHAR_MAX);
  }

  return result;

} /* end bbs_rand() */

/*------------------------------------------------------------------------*/
/* HERE THERE BE DRAGONS                                                  */
