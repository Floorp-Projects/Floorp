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
#include "nsTArray.h"

/*-------------------------------------------------------------------------
 * Class nsJARInputStream declaration. This class defines the type of the
 * object returned by calls to nsJAR::GetInputStream(filename) for the
 * purpose of reading a file item out of a JAR file. 
 *------------------------------------------------------------------------*/
class nsJARInputStream : public nsIInputStream
{
  public:
    nsJARInputStream() : 
        mOutSize(0), mInCrc(0), mOutCrc(0), mCurPos(0),
        mMode(MODE_NOTINITED)
    { 
      memset(&mZs, 0, sizeof(z_stream));
    }

    ~nsJARInputStream() { Close(); }

    NS_DECL_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
   
    // takes ownership of |fd|, even on failure
    nsresult InitFile(nsJAR *aJar, nsZipItem *item);

    nsresult InitDirectory(nsJAR *aJar,
                           const nsACString& aJarDirSpec,
                           const char* aDir);
  
  private:
    nsRefPtr<nsZipHandle>  mFd;         // handle for reading
    PRUint32               mOutSize;    // inflated size 
    PRUint32               mInCrc;      // CRC as provided by the zipentry
    PRUint32               mOutCrc;     // CRC as calculated by me
    z_stream               mZs;         // zip data structure

    /* For directory reading */
    nsRefPtr<nsJAR>        mJar;        // string reference to zipreader
    PRUint32               mNameLen;    // length of dirname
    nsCString              mBuffer;     // storage for generated text of stream
    PRUint32               mCurPos;     // Current position in buffer
    PRUint32               mArrPos;     // current position within mArray
    nsTArray<nsCString>    mArray;      // array of names in (zip) directory

	typedef enum {
        MODE_NOTINITED,
        MODE_CLOSED,
        MODE_DIRECTORY,
        MODE_INFLATE,
        MODE_COPY
    } JISMode;

    JISMode                mMode;		// Modus of the stream

    nsresult ContinueInflate(char* aBuf, PRUint32 aCount, PRUint32* aBytesRead);
    nsresult ReadDirectory(char* aBuf, PRUint32 aCount, PRUint32* aBytesRead);
    PRUint32 CopyDataToBuffer(char* &aBuffer, PRUint32 &aCount);
};

#endif /* nsJARINPUTSTREAM_h__ */

