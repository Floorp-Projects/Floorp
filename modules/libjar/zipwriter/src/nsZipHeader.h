/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#ifndef _nsZipHeader_h_
#define _nsZipHeader_h_

#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIZipReader.h"
#include "nsAutoPtr.h"
#include "mozilla/Attributes.h"

// High word is S_IFREG, low word is DOS file attribute
#define ZIP_ATTRS_FILE 0x80000000
// High word is S_IFDIR, low word is DOS dir attribute
#define ZIP_ATTRS_DIRECTORY 0x40000010
#define PERMISSIONS_FILE 0644
#define PERMISSIONS_DIR 0755

// Combine file type attributes with unix style permissions
#define ZIP_ATTRS(p, a) ((p & 0xfff) << 16) | a

class nsZipHeader MOZ_FINAL : public nsIZipEntry
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIZIPENTRY

    nsZipHeader() :
        mCRC(0),
        mCSize(0),
        mUSize(0),
        mEAttr(0),
        mOffset(0),
        mFieldLength(0),
        mLocalFieldLength(0),
        mVersionMade(0x0300 + 23), // Generated on Unix by v2.3 (matches infozip)
        mVersionNeeded(20), // Requires v2.0 to extract
        mFlags(0),
        mMethod(0),
        mTime(0),
        mDate(0),
        mDisk(0),
        mIAttr(0),
        mInited(false),
        mWriteOnClose(false),
        mExtraField(NULL),
        mLocalExtraField(NULL)
    {
    }

    ~nsZipHeader()
    {
        mExtraField = NULL;
        mLocalExtraField = NULL;
    }

    PRUint32 mCRC;
    PRUint32 mCSize;
    PRUint32 mUSize;
    PRUint32 mEAttr;
    PRUint32 mOffset;
    PRUint32 mFieldLength;
    PRUint32 mLocalFieldLength;
    PRUint16 mVersionMade;
    PRUint16 mVersionNeeded;
    PRUint16 mFlags;
    PRUint16 mMethod;
    PRUint16 mTime;
    PRUint16 mDate;
    PRUint16 mDisk;
    PRUint16 mIAttr;
    bool mInited;
    bool mWriteOnClose;
    nsCString mName;
    nsCString mComment;
    nsAutoArrayPtr<PRUint8> mExtraField;
    nsAutoArrayPtr<PRUint8> mLocalExtraField;

    void Init(const nsACString & aPath, PRTime aDate, PRUint32 aAttr,
              PRUint32 aOffset);
    PRUint32 GetFileHeaderLength();
    nsresult WriteFileHeader(nsIOutputStream *aStream);
    PRUint32 GetCDSHeaderLength();
    nsresult WriteCDSHeader(nsIOutputStream *aStream);
    nsresult ReadCDSHeader(nsIInputStream *aStream);
    const PRUint8 * GetExtraField(PRUint16 aTag, bool aLocal, PRUint16 *aBlockSize);
};

#endif
