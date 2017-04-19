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

#ifdef MOZ_JAR_BROTLI
struct BrotliStateStruct;
#endif

/*-------------------------------------------------------------------------
 * Class nsJARInputStream declaration. This class defines the type of the
 * object returned by calls to nsJAR::GetInputStream(filename) for the
 * purpose of reading a file item out of a JAR file. 
 *------------------------------------------------------------------------*/
class nsJARInputStream final : public nsIInputStream
{
  public:
    nsJARInputStream()
    : mOutSize(0)
    , mInCrc(0)
    , mOutCrc(0)
#ifdef MOZ_JAR_BROTLI
    , mBrotliState(nullptr)
#endif
    , mNameLen(0)
    , mCurPos(0)
    , mArrPos(0)
    , mMode(MODE_NOTINITED)
    { 
      memset(&mZs, 0, sizeof(z_stream));
    }

    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIINPUTSTREAM
   
    // takes ownership of |fd|, even on failure
    nsresult InitFile(nsJAR *aJar, nsZipItem *item);

    nsresult InitDirectory(nsJAR *aJar,
                           const nsACString& aJarDirSpec,
                           const char* aDir);
  
  private:
    ~nsJARInputStream() { Close(); }

    RefPtr<nsZipHandle>  mFd;         // handle for reading
    uint32_t               mOutSize;    // inflated size 
    uint32_t               mInCrc;      // CRC as provided by the zipentry
    uint32_t               mOutCrc;     // CRC as calculated by me
    z_stream               mZs;         // zip data structure
#ifdef MOZ_JAR_BROTLI
    BrotliStateStruct*     mBrotliState; // Brotli decoder state
#endif

    /* For directory reading */
    RefPtr<nsJAR>          mJar;        // string reference to zipreader
    uint32_t               mNameLen;    // length of dirname
    nsCString              mBuffer;     // storage for generated text of stream
    uint32_t               mCurPos;     // Current position in buffer
    uint32_t               mArrPos;     // current position within mArray
    nsTArray<nsCString>    mArray;      // array of names in (zip) directory

	typedef enum {
        MODE_NOTINITED,
        MODE_CLOSED,
        MODE_DIRECTORY,
        MODE_INFLATE,
#ifdef MOZ_JAR_BROTLI
        MODE_BROTLI,
#endif
        MODE_COPY
    } JISMode;

    JISMode                mMode;		// Modus of the stream

    nsresult ContinueInflate(char* aBuf, uint32_t aCount, uint32_t* aBytesRead);
    nsresult ReadDirectory(char* aBuf, uint32_t aCount, uint32_t* aBytesRead);
    uint32_t CopyDataToBuffer(char* &aBuffer, uint32_t &aCount);
};

#endif /* nsJARINPUTSTREAM_h__ */

