/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetSegmentUtils_h__
#define nsNetSegmentUtils_h__

#include "nsIOService.h"

/**
 * applies defaults to segment params in a consistent way.
 */
static inline void
net_ResolveSegmentParams(uint32_t &segsize, uint32_t &segcount)
{
    if (!segsize)
        segsize = nsIOService::gDefaultSegmentSize;

    if (!segcount)
        segcount = nsIOService::gDefaultSegmentCount;
}

#endif // !nsNetSegmentUtils_h__
