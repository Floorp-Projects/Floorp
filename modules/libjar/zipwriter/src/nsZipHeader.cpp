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
 * The Original Code is Zip Writer Component.
 *
 * The Initial Developer of the Original Code is
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * ***** END LICENSE BLOCK *****
 */

#include "StreamFunctions.h"
#include "nsZipHeader.h"
#include "nsMemory.h"

#define ZIP_FILE_HEADER_SIGNATURE 0x04034b50
#define ZIP_FILE_HEADER_SIZE 30
#define ZIP_CDS_HEADER_SIGNATURE 0x02014b50
#define ZIP_CDS_HEADER_SIZE 46

#define FLAGS_IS_UTF8 0x800

#define ZIP_EXTENDED_TIMESTAMP_FIELD 0x5455
#define ZIP_EXTENDED_TIMESTAMP_MODTIME 0x01

/**
 * nsZipHeader represents an entry from a zip file.
 */
NS_IMPL_ISUPPORTS1(nsZipHeader, nsIZipEntry)

/* readonly attribute unsigned short compression; */
NS_IMETHODIMP nsZipHeader::GetCompression(PRUint16 *aCompression)
{
    NS_ASSERTION(mInited, "Not initalised");

    *aCompression = mMethod;
    return NS_OK;
}

/* readonly attribute unsigned long size; */
NS_IMETHODIMP nsZipHeader::GetSize(PRUint32 *aSize)
{
    NS_ASSERTION(mInited, "Not initalised");

    *aSize = mCSize;
    return NS_OK;
}

/* readonly attribute unsigned long realSize; */
NS_IMETHODIMP nsZipHeader::GetRealSize(PRUint32 *aRealSize)
{
    NS_ASSERTION(mInited, "Not initalised");

    *aRealSize = mUSize;
    return NS_OK;
}

/* readonly attribute unsigned long CRC32; */
NS_IMETHODIMP nsZipHeader::GetCRC32(PRUint32 *aCRC32)
{
    NS_ASSERTION(mInited, "Not initalised");

    *aCRC32 = mCRC;
    return NS_OK;
}

/* readonly attribute boolean isDirectory; */
NS_IMETHODIMP nsZipHeader::GetIsDirectory(bool *aIsDirectory)
{
    NS_ASSERTION(mInited, "Not initalised");

    if (mName.Last() == '/')
        *aIsDirectory = PR_TRUE;
    else
        *aIsDirectory = PR_FALSE;
    return NS_OK;
}

/* readonly attribute PRTime lastModifiedTime; */
NS_IMETHODIMP nsZipHeader::GetLastModifiedTime(PRTime *aLastModifiedTime)
{
    NS_ASSERTION(mInited, "Not initalised");

    // Try to read timestamp from extra field
    PRUint16 blocksize;
    const PRUint8 *tsField = GetExtraField(ZIP_EXTENDED_TIMESTAMP_FIELD, PR_FALSE, &blocksize);
    if (tsField && blocksize >= 5) {
        PRUint32 pos = 4;
        PRUint8 flags;
        flags = READ8(tsField, &pos);
        if (flags & ZIP_EXTENDED_TIMESTAMP_MODTIME) {
            *aLastModifiedTime = (PRTime)(READ32(tsField, &pos))
                                 * PR_USEC_PER_SEC;
            return NS_OK;
        }
    }

    // Use DOS date/time fields
    // Note that on DST shift we can't handle correctly the hour that is valid
    // in both DST zones
    PRExplodedTime time;

    time.tm_usec = 0;

    time.tm_hour = (mTime >> 11) & 0x1F;
    time.tm_min = (mTime >> 5) & 0x3F;
    time.tm_sec = (mTime & 0x1F) * 2;

    time.tm_year = (mDate >> 9) + 1980;
    time.tm_month = ((mDate >> 5) & 0x0F) - 1;
    time.tm_mday = mDate & 0x1F;

    time.tm_params.tp_gmt_offset = 0;
    time.tm_params.tp_dst_offset = 0;

    PR_NormalizeTime(&time, PR_GMTParameters);
    time.tm_params.tp_gmt_offset = PR_LocalTimeParameters(&time).tp_gmt_offset;
    PR_NormalizeTime(&time, PR_GMTParameters);
    time.tm_params.tp_dst_offset = PR_LocalTimeParameters(&time).tp_dst_offset;

    *aLastModifiedTime = PR_ImplodeTime(&time);

    return NS_OK;
}

