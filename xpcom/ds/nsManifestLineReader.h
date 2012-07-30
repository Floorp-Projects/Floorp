/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsManifestLineReader_h__
#define nsManifestLineReader_h__

#include "nspr.h"
#include "nsDebug.h"

class nsManifestLineReader
{
public:
    nsManifestLineReader() : mBase(nullptr) {} 
    ~nsManifestLineReader() {}

    void Init(char* base, PRUint32 flen) 
    {
        mBase = mCur = mNext = base; 
        mLength = 0;
        mLimit = base + flen;
    }

    bool NextLine()
    {
        if(mNext >= mLimit)
            return false;
        
        mCur = mNext;
        mLength = 0;
        
        while(mNext < mLimit)
        {
            if(IsEOL(*mNext))
            {
                *mNext = '\0';
                for(++mNext; mNext < mLimit; ++mNext)
                    if(!IsEOL(*mNext))
                        break;
                return true;
            }
            ++mNext;
            ++mLength;
        }
        return false;        
    }

    int ParseLine(char** chunks, int* lengths, int maxChunks)
    {
        NS_ASSERTION(mCur && maxChunks && chunks, "bad call to ParseLine");
        int found = 0;
        chunks[found++] = mCur;

        if(found < maxChunks)
        {
            char *lastchunk = mCur;
            int *lastlength = lengths;
            for(char* cur = mCur; *cur; cur++)
            {
                if(*cur == ',')
                {
                    *cur = 0;
                    // always fill in the previous chunk's length
                    *lastlength++ = cur - lastchunk;
                    chunks[found++] = lastchunk = cur+1;
                    if(found == maxChunks)
                        break;
                }
            }
            // crazy pointer math - calculate the length of the final chunk
            *lastlength = (mCur + mLength) - lastchunk;
        }
        return found;
    }

    char*       LinePtr() {return mCur;}    
    PRUint32    LineLength() {return mLength;}    

    bool        IsEOL(char c) {return c == '\n' || c == '\r';}
private:
    char*       mCur;
    PRUint32    mLength;
    char*       mNext;
    char*       mBase;
    char*       mLimit;
};

#endif
