/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * A bloom filter.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __sslbloom_h_
#define __sslbloom_h_

#include "prtypes.h"
#include "seccomon.h"

typedef struct sslBloomFilterStr {
    unsigned int k;    /* The number of hashes. */
    unsigned int bits; /* The number of bits in each hash: bits = log2(m) */
    PRUint8 *filter;   /* The filter itself. */
} sslBloomFilter;

SECStatus sslBloom_Init(sslBloomFilter *filter, unsigned int k, unsigned int bits);
void sslBloom_Zero(sslBloomFilter *filter);
void sslBloom_Fill(sslBloomFilter *filter);
/* Add the given hashes to the filter.  It's the caller's responsibility to
 * ensure that there is at least |ceil(k*bits/8)| bytes of data available in
 * |hashes|. Returns PR_TRUE if the entry was already present or it was likely
 * to be present. */
PRBool sslBloom_Add(sslBloomFilter *filter, const PRUint8 *hashes);
PRBool sslBloom_Check(sslBloomFilter *filter, const PRUint8 *hashes);
void sslBloom_Destroy(sslBloomFilter *filter);

#endif /* __sslbloom_h_ */
