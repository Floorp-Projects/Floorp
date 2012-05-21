/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFileStreams_h__
#define nsFileStreams_h__

#include "nsAlgorithm.h"
#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsISeekableStream.h"
#include "nsILineInputStream.h"
#include "nsCOMPtr.h"
#include "prlog.h"
#include "prio.h"
#include "nsIIPCSerializable.h"

template<class CharType> class nsLineBuffer;

////////////////////////////////////////////////////////////////////////////////

class nsFileStream : public nsISeekableStream
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM

    nsFileStream();
    virtual ~nsFileStream();

    nsresult Close();

protected:
    PRFileDesc* mFD;

    /**
     * Flags describing our behavior.  See the IDL file for possible values.
     */
    PRInt32 mBehaviorFlags;

    /**
     * Whether we have a pending open (see DEFER_OPEN in the IDL file).
     */
    bool mDeferredOpen;

    struct OpenParams {
        nsCOMPtr<nsILocalFile> localFile;
        PRInt32 ioFlags;
        PRInt32 perm;
    };

    /**
     * Data we need to do an open.
     */
    OpenParams mOpenParams;

    /**
     * Prepares the data we need to open the file, and either does the open now
     * by calling DoOpen(), or leaves it to be opened later by a call to
     * DoPendingOpen().
     */
    nsresult MaybeOpen(nsILocalFile* aFile, PRInt32 aIoFlags, PRInt32 aPerm,
                       bool aDeferred);

    /**
     * Cleans up data prepared in MaybeOpen.
     */
    void CleanUpOpen();

    /**
     * Open the file. This is called either from MaybeOpen (during Init)
     * or from DoPendingOpen (if DEFER_OPEN is used when initializing this
     * stream). The default behavior of DoOpen is to open the file and save the
     * file descriptor.
     */
    virtual nsresult DoOpen();

    /**
     * If there is a pending open, do it now. It's important for this to be
     * inline since we do it in almost every stream API call.
     */
    inline nsresult DoPendingOpen();
};

////////////////////////////////////////////////////////////////////////////////

class nsFileInputStream : public nsFileStream,
                          public nsIFileInputStream,
                          public nsILineInputStream,
                          public nsIIPCSerializable
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIINPUTSTREAM
    NS_DECL_NSIFILEINPUTSTREAM
    NS_DECL_NSILINEINPUTSTREAM
    NS_DECL_NSIIPCSERIALIZABLE
    
    // Overrided from nsFileStream
    NS_IMETHOD Seek(PRInt32 aWhence, PRInt64 aOffset);

    nsFileInputStream() : nsFileStream() 
    {
        mLineBuffer = nsnull;
    }
    virtual ~nsFileInputStream() 
    {
        Close();
    }

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    nsLineBuffer<char> *mLineBuffer;

    /**
     * The file being opened.
     */
    nsCOMPtr<nsIFile> mFile;
    /**
     * The IO flags passed to Init() for the file open.
     */
    PRInt32 mIOFlags;
    /**
     * The permissions passed to Init() for the file open.
     */
    PRInt32 mPerm;

protected:
    /**
     * Internal, called to open a file.  Parameters are the same as their
     * Init() analogues.
     */
    nsresult Open(nsIFile* file, PRInt32 ioFlags, PRInt32 perm);
    /**
     * Reopen the file (for OPEN_ON_READ only!)
     */
    nsresult Reopen() { return Open(mFile, mIOFlags, mPerm); }
};

////////////////////////////////////////////////////////////////////////////////

class nsPartialFileInputStream : public nsFileInputStream,
                                 public nsIPartialFileInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPARTIALFILEINPUTSTREAM

    NS_IMETHOD Tell(PRInt64 *aResult);
    NS_IMETHOD Available(PRUint32 *aResult);
    NS_IMETHOD Read(char* aBuf, PRUint32 aCount, PRUint32* aResult);
    NS_IMETHOD Seek(PRInt32 aWhence, PRInt64 aOffset);

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

private:
    PRUint32 TruncateSize(PRUint32 aSize) {
          return (PRUint32)NS_MIN<PRUint64>(mLength - mPosition, aSize);
    }

    PRUint64 mStart;
    PRUint64 mLength;
    PRUint64 mPosition;
};

////////////////////////////////////////////////////////////////////////////////

class nsFileOutputStream : public nsFileStream,
                           public nsIFileOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIOUTPUTSTREAM
    NS_DECL_NSIFILEOUTPUTSTREAM

    nsFileOutputStream() : nsFileStream() {}
    virtual ~nsFileOutputStream() { nsFileOutputStream::Close(); }
    
    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);
};

////////////////////////////////////////////////////////////////////////////////

class nsSafeFileOutputStream : public nsFileOutputStream,
                               public nsISafeOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISAFEOUTPUTSTREAM

    nsSafeFileOutputStream() :
        mTargetFileExists(true),
        mWriteResult(NS_OK) {}

    virtual ~nsSafeFileOutputStream() { nsSafeFileOutputStream::Close(); }

    virtual nsresult DoOpen();

    NS_IMETHODIMP Close();
    NS_IMETHODIMP Write(const char *buf, PRUint32 count, PRUint32 *result);
    NS_IMETHODIMP Init(nsIFile* file, PRInt32 ioFlags, PRInt32 perm, PRInt32 behaviorFlags);

protected:
    nsCOMPtr<nsIFile>         mTargetFile;
    nsCOMPtr<nsIFile>         mTempFile;

    bool     mTargetFileExists;
    nsresult mWriteResult; // Internally set in Write()
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsFileStreams_h__
