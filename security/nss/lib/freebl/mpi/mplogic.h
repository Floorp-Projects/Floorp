/*
 *  mplogic.h
 *
 *  Bitwise logical operations on MPI values
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _H_MPLOGIC_
#define _H_MPLOGIC_

#include "mpi.h"
SEC_BEGIN_PROTOS

/*
  The logical operations treat an mp_int as if it were a bit vector,
  without regard to its sign (an mp_int is represented in a signed
  magnitude format).  Values are treated as if they had an infinite
  string of zeros left of the most-significant bit.
 */

/* Parity results                    */

#define MP_EVEN MP_YES
#define MP_ODD MP_NO

/* Bitwise functions                 */

mp_err mpl_not(mp_int *a, mp_int *b);            /* one's complement  */
mp_err mpl_and(mp_int *a, mp_int *b, mp_int *c); /* bitwise AND       */
mp_err mpl_or(mp_int *a, mp_int *b, mp_int *c);  /* bitwise OR        */
mp_err mpl_xor(mp_int *a, mp_int *b, mp_int *c); /* bitwise XOR       */

/* Shift functions                   */

mp_err mpl_rsh(const mp_int *a, mp_int *b, mp_digit d); /* right shift    */
mp_err mpl_lsh(const mp_int *a, mp_int *b, mp_digit d); /* left shift     */

/* Bit count and parity              */

mp_err mpl_num_set(mp_int *a, unsigned int *num);   /* count set bits    */
mp_err mpl_num_clear(mp_int *a, unsigned int *num); /* count clear bits  */
mp_err mpl_parity(mp_int *a);                       /* determine parity  */

/* Get & Set the value of a bit */

mp_err mpl_set_bit(mp_int *a, mp_size bitNum, mp_size value);
mp_err mpl_get_bit(const mp_int *a, mp_size bitNum);
mp_err mpl_get_bits(const mp_int *a, mp_size lsbNum, mp_size numBits);
mp_size mpl_significant_bits(const mp_int *a);

SEC_END_PROTOS

#endif /* end _H_MPLOGIC_ */
