/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* nsJARInputStream.cpp
 * 
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Netscape Communicator source code. 
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mitch Stoltz <mstoltz@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsJARInputStream.h"
#include "zipstruct.h"         // defines ZIP compression codes
#include "nsZipArchive.h"

#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsIFile.h"

/*---------------------------------------------
 *  nsISupports implementation
 *--------------------------------------------*/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJARInputStream, nsIInputStream)

/*----------------------------------------------------------
 * nsJARInputStream implementation
 *--------------------------------------------------------*/

nsresult
nsJARInputStream::InitFile(nsZipArchive* aZip, nsZipItem *item, PRFileDesc *fd)
{
    nsresult rv;

    // Keep the file handle, even on failure
    mFd = fd;
      
    NS_ENSURE_ARG_POINTER(aZip);
    NS_ENSURE_ARG_POINTER(item);
    NS_ENSURE_ARG_POINTER(fd);

    // Mark it as closed, in case something fails in initialisation
    mClosed = PR_TRUE;

    // Keep the important bits of nsZipItem only
    mInSize = item->size;
 
    //-- prepare for the compression type
    switch (item->compression) {
       case STORED: 
           break;

       case DEFLATED:
           mInflate = (InflateStruct *) PR_Malloc(sizeof(InflateStruct));
           NS_ENSURE_TRUE(mInflate, NS_ERROR_OUT_OF_MEMORY);
    
           rv = gZlibInit(&(mInflate->mZs));
           NS_ENSURE_SUCCESS(rv, NS_ERROR_OUT_OF_MEMORY);
    
           mInflate->mOutSize = item->realsize;
           mInflate->mInCrc = item->crc32;
           mInflate->mOutCrc = crc32(0L, Z_NULL, 0);
           break;

       default:
           return NS_ERROR_NOT_IMPLEMENTED;
    }
   
    //-- Set filepointer to start of item
    rv = aZip->SeekToItem(item, mFd);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_FILE_CORRUPTED);
        
    // Open for reading
    mClosed = PR_FALSE;
    mCurPos = 0;
    return NS_OK;
}

nsresult
nsJARInputStream::InitDirectory(nsZipArchive* aZip,
                                const nsACString& aJarDirSpec,
                                const char* aDir)
{
    NS_ENSURE_ARG_POINTER(aZip);
    NS_ENSURE_ARG_POINTER(aDir);

    // Mark it as closed, in case something fails in initialisation
    mClosed = PR_TRUE;
    mDirectory = PR_TRUE;
    
    // Keep the zipReader for getting the actual zipItems
    mZip = aZip;
    nsZipFind *find;
    nsresult rv;
    // We can get aDir's contents as strings via FindEntries
    // with the following pattern (see nsIZipReader.findEntries docs)
    // assuming dirName is properly escaped:
    //
    //   dirName + "?*~" + dirName + "?*/?*"
    nsDependentCString dirName(aDir);
    mNameLen = dirName.Length();

    // iterate through dirName and copy it to escDirName, escaping chars
    // which are special at the "top" level of the regexp so FindEntries
    // works correctly
    nsCAutoString escDirName;
    const char* curr = dirName.BeginReading();
    const char* end  = dirName.EndReading();
    while (curr != end) {
        switch (*curr) {
            case '*':
            case '?':
            case '$':
            case '[':
            case ']':
            case '^':
            case '~':
            case '(':
            case ')':
            case '\\':
                escDirName.Append('\\');
                // fall through
            default:
                escDirName.Append(*curr);
        }
        ++curr;
    }
    nsCAutoString pattern = escDirName + NS_LITERAL_CSTRING("?*~") +
                            escDirName + NS_LITERAL_CSTRING("?*/?*");
    rv = aZip->FindInit(pattern.get(), &find);
    if (NS_FAILED(rv)) return rv;

    const char *name;
    while ((rv = find->FindNext( &name )) == NS_OK) {
        // No need to copy string, just share the one from nsZipArchive
        mArray.AppendCString(nsDependentCString(name));
    }
    delete find;

    if (rv != NS_ERROR_FILE_TARGET_DOES_NOT_EXIST && NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;    // no error translation
    }

    // Sort it
    mArray.Sort();

    mBuffer.AssignLiteral("300: ");
    mBuffer.Append(aJarDirSpec);
    mBuffer.AppendLiteral("\n200: filename content-length last-modified file-type\n");

    // Open for reading
    mClosed = PR_FALSE;
    mCurPos = 0;
    mArrPos = 0;
    return NS_OK;
}

