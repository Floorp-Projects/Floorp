/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL. You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All Rights
 * Reserved.
 */


/*

  The converts a filesystem directory into an "HTTP index" stream.

 */

#include "nsEscape.h"
#include "nsDirectoryIndexStream.h"
#include "prio.h"


nsDirectoryIndexStream::nsDirectoryIndexStream()
    : mOffset(0),
      mIter(nsnull)
{
    NS_INIT_REFCNT();
}


nsresult
nsDirectoryIndexStream::Init(const nsFileSpec& aDir)
{
    NS_PRECONDITION(aDir.IsDirectory(), "not a directory");
    if (! aDir.IsDirectory())
        return NS_ERROR_ILLEGAL_VALUE;

    mDir = aDir;

    // Sigh. We have to allocate on the heap because there are no
    // assignment operators defined.
    mIter = new nsDirectoryIterator(mDir, PR_FALSE);		// rjc: don't resolve aliases
    if (! mIter)
        return NS_ERROR_OUT_OF_MEMORY;

    mBuf  = "200: filename content-length last-modified file-type\n";
    return NS_OK;
}


nsDirectoryIndexStream::~nsDirectoryIndexStream()
{
    delete mIter;
}



nsresult
nsDirectoryIndexStream::Create(const nsFileSpec& aDir, nsISupports** aResult)
{
    nsDirectoryIndexStream* result = new nsDirectoryIndexStream();
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init(aDir);
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}

NS_IMPL_ADDREF(nsDirectoryIndexStream);
NS_IMPL_RELEASE(nsDirectoryIndexStream);

NS_IMETHODIMP
nsDirectoryIndexStream::QueryInterface(REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(nsCOMTypeInfo<nsIInputStream>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsIBaseStream>::GetIID()) ||
        aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
        *aResult = NS_STATIC_CAST(nsIInputStream*, this);
        NS_ADDREF(this);
        return NS_OK;
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }
}


NS_IMETHODIMP
nsDirectoryIndexStream::Close()
{
    return NS_OK;
}



NS_IMETHODIMP
nsDirectoryIndexStream::GetLength(PRUint32* aLength)
{
    // Lie, and tell the caller that the stream is endless (until we
    // actually don't have anything left).
    if (mIter->Exists()) {
        *aLength = PRUint32(-1);
        return NS_OK;
    }
    else {
        *aLength = 0;
        return NS_BASE_STREAM_EOF;
    }
}


NS_IMETHODIMP
nsDirectoryIndexStream::Read(char* aBuf, PRUint32 aCount, PRUint32* aReadCount)
{
    PRUint32 nread = 0;

    // If anything is enqueued (or left-over) in mBuf, then feed it to
    // the reader first.
    while (mOffset < mBuf.Length() && aCount != 0) {
        *(aBuf++) = char(mBuf.CharAt(mOffset++));
        --aCount;
        ++nread;
    }

    // Room left?
    if (aCount > 0) {
        mOffset = 0;
        mBuf.Truncate();

        // Okay, now we'll suck stuff off of our iterator into the mBuf...
        while (PRUint32(mBuf.Length()) < aCount && mIter->Exists()) {
            nsFileSpec current = mIter->Spec();
            ++(*mIter);

		// rjc: don't return hidden files/directories!
		if (current.IsHidden())	continue;

            PRFileInfo fileinfo;
            PRStatus status = PR_GetFileInfo(nsNSPRPath(current), &fileinfo);
            if (status != PR_SUCCESS)
                continue;

            mBuf += "201: ";

            // The "filename" field
            {
                char* leafname = current.GetLeafName();
                if (leafname) {
                    char* escaped = nsEscape(leafname, url_Path);
                    if (escaped) {
                        mBuf += escaped;
                        mBuf.Append(' ');
                        delete[] escaped; // XXX!
                    }
                    nsCRT::free(leafname);
                }
            }

            // The "content-length" field
            mBuf.Append(fileinfo.size, 10);
            mBuf.Append(' ');

            // The "last-modified" field
            PRExplodedTime tm;
            PR_ExplodeTime(fileinfo.modifyTime, PR_GMTParameters, &tm);
            {
                char buf[64];
                PR_FormatTimeUSEnglish(buf, sizeof(buf), "%a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ", &tm);
                mBuf.Append(buf);
            }

            // The "file-type" field
            if (current.IsFile()) {
                mBuf += "FILE ";
            }
            else if (current.IsDirectory()) {
                mBuf += "DIRECTORY ";
            }
            else if (current.IsSymlink()) {
                mBuf += "SYMBOLIC-LINK ";
            }

            mBuf.Append('\n');
        }

        // ...and once we've either run out of directory entries, or
        // filled up the buffer, then we'll push it to the reader.
        while (mOffset < mBuf.Length() && aCount != 0) {
            *(aBuf++) = char(mBuf.CharAt(mOffset++));
            --aCount;
            ++nread;
        }
    }

    *aReadCount = nread;
    return NS_OK;
}
