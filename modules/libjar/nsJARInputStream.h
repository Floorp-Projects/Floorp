/* nsJARInputStream.h
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

#ifndef nsJARINPUTSTREAM_h__
#define nsJARINPUTSTREAM_h__

#include "nsIInputStream.h"
#include "nsJAR.h"

/*-------------------------------------------------------------------------
 * Class nsJARInputStream declaration. This class defines the type of the
 * object returned by calls to nsJAR::GetInputStream(filename) for the
 * purpose of reading a file item out of a JAR file. 
 *------------------------------------------------------------------------*/
class nsJARInputStream : public nsIInputStream
{
  public:
    nsJARInputStream() : 
        mFd(nsnull), mInSize(0), mCurPos(0),
        mClosed(PR_FALSE), mInflate(nsnull), mDirectory(0) { }
    
    ~nsJARInputStream() { Close(); }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
   
    // takes ownership of |fd|, even on failure
    nsresult InitFile(nsZipArchive* aZip, nsZipItem *item, PRFileDesc *fd);

    nsresult InitDirectory(nsZipArchive* aZip,
                           const nsACString& aJarDirSpec,
                           const char* aDir);
  
  private:
    PRFileDesc*   mFd;              // My own file handle, for reading
    PRUint32      mInSize;          // Size in original file 
    PRUint32      mCurPos;          // Current position in input 

    struct InflateStruct {
        PRUint32      mOutSize;     // inflated size 
        PRUint32      mInCrc;       // CRC as provided by the zipentry
        PRUint32      mOutCrc;      // CRC as calculated by me
        z_stream      mZs;          // zip data structure
        unsigned char mReadBuf[ZIP_BUFLEN]; // Readbuffer to inflate from
    };
    struct InflateStruct *   mInflate;

    /* For directory reading */
    nsZipArchive*           mZip;        // the zipReader
    PRUint32                mNameLen; // length of dirname
    nsCAutoString           mBuffer;  // storage for generated text of stream
    PRUint32                mArrPos;  // current position within mArray
    nsCStringArray          mArray;   // array of names in (zip) directory

    PRPackedBool    mDirectory;
    PRPackedBool    mClosed;          // Whether the stream is closed

    nsresult ContinueInflate(char* aBuf, PRUint32 aCount, PRUint32* aBytesRead);
    nsresult ReadDirectory(char* aBuf, PRUint32 aCount, PRUint32* aBytesRead);
    PRUint32 CopyDataToBuffer(char* &aBuffer, PRUint32 &aCount);
};

#endif /* nsJARINPUTSTREAM_h__ */