/* readonly attribute boolean isSynthetic; */
NS_IMETHODIMP nsZipHeader::GetIsSynthetic(bool *aIsSynthetic)
{
    NS_ASSERTION(mInited, "Not initalised");

    *aIsSynthetic = PR_FALSE;
    return NS_OK;
}

void nsZipHeader::Init(const nsACString & aPath, PRTime aDate, PRUint32 aAttr,
                       PRUint32 aOffset)
{
    NS_ASSERTION(!mInited, "Already initalised");

    PRExplodedTime time;
    PR_ExplodeTime(aDate, PR_LocalTimeParameters, &time);

    mTime = time.tm_sec / 2 + (time.tm_min << 5) + (time.tm_hour << 11);
    mDate = time.tm_mday + ((time.tm_month + 1) << 5) +
            ((time.tm_year - 1980) << 9);

    // Store modification timestamp as extra field
    // First fill CDS extra field
    mFieldLength = 9;
    mExtraField = new PRUint8[mFieldLength];
    if (!mExtraField) {
        mFieldLength = 0;
    } else {
        PRUint32 pos = 0;
        WRITE16(mExtraField.get(), &pos, ZIP_EXTENDED_TIMESTAMP_FIELD);
        WRITE16(mExtraField.get(), &pos, 5);
        WRITE8(mExtraField.get(), &pos, ZIP_EXTENDED_TIMESTAMP_MODTIME);
        WRITE32(mExtraField.get(), &pos, aDate / PR_USEC_PER_SEC);

        // Fill local extra field
        mLocalExtraField = new PRUint8[mFieldLength];
        if (mLocalExtraField) {
            mLocalFieldLength = mFieldLength;
            memcpy(mLocalExtraField.get(), mExtraField.get(), mLocalFieldLength);
        }
    }

    mEAttr = aAttr;
    mOffset = aOffset;
    mName = aPath;
    mComment = NS_LITERAL_CSTRING("");
    // Claim a UTF-8 path in case it needs it.
    mFlags |= FLAGS_IS_UTF8;
    mInited = PR_TRUE;
}

PRUint32 nsZipHeader::GetFileHeaderLength()
{
    return ZIP_FILE_HEADER_SIZE + mName.Length() + mLocalFieldLength;
}

nsresult nsZipHeader::WriteFileHeader(nsIOutputStream *aStream)
{
    NS_ASSERTION(mInited, "Not initalised");

    PRUint8 buf[ZIP_FILE_HEADER_SIZE];
    PRUint32 pos = 0;
    WRITE32(buf, &pos, ZIP_FILE_HEADER_SIGNATURE);
    WRITE16(buf, &pos, mVersionNeeded);
    WRITE16(buf, &pos, mFlags);
    WRITE16(buf, &pos, mMethod);
    WRITE16(buf, &pos, mTime);
    WRITE16(buf, &pos, mDate);
    WRITE32(buf, &pos, mCRC);
    WRITE32(buf, &pos, mCSize);
    WRITE32(buf, &pos, mUSize);
    WRITE16(buf, &pos, mName.Length());
    WRITE16(buf, &pos, mLocalFieldLength);

    nsresult rv = ZW_WriteData(aStream, (const char *)buf, pos);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ZW_WriteData(aStream, mName.get(), mName.Length());
    NS_ENSURE_SUCCESS(rv, rv);

    if (mLocalFieldLength)
    {
      rv = ZW_WriteData(aStream, (const char *)mLocalExtraField.get(), mLocalFieldLength);
      NS_ENSURE_SUCCESS(rv, rv);
    }

    return NS_OK;
}

PRUint32 nsZipHeader::GetCDSHeaderLength()
{
    return ZIP_CDS_HEADER_SIZE + mName.Length() + mComment.Length() +
           mFieldLength;
}

