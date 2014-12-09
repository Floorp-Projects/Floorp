/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _CPR_WIN_RAND_H_
#define _CPR_WIN_RAND_H_

#define CPR_WIN_RAND_DEBUG

#define cpr_srand(seed) srand(seed)
#define cpr_rand()      rand()

extern int cpr_crypto_rand(uint8_t *buf, int *len);
#endif

