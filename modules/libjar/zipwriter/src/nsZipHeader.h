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

#ifndef _nsZipHeader_h_
#define _nsZipHeader_h_

#include "nsString.h"
#include "nsIOutputStream.h"
#include "nsIInputStream.h"
#include "nsIZipReader.h"
#include "nsAutoPtr.h"

// High word is S_IFREG, low word is DOS file attribute
#define ZIP_ATTRS_FILE 0x80000000
// High word is S_IFDIR, low word is DOS dir attribute
#define ZIP_ATTRS_DIRECTORY 0x40000010
#define PERMISSIONS_FILE 0644
#define PERMISSIONS_DIR 0755

// Combine file type attributes with unix style permissions
#define ZIP_ATTRS(p, a) ((p & 0xfff) << 16) | a

class nsZipHeader : public nsIZipEntry
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
        mInited(PR_FALSE),
        mWriteOnClose(PR_FALSE),
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
    PRPackedBool mInited;
    PRPackedBool mWriteOnClose;
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
    const PRUint8 * GetExtraField(PRUint16 aTag, PRBool aLocal, PRUint16 *aBlockSize);
};

#endif
