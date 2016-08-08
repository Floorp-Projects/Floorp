// /* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsFileStreams_h__
#define nsFileStreams_h__

#include "nsAutoPtr.h"
#include "nsIFileStreams.h"
#include "nsIFile.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsISafeOutputStream.h"
#include "nsISeekableStream.h"
#include "nsILineInputStream.h"
#include "nsCOMPtr.h"
#include "nsIIPCSerializableInputStream.h"
#include "nsReadLine.h"
#include <algorithm>


////////////////////////////////////////////////////////////////////////////////

class nsFileStreamBase : public nsISeekableStream,
                         public nsIFileMetadata
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSISEEKABLESTREAM
    NS_DECL_NSIFILEMETADATA

    nsFileStreamBase();

protected:
    virtual ~nsFileStreamBase();

    nsresult Close();
    nsresult Available(uint64_t* _retval);
    nsresult Read(char* aBuf, uint32_t aCount, uint32_t* _retval);
    nsresult ReadSegments(nsWriteSegmentFun aWriter, void* aClosure,
                          uint32_t aCount, uint32_t* _retval);
    nsresult IsNonBlocking(bool* _retval);
    nsresult Flush();
    nsresult Write(const char* aBuf, uint32_t aCount, uint32_t* _retval);
    nsresult WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                       uint32_t* _retval);
    nsresult WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                           uint32_t aCount, uint32_t* _retval);

    PRFileDesc* mFD;

    /**
     * Flags describing our behavior.  See the IDL file for possible values.
     */
    int32_t mBehaviorFlags;

    /**
     * Whether we have a pending open (see DEFER_OPEN in the IDL file).
     */
    bool mDeferredOpen;

    struct OpenParams {
        nsCOMPtr<nsIFile> localFile;
        int32_t ioFlags;
        int32_t perm;
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
    nsresult MaybeOpen(nsIFile* aFile, int32_t aIoFlags, int32_t aPerm,
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

class nsFileInputStream : public nsFileStreamBase,
                          public nsIFileInputStream,
                          public nsILineInputStream,
                          public nsIIPCSerializableInputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIFILEINPUTSTREAM
    NS_DECL_NSILINEINPUTSTREAM
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

    NS_IMETHOD Close() override;
    NS_IMETHOD Tell(int64_t *aResult) override;
    NS_IMETHOD Available(uint64_t* _retval) override;
    NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* _retval) override;
    NS_IMETHOD ReadSegments(nsWriteSegmentFun aWriter, void *aClosure,
                            uint32_t aCount, uint32_t* _retval) override
    {
        return nsFileStreamBase::ReadSegments(aWriter, aClosure, aCount,
                                              _retval);
    }
    NS_IMETHOD IsNonBlocking(bool* _retval) override
    {
        return nsFileStreamBase::IsNonBlocking(_retval);
    }

    // Overrided from nsFileStreamBase
    NS_IMETHOD Seek(int32_t aWhence, int64_t aOffset) override;

    nsFileInputStream()
      : mLineBuffer(nullptr), mIOFlags(0), mPerm(0), mCachedPosition(0)
    {}

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    virtual ~nsFileInputStream()
    {
        Close();
    }

    nsresult SeekInternal(int32_t aWhence, int64_t aOffset, bool aClearBuf=true);

    nsAutoPtr<nsLineBuffer<char> > mLineBuffer;

    /**
     * The file being opened.
     */
    nsCOMPtr<nsIFile> mFile;
    /**
     * The IO flags passed to Init() for the file open.
     */
    int32_t mIOFlags;
    /**
     * The permissions passed to Init() for the file open.
     */
    int32_t mPerm;

    /**
     * Cached position for Tell for automatically reopening streams.
     */
    int64_t mCachedPosition;

protected:
    /**
     * Internal, called to open a file.  Parameters are the same as their
     * Init() analogues.
     */
    nsresult Open(nsIFile* file, int32_t ioFlags, int32_t perm);
};

////////////////////////////////////////////////////////////////////////////////

