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

class nsHttpChunkedDecoder
{
public:
    nsHttpChunkedDecoder() : mChunkRemaining(0), mReachedEOF(0) {}
   ~nsHttpChunkedDecoder() {}

    PRBool ReachedEOF() { return mReachedEOF; }

    // Called by the transaction to handle chunked content.
    nsresult HandleChunkedContent(char *buf,
                                  PRUint32 count,
                                  PRUint32 *countRead);
private:
    nsresult ParseChunkRemaining(char *buf,
                                 PRUint32 count,
                                 PRUint32 *countRead);

private:
    PRUint32     mChunkRemaining;
    nsCString    mLineBuf; // may hold a partial line
    PRPackedBool mReachedEOF;
};

#endif