nsresult nsZipHeader::WriteCDSHeader(nsIOutputStream *aStream)
{
    NS_ASSERTION(mInited, "Not initalised");

    PRUint8 buf[ZIP_CDS_HEADER_SIZE];
    PRUint32 pos = 0;
    WRITE32(buf, &pos, ZIP_CDS_HEADER_SIGNATURE);
    WRITE16(buf, &pos, mVersionMade);
    WRITE16(buf, &pos, mVersionNeeded);
    WRITE16(buf, &pos, mFlags);
    WRITE16(buf, &pos, mMethod);
    WRITE16(buf, &pos, mTime);
    WRITE16(buf, &pos, mDate);
    WRITE32(buf, &pos, mCRC);
    WRITE32(buf, &pos, mCSize);
    WRITE32(buf, &pos, mUSize);
    WRITE16(buf, &pos, mName.Length());
    WRITE16(buf, &pos, mFieldLength);
    WRITE16(buf, &pos, mComment.Length());
    WRITE16(buf, &pos, mDisk);
    WRITE16(buf, &pos, mIAttr);
    WRITE32(buf, &pos, mEAttr);
    WRITE32(buf, &pos, mOffset);

    nsresult rv = ZW_WriteData(aStream, (const char *)buf, pos);
    NS_ENSURE_SUCCESS(rv, rv);

    rv = ZW_WriteData(aStream, mName.get(), mName.Length());
    NS_ENSURE_SUCCESS(rv, rv);
    if (mExtraField) {
        rv = ZW_WriteData(aStream, (const char *)mExtraField.get(), mFieldLength);
        NS_ENSURE_SUCCESS(rv, rv);
    }
    return ZW_WriteData(aStream, mComment.get(), mComment.Length());
}

nsresult nsZipHeader::ReadCDSHeader(nsIInputStream *stream)
{
    NS_ASSERTION(!mInited, "Already initalised");

    PRUint8 buf[ZIP_CDS_HEADER_SIZE];

    nsresult rv = ZW_ReadData(stream, (char *)buf, ZIP_CDS_HEADER_SIZE);
    NS_ENSURE_SUCCESS(rv, rv);

    PRUint32 pos = 0;
    PRUint32 signature = READ32(buf, &pos);
    if (signature != ZIP_CDS_HEADER_SIGNATURE)
        return NS_ERROR_FILE_CORRUPTED;

    mVersionMade = READ16(buf, &pos);
    mVersionNeeded = READ16(buf, &pos);
    mFlags = READ16(buf, &pos);
    mMethod = READ16(buf, &pos);
    mTime = READ16(buf, &pos);
    mDate = READ16(buf, &pos);
    mCRC = READ32(buf, &pos);
    mCSize = READ32(buf, &pos);
    mUSize = READ32(buf, &pos);
    PRUint16 namelength = READ16(buf, &pos);
    mFieldLength = READ16(buf, &pos);
    PRUint16 commentlength = READ16(buf, &pos);
    mDisk = READ16(buf, &pos);
    mIAttr = READ16(buf, &pos);
    mEAttr = READ32(buf, &pos);
    mOffset = READ32(buf, &pos);

    if (namelength > 0) {
        nsAutoArrayPtr<char> field(new char[namelength]);
        NS_ENSURE_TRUE(field, NS_ERROR_OUT_OF_MEMORY);
        rv = ZW_ReadData(stream, field.get(), namelength);
        NS_ENSURE_SUCCESS(rv, rv);
        mName.Assign(field, namelength);
    }
    else
        mName = NS_LITERAL_CSTRING("");

    if (mFieldLength > 0) {
        mExtraField = new PRUint8[mFieldLength];
        NS_ENSURE_TRUE(mExtraField, NS_ERROR_OUT_OF_MEMORY);
        rv = ZW_ReadData(stream, (char *)mExtraField.get(), mFieldLength);
        NS_ENSURE_SUCCESS(rv, rv);
    }

    if (commentlength > 0) {
        nsAutoArrayPtr<char> field(new char[commentlength]);
        NS_ENSURE_TRUE(field, NS_ERROR_OUT_OF_MEMORY);
        rv = ZW_ReadData(stream, field.get(), commentlength);
        NS_ENSURE_SUCCESS(rv, rv);
        mComment.Assign(field, commentlength);
    }
    else
        mComment = NS_LITERAL_CSTRING("");

    mInited = PR_TRUE;
    return NS_OK;
}

const PRUint8 * nsZipHeader::GetExtraField(PRUint16 aTag, bool aLocal, PRUint16 *aBlockSize)
{
    const PRUint8 *buf = aLocal ? mLocalExtraField : mExtraField;
    PRUint32 buflen = aLocal ? mLocalFieldLength : mFieldLength;
    PRUint32 pos = 0;
    PRUint16 tag, blocksize;

    while (buf && (pos + 4) <= buflen) {
      tag = READ16(buf, &pos);
      blocksize = READ16(buf, &pos);

      if (aTag == tag && (pos + blocksize) <= buflen) {
        *aBlockSize = blocksize;
        return buf + pos - 4;
      }

      pos += blocksize;
    }

    return NULL;
}