NS_IMETHODIMP 
nsJARInputStream::Available(PRUint32 *_retval)
{
    if (mClosed)
        return NS_BASE_STREAM_CLOSED;

    if (mDirectory)
        *_retval = mBuffer.Length();
    else if (mInflate) 
        *_retval = mInflate->mOutSize - mInflate->mZs.total_out;
    else 
        *_retval = mInSize - mCurPos;
    return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Read(char* aBuffer, PRUint32 aCount, PRUint32 *aBytesRead)
{
    NS_ENSURE_ARG_POINTER(aBuffer);
    NS_ENSURE_ARG_POINTER(aBytesRead);

    *aBytesRead = 0;

    nsresult rv = NS_OK;
    if (mClosed)
        return rv;

    if (mDirectory) {
        rv = ReadDirectory(aBuffer, aCount, aBytesRead);
    } else {
        if (mInflate) {
            rv = ContinueInflate(aBuffer, aCount, aBytesRead);
        } else {
            PRInt32 bytesRead = 0;
            aCount = PR_MIN(aCount, mInSize - mCurPos);
            if (aCount) {        
                bytesRead = PR_Read(mFd, aBuffer, aCount);
                if (bytesRead < 0)
                    return NS_ERROR_FILE_CORRUPTED;
                mCurPos += bytesRead;
            }
            *aBytesRead = bytesRead;
        }
        
        // be aggressive about closing!
        // note that sometimes, we will close mFd before we've finished
        // deflating - this is because zlib buffers the input
        // So, don't free the ReadBuf/InflateStruct yet.
        if (mCurPos >= mInSize && mFd) {
            PR_Close(mFd);
            mFd = nsnull;
        }
    }
    return rv;
}

NS_IMETHODIMP
nsJARInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    // don't have a buffer to read from, so this better not be called!
    NS_NOTREACHED("Consumers should be using Read()!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsJARInputStream::Close()
{
    PR_FREEIF(mInflate);
    if (mFd) {
        PR_Close(mFd);
        mFd = nsnull;
    }
    mClosed = PR_TRUE;
    return NS_OK;
}

nsresult 
nsJARInputStream::ContinueInflate(char* aBuffer, PRUint32 aCount,
                                  PRUint32* aBytesRead)
{
    // No need to check the args, ::Read did that, but assert them at least
    NS_ASSERTION(mInflate,"inflate data structure missing");
    NS_ASSERTION(aBuffer,"aBuffer parameter must not be null");
    NS_ASSERTION(aBytesRead,"aBytesRead parameter must not be null");

    // Keep old total_out count
    const PRUint32 oldTotalOut = mInflate->mZs.total_out;
    
    // make sure we aren't reading too much
    mInflate->mZs.avail_out = (mInflate->mOutSize-oldTotalOut > aCount) ? aCount : mInflate->mOutSize-oldTotalOut;
    mInflate->mZs.next_out = (unsigned char*)aBuffer;

    int zerr = Z_OK;
    //-- inflate loop
    while (mInflate->mZs.avail_out > 0 && zerr == Z_OK) {

        if (mInflate->mZs.avail_in == 0 && mCurPos < mInSize) {
            // time to fill the buffer!
            PRUint32 bytesToRead = PR_MIN(mInSize - mCurPos, ZIP_BUFLEN);

            NS_ASSERTION(mFd, "File handle missing");
            PRInt32 bytesRead = PR_Read(mFd, mInflate->mReadBuf, bytesToRead);
            if (bytesRead < 0) {
                zerr = Z_ERRNO;
                break;
            }
            mCurPos += bytesRead;

            // now set the state for 'inflate'
            mInflate->mZs.next_in = mInflate->mReadBuf;
            mInflate->mZs.avail_in = bytesRead;
        }

        // now inflate
        zerr = inflate(&(mInflate->mZs), Z_SYNC_FLUSH);
    }

    if ((zerr != Z_OK) && (zerr != Z_STREAM_END))
        return NS_ERROR_FILE_CORRUPTED;

    *aBytesRead = (mInflate->mZs.total_out - oldTotalOut);

    // Calculate the CRC on the output
    mInflate->mOutCrc = crc32(mInflate->mOutCrc, (unsigned char*)aBuffer, *aBytesRead);

    // be aggressive about ending the inflation
    // for some reason we don't always get Z_STREAM_END
    if (zerr == Z_STREAM_END || mInflate->mZs.total_out == mInflate->mOutSize) {
        inflateEnd(&(mInflate->mZs));

        // stop returning valid data as soon as we know we have a bad CRC
        if (mInflate->mOutCrc != mInflate->mInCrc) {
            // asserting because while this rarely happens, you definitely
            // want to catch it in debug builds!
            NS_NOTREACHED(0);
            return NS_ERROR_FILE_CORRUPTED;
        }
    }

    return NS_OK;
}

nsresult
nsJARInputStream::ReadDirectory(char* aBuffer, PRUint32 aCount, PRUint32 *aBytesRead)
{
    // No need to check the args, ::Read did that, but assert them at least
    NS_ASSERTION(aBuffer,"aBuffer parameter must not be null");
    NS_ASSERTION(aBytesRead,"aBytesRead parameter must not be null");

    // If the buffer contains data, copy what's there up to the desired amount
    PRUint32 numRead = CopyDataToBuffer(aBuffer, aCount);

    if (aCount > 0) {
        // empty the buffer and start writing directory entry lines to it
        mBuffer.Truncate();
        mCurPos = 0;
        const PRUint32 arrayLen = mArray.Count();

        for ( ;aCount > mBuffer.Length(); mArrPos++) {
            // have we consumed all the directory contents?
            if (arrayLen <= mArrPos)
                break;

            const char * entryName = mArray[mArrPos]->get();
            PRUint32 entryNameLen = mArray[mArrPos]->Length();
            nsZipItem* ze = mZip->GetItem(entryName);
            NS_ENSURE_TRUE(ze, NS_ERROR_FILE_TARGET_DOES_NOT_EXIST);

            // Last Modified Time
            PRExplodedTime tm;
            PR_ExplodeTime(GetModTime(ze->date, ze->time), PR_GMTParameters, &tm);
            char itemLastModTime[65];
            PR_FormatTimeUSEnglish(itemLastModTime,
                                   sizeof(itemLastModTime),
                                   " %a,%%20%d%%20%b%%20%Y%%20%H:%M:%S%%20GMT ",
                                   &tm);

            // write a 201: line to the buffer for this item
            // 200: filename content-length last-modified file-type
            mBuffer.AppendLiteral("201: ");

            // Names must be escaped and relative, so use the pre-calculated length
            // of the directory name as the offset into the string
            // NS_EscapeURL adds the escaped URL to the give string buffer
            NS_EscapeURL(entryName + mNameLen,
                         entryNameLen - mNameLen, 
                         esc_Minimal | esc_AlwaysCopy,
                         mBuffer);

            mBuffer.Append(' ');
            mBuffer.AppendInt(ze->realsize, 10);
            mBuffer.Append(itemLastModTime); // starts/ends with ' '
            if (ze->isDirectory) 
                mBuffer.AppendLiteral("DIRECTORY\n");
            else
                mBuffer.AppendLiteral("FILE\n");
        }

        // Copy up to the desired amount of data to buffer
        numRead += CopyDataToBuffer(aBuffer, aCount);
    }

    *aBytesRead = numRead;
    return NS_OK;
}

PRUint32
nsJARInputStream::CopyDataToBuffer(char* &aBuffer, PRUint32 &aCount)
{
    const PRUint32 writeLength = PR_MIN(aCount, mBuffer.Length() - mCurPos);

    if (writeLength > 0) {
        memcpy(aBuffer, mBuffer.get() + mCurPos, writeLength);
        mCurPos += writeLength;
        aCount  -= writeLength;
        aBuffer += writeLength;
    }

    // return number of bytes copied to the buffer so the
    // Read method can return the number of bytes copied
    return writeLength;
}
