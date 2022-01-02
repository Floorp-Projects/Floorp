/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * A bloom filter.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sslbloom.h"
#include "prnetdb.h"
#include "secport.h"

static inline unsigned int
sslBloom_Size(unsigned int bits)
{
    return (bits >= 3) ? (1 << (bits - 3)) : 1;
}

SECStatus
sslBloom_Init(sslBloomFilter *filter, unsigned int k, unsigned int bits)
{
    PORT_Assert(filter);
    PORT_Assert(bits > 0);
    PORT_Assert(bits <= sizeof(PRUint32) * 8);
    PORT_Assert(k > 0);

    filter->filter = PORT_ZNewArray(PRUint8, sslBloom_Size(bits));
    if (!filter->filter) {
        return SECFailure; /* Error code already set. */
    }

    filter->k = k;
    filter->bits = bits;
    return SECSuccess;
}

void
sslBloom_Zero(sslBloomFilter *filter)
{
    PORT_Memset(filter->filter, 0, sslBloom_Size(filter->bits));
}

void
sslBloom_Fill(sslBloomFilter *filter)
{
    PORT_Memset(filter->filter, 0xff, sslBloom_Size(filter->bits));
}

static PRBool
sslBloom_AddOrCheck(sslBloomFilter *filter, const PRUint8 *hashes, PRBool add)
{
    unsigned int iteration;
    unsigned int bitIndex;
    PRUint32 tmp = 0;
    PRUint8 mask;
    unsigned int bytes = (filter->bits + 7) / 8;
    unsigned int shift = (bytes * 8) - filter->bits;
    PRBool found = PR_TRUE;

    PORT_Assert(bytes <= sizeof(unsigned int));

    for (iteration = 0; iteration < filter->k; ++iteration) {
        PORT_Memcpy(((PRUint8 *)&tmp) + (sizeof(tmp) - bytes),
                    hashes, bytes);
        hashes += bytes;
        bitIndex = PR_ntohl(tmp) >> shift;

        mask = 1 << (bitIndex % 8);
        found = found && filter->filter[bitIndex / 8] & mask;
        if (add) {
            filter->filter[bitIndex / 8] |= mask;
        }
    }
    return found;
}

PRBool
sslBloom_Add(sslBloomFilter *filter, const PRUint8 *hashes)
{
    return sslBloom_AddOrCheck(filter, hashes, PR_TRUE);
}

PRBool
sslBloom_Check(sslBloomFilter *filter, const PRUint8 *hashes)
{
    return sslBloom_AddOrCheck(filter, hashes, PR_FALSE);
}

void
sslBloom_Destroy(sslBloomFilter *filter)
{
    PORT_Free(filter->filter);
    PORT_Memset(filter, 0, sizeof(*filter));
}
