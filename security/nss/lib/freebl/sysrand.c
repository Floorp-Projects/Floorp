/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef FREEBL_NO_DEPEND
#include "stubs.h"
#endif

#include "seccomon.h"

static size_t rng_systemFromNoise(unsigned char *dest, size_t maxLen);

#if defined(XP_UNIX) || defined(XP_BEOS)
#include "unix_rand.c"
#endif
#ifdef XP_WIN
#include "win_rand.c"
#endif
#ifdef XP_OS2
#include "os2_rand.c"
#endif

/*
 * Normal RNG_SystemRNG() isn't available, use the system noise to collect
 * the required amount of entropy.
 */
static size_t 
rng_systemFromNoise(unsigned char *dest, size_t maxLen) 
{
   size_t retBytes = maxLen;

   while (maxLen) {
	size_t nbytes = RNG_GetNoise(dest, maxLen);

	PORT_Assert(nbytes != 0);

	dest += nbytes;
	maxLen -= nbytes;

	/* some hw op to try to introduce more entropy into the next
	 * RNG_GetNoise call */
	rng_systemJitter();
   }
   return retBytes;
}

