/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsHttpChunkedDecoder_h__
#define nsHttpChunkedDecoder_h__

#include "nsHttp.h"
#include "nsError.h"
#include "nsString.h"
#include "nsHttpHeaderArray.h"

class nsHttpChunkedDecoder
{
public:
    nsHttpChunkedDecoder() : mTrailers(nsnull)
                           , mChunkRemaining(0)
                           , mReachedEOF(false)
                           , mWaitEOF(false) {}
   ~nsHttpChunkedDecoder() { delete mTrailers; }

    bool ReachedEOF() { return mReachedEOF; }

    // called by the transaction to handle chunked content.
    nsresult HandleChunkedContent(char *buf,
                                  PRUint32 count,
                                  PRUint32 *contentRead,
                                  PRUint32 *contentRemaining);

    nsHttpHeaderArray *Trailers() { return mTrailers; }

    nsHttpHeaderArray *TakeTrailers() { nsHttpHeaderArray *h = mTrailers;
                                        mTrailers = nsnull;
                                        return h; }

    PRUint32 GetChunkRemaining() { return mChunkRemaining; }

private:
    nsresult ParseChunkRemaining(char *buf,
                                 PRUint32 count,
                                 PRUint32 *countRead);

private:
    nsHttpHeaderArray *mTrailers;
    PRUint32           mChunkRemaining;
    nsCString          mLineBuf; // may hold a partial line
    bool               mReachedEOF;
    bool               mWaitEOF;
};

#endif