class nsPartialFileInputStream : public nsFileInputStream,
                                 public nsIPartialFileInputStream
{
public:
    using nsFileInputStream::Init;
    using nsFileInputStream::Read;
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIPARTIALFILEINPUTSTREAM
    NS_DECL_NSIIPCSERIALIZABLEINPUTSTREAM

    nsPartialFileInputStream()
      : mStart(0), mLength(0), mPosition(0), mDeferredSeek(false)
    { }

    NS_IMETHOD Tell(int64_t *aResult) override;
    NS_IMETHOD Available(uint64_t *aResult) override;
    NS_IMETHOD Read(char* aBuf, uint32_t aCount, uint32_t* aResult) override;
    NS_IMETHOD Seek(int32_t aWhence, int64_t aOffset) override;

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    ~nsPartialFileInputStream()
    { }

    inline nsresult DoPendingSeek();

private:
    uint64_t TruncateSize(uint64_t aSize) {
          return std::min<uint64_t>(mLength - mPosition, aSize);
    }

    uint64_t mStart;
    uint64_t mLength;
    uint64_t mPosition;
    bool mDeferredSeek;
};

////////////////////////////////////////////////////////////////////////////////

class nsFileOutputStream : public nsFileStreamBase,
                           public nsIFileOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIFILEOUTPUTSTREAM
    NS_FORWARD_NSIOUTPUTSTREAM(nsFileStreamBase::)

    static nsresult
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

protected:
    virtual ~nsFileOutputStream()
    {
        Close();
    }
};

////////////////////////////////////////////////////////////////////////////////

/**
 * A safe file output stream that overwrites the destination file only
 * once writing is complete. This protects against incomplete writes
 * due to the process or the thread being interrupted or crashed.
 */
class nsAtomicFileOutputStream : public nsFileOutputStream,
                                 public nsISafeOutputStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSISAFEOUTPUTSTREAM

    nsAtomicFileOutputStream() :
        mTargetFileExists(true),
        mWriteResult(NS_OK) {}

    virtual nsresult DoOpen() override;

    NS_IMETHOD Close() override;
    NS_IMETHOD Write(const char *buf, uint32_t count, uint32_t *result) override;
    NS_IMETHOD Init(nsIFile* file, int32_t ioFlags, int32_t perm, int32_t behaviorFlags) override;

protected:
    virtual ~nsAtomicFileOutputStream()
    {
        Close();
    }

    nsCOMPtr<nsIFile>         mTargetFile;
    nsCOMPtr<nsIFile>         mTempFile;

    bool     mTargetFileExists;
    nsresult mWriteResult; // Internally set in Write()

};

////////////////////////////////////////////////////////////////////////////////

/**
 * A safe file output stream that overwrites the destination file only
 * once writing + flushing is complete. This protects against more
 * classes of software/hardware errors than nsAtomicFileOutputStream,
 * at the expense of being more costly to the disk, OS and battery.
 */
class nsSafeFileOutputStream : public nsAtomicFileOutputStream
{
public:

    NS_IMETHOD Finish() override;
};

////////////////////////////////////////////////////////////////////////////////

class nsFileStream : public nsFileStreamBase,
                     public nsIInputStream,
                     public nsIOutputStream,
                     public nsIFileStream
{
public:
    NS_DECL_ISUPPORTS_INHERITED
    NS_DECL_NSIFILESTREAM
    NS_FORWARD_NSIINPUTSTREAM(nsFileStreamBase::)

    // Can't use NS_FORWARD_NSIOUTPUTSTREAM due to overlapping methods
    // Close() and IsNonBlocking() 
    NS_IMETHOD Flush() override
    {
        return nsFileStreamBase::Flush();
    }
    NS_IMETHOD Write(const char* aBuf, uint32_t aCount, uint32_t* _retval) override
    {
        return nsFileStreamBase::Write(aBuf, aCount, _retval);
    }
    NS_IMETHOD WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                         uint32_t* _retval) override
    {
        return nsFileStreamBase::WriteFrom(aFromStream, aCount, _retval);
    }
    NS_IMETHOD WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                             uint32_t aCount, uint32_t* _retval) override
    {
        return nsFileStreamBase::WriteSegments(aReader, aClosure, aCount,
                                               _retval);
    }

protected:
    virtual ~nsFileStream()
    {
        Close();
    }
};

////////////////////////////////////////////////////////////////////////////////

#endif // nsFileStreams_h__
