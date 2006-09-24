/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* nsJARDirectoryInputStream.cpp
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
 * The Original Code is Mozilla libjar code.
 *
 * The Initial Developer of the Original Code is
 * Jeff Walden <jwalden+code@mit.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsJARDirectoryInputStream.h"
#include "nsNetUtil.h"
#include "nsEscape.h"
#include "nsIFile.h"


/*---------------------------------------------
 *  nsISupports implementation
 *--------------------------------------------*/

NS_IMPL_THREADSAFE_ISUPPORTS1(nsJARDirectoryInputStream, nsIInputStream)

/*----------------------------------------------------------
 * nsJARDirectoryInputStream implementation
 *--------------------------------------------------------*/

NS_IMETHODIMP 
nsJARDirectoryInputStream::Available(PRUint32 *_retval)
{
    if (NS_FAILED(mStatus))
        return mStatus;

    *_retval = mBuffer.Length();
    return NS_OK;
}

NS_IMETHODIMP
nsJARDirectoryInputStream::Read(char* buf, PRUint32 count, PRUint32 *bytesRead)
{
    if (mStatus == NS_BASE_STREAM_CLOSED) {
        *bytesRead = 0;
        return NS_OK;
    }

    if (NS_FAILED(mStatus))
        return mStatus;

    nsresult rv;

    // If the buffer contains data, copy what's there up to the desired amount
    PRUint32 numRead = CopyDataToBuffer(buf, count);

    if (count > 0) {
        // empty the buffer and start writing directory entry lines to it
        mBuffer.Truncate();
        mBufPos = 0;
        PRUint32 arrayLen = mArray.Count();
        for ( ;count > mBuffer.Length(); mArrPos++) {
            // have we consumed all the directory contents?
            if (arrayLen <= mArrPos)
                break;

            // Name
            const char * entryName = mArray[mArrPos]->get();
            PRUint32 entryNameLen = mArray[mArrPos]->Length();
            const char * relativeName = entryName + mDirNameLen;
            PRUint32 relativeNameLen = entryNameLen - mDirNameLen;

            nsCOMPtr<nsIZipEntry> ze;
            rv = mZip->GetEntry(entryName, getter_AddRefs(ze));
            if (NS_FAILED(rv)) return rv;

            // Type
            PRBool isDir = PR_FALSE;
            rv = ze->GetIsDirectory(&isDir);
            if (NS_FAILED(rv)) return rv;

            // Size (real, not compressed)
            PRUint32 itemRealSize = 0;
            rv = ze->GetRealSize(&itemRealSize);
            if (NS_FAILED(rv)) return rv;

            // Last Modified Time
            PRTime lmt = LL_Zero();
            rv = ze->GetLastModifiedTime(&lmt);
            if (NS_FAILED(rv)) return rv;

            PRExplodedTime tm;
            PR_ExplodeTime(lmt, PR_GMTParameters, &tm);
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
            NS_EscapeURL(relativeName, relativeNameLen,
                         esc_Minimal | esc_AlwaysCopy, mBuffer);

            mBuffer.AppendLiteral(" ");
            mBuffer.AppendInt(itemRealSize, 10);
            mBuffer.Append(itemLastModTime); // starts/ends with ' '
            mBuffer.Append(isDir ? "DIRECTORY\n" : "FILE\n");
        }

        // Copy up to the desired amount of data to buffer
        numRead += CopyDataToBuffer(buf, count);
    }

    *bytesRead = numRead;
    return NS_OK;
}

NS_IMETHODIMP
nsJARDirectoryInputStream::ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval)
{
    // XXX write me!
    NS_NOTREACHED("Consumers should be using Read()!");
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsJARDirectoryInputStream::IsNonBlocking(PRBool *aNonBlocking)
{
    *aNonBlocking = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsJARDirectoryInputStream::Close()
{
    mStatus = NS_BASE_STREAM_CLOSED;
    return NS_OK;
}

/* static */ nsresult
nsJARDirectoryInputStream::Create(nsIZipReader* aZip,
                                  const nsACString& aJarDirSpec,
                                  const char* aDir,
                                  nsIInputStream** result)
{
    NS_ENSURE_ARG_POINTER(aZip);
    NS_ENSURE_ARG_POINTER(aDir);
    NS_ENSURE_ARG_POINTER(result);

    nsJARDirectoryInputStream* jdis = new nsJARDirectoryInputStream();
    if (!jdis) return NS_ERROR_OUT_OF_MEMORY;

    // addref now so we can call Init()
    *result = jdis;
    NS_ADDREF(*result);

    nsresult rv = jdis->Init(aZip, aJarDirSpec, aDir);
    if (NS_FAILED(rv)) NS_RELEASE(*result);

    return rv;
}

nsresult
nsJARDirectoryInputStream::Init(nsIZipReader* aZip,
                                const nsACString& aJarDirSpec,
                                const char* aDir)
{
    // Ensure that aDir is really a directory and that it exists.
    // Watch out for the jar:foo.zip!/ (aDir is empty) top-level
    // special case!
    nsresult rv;

    // Keep the zipReader for getting the actual zipItems
    mZip = aZip;

    if (*aDir) {
        nsCOMPtr<nsIZipEntry> ze;
        rv = aZip->GetEntry(aDir, getter_AddRefs(ze));
        if (NS_FAILED(rv)) return rv;

        PRBool isDir;
        rv = ze->GetIsDirectory(&isDir);
        if (NS_FAILED(rv)) return rv;

        if (!isDir)
            return NS_ERROR_ILLEGAL_VALUE;
    }

    // We can get aDir's contents as strings via FindEntries
    // with the following pattern (see nsIZipReader.findEntries docs)
    // assuming dirName is properly escaped:
    //
    //   dirName + "?*~" + dirName + "?*/?*"
    nsDependentCString dirName(aDir);
    mDirNameLen = dirName.Length();

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

    nsCOMPtr<nsIUTF8StringEnumerator> dirEnum;
    rv = aZip->FindEntries(pattern.get(), getter_AddRefs(dirEnum));
    if (NS_FAILED(rv)) return rv;

    PRBool more;
    nsCAutoString entryName;
    while (NS_SUCCEEDED(dirEnum->HasMore(&more)) && more) {
        rv = dirEnum->GetNext(entryName);
        if (NS_SUCCEEDED(rv)) {
            mArray.AppendCString(entryName);
        }
    }

    // Sort it
    mArray.Sort();

    mBuffer.AppendLiteral("300: ");
    mBuffer.Append(aJarDirSpec);
    mBuffer.AppendLiteral("\n200: filename content-length last-modified file-type\n");

    return NS_OK;
}

PRUint32
nsJARDirectoryInputStream::CopyDataToBuffer(char* &aBuffer, PRUint32 &aCount)
{
    PRUint32 writeLength = PR_MIN(aCount, mBuffer.Length() - mBufPos);

    if (writeLength > 0) {
        memcpy(aBuffer, mBuffer.get() + mBufPos, writeLength);
        mBufPos += writeLength;
        aCount  -= writeLength;
        aBuffer += writeLength;
    }

    // return number of bytes copied to the buffer so the
    // Read method can return the number of bytes copied
    return writeLength;
}

//----------------------------------------------
// nsJARDirectoryInputStream constructor and destructor
//----------------------------------------------

nsJARDirectoryInputStream::nsJARDirectoryInputStream()
    : mStatus(NS_OK), mArrPos(0), mBufPos(0)
{
}

nsJARDirectoryInputStream::~nsJARDirectoryInputStream()
{
    Close();
}
