/* Default configuration for MPI library
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MPI_CONFIG_H_
#define MPI_CONFIG_H_

/*
  For boolean options,
  0 = no
  1 = yes

  Other options are documented individually.

 */

#ifndef MP_IOFUNC
#define MP_IOFUNC 0 /* include mp_print() ?                */
#endif

#ifndef MP_MODARITH
#define MP_MODARITH 1 /* include modular arithmetic ?        */
#endif

#ifndef MP_NUMTH
#define MP_NUMTH 1 /* include number theoretic functions? */
#endif

#ifndef MP_LOGTAB
#define MP_LOGTAB 1 /* use table of logs instead of log()? */
#endif

#ifndef MP_MEMSET
#define MP_MEMSET 1 /* use memset() to zero buffers?       */
#endif

#ifndef MP_MEMCPY
#define MP_MEMCPY 1 /* use memcpy() to copy buffers?       */
#endif

#ifndef MP_ARGCHK
/*
  0 = no parameter checks
  1 = runtime checks, continue execution and return an error to caller
  2 = assertions; dump core on parameter errors
 */
#ifdef DEBUG
#define MP_ARGCHK 2 /* how to check input arguments        */
#else
#define MP_ARGCHK 1 /* how to check input arguments        */
#endif
#endif

#ifndef MP_DEBUG
#define MP_DEBUG 0 /* print diagnostic output?            */
#endif

#ifndef MP_DEFPREC
#define MP_DEFPREC 64 /* default precision, in digits        */
#endif

#ifndef MP_SQUARE
#define MP_SQUARE 1 /* use separate squaring code?         */
#endif

#endif /* ifndef MPI_CONFIG_H_ */
