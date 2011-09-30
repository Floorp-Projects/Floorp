/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsManifestLineReader_h__
#define nsManifestLineReader_h__

#include "nspr.h"
#include "nsDebug.h"

class nsManifestLineReader
{
public:
    nsManifestLineReader() : mBase(nsnull) {} 
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
            return PR_FALSE;
        
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
                return PR_TRUE;
            }
            ++mNext;
            ++mLength;
        }
        return PR_FALSE;        
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
