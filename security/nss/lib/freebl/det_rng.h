/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __det_rng_h_
#define __det_rng_h_

SECStatus prng_ResetForFuzzing(PZLock *rng_lock);
SECStatus prng_GenerateDeterministicRandomBytes(PZLock *rng_lock, void *dest,
                                                size_t len);

#endif /* __det_rng_h_ */
