/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#ifndef nsHttpChunkedDecoder_h__
#define nsHttpChunkedDecoder_h__

#include "nsError.h"
#include "nsString.h"
#include "nsHttpHeaderArray.h"

class nsHttpChunkedDecoder
{
public:
    nsHttpChunkedDecoder() : mTrailers(nsnull)
                           , mChunkRemaining(0)
                           , mReachedEOF(PR_FALSE)
                           , mWaitEOF(PR_FALSE) {}
   ~nsHttpChunkedDecoder() { delete mTrailers; }

    PRBool ReachedEOF() { return mReachedEOF; }

    // called by the transaction to handle chunked content.
    nsresult HandleChunkedContent(char *buf,
                                  PRUint32 count,
                                  PRUint32 *countRead);

    nsHttpHeaderArray *Trailers() { return mTrailers; }

    nsHttpHeaderArray *TakeTrailers() { nsHttpHeaderArray *h = mTrailers;
                                        mTrailers = nsnull;
                                        return h; }

private:
    nsresult ParseChunkRemaining(char *buf,
                                 PRUint32 count,
                                 PRUint32 *countRead);

private:
    nsHttpHeaderArray *mTrailers;
    PRUint32           mChunkRemaining;
    nsCString          mLineBuf; // may hold a partial line
    PRPackedBool       mReachedEOF;
    PRPackedBool       mWaitEOF;
};

#endif
