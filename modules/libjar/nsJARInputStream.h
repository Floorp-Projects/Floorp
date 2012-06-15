/* nsJARInputStream.h
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsJARINPUTSTREAM_h__
#define nsJARINPUTSTREAM_h__

#include "nsIInputStream.h"
#include "nsJAR.h"
#include "nsTArray.h"
#include "mozilla/Attributes.h"

/*-------------------------------------------------------------------------
 * Class nsJARInputStream declaration. This class defines the type of the
 * object returned by calls to nsJAR::GetInputStream(filename) for the
 * purpose of reading a file item out of a JAR file. 
 *------------------------------------------------------------------------*/
class nsJARInputStream MOZ_FINAL : public nsIInputStream
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

