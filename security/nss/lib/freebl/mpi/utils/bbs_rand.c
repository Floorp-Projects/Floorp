/*
 *  Blum, Blum & Shub PRNG using the MPI library
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
 *  $Id: bbs_rand.c,v 1.1 2000/07/14 00:44:53 nelsonb%netscape.com Exp $
 */

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
