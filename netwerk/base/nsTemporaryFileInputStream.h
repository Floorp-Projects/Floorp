/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsTemporaryFileInputStream_h__
#define nsTemporaryFileInputStream_h__

#include "mozilla/Mutex.h"
#include "nsAutoPtr.h"
#include "nsIInputStream.h"
#include "nsISeekableStream.h"
#include "prio.h"

class nsTemporaryFileInputStream : public nsIInputStream
                                 , public nsISeekableStream
{
public:
  //used to release a PRFileDesc
  class FileDescOwner
  {
    friend class nsTemporaryFileInputStream;
  public:
    NS_INLINE_DECL_THREADSAFE_REFCOUNTING(FileDescOwner)
    explicit FileDescOwner(PRFileDesc* aFD)
      : mFD(aFD),
        mMutex("FileDescOwner::mMutex")
    {
      MOZ_ASSERT(aFD);
    }
  private:
    ~FileDescOwner()
    {
      PR_Close(mFD);
    }
  public:
    mozilla::Mutex& FileMutex() { return mMutex; }

  private:
    PRFileDesc* mFD;
    mozilla::Mutex mMutex;
  };

  nsTemporaryFileInputStream(FileDescOwner* aFileDescOwner, uint64_t aStartPos, uint64_t aEndPos);

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIINPUTSTREAM
  NS_DECL_NSISEEKABLESTREAM

private:
  virtual ~nsTemporaryFileInputStream() { }

  RefPtr<FileDescOwner> mFileDescOwner;
  uint64_t mStartPos;
  uint64_t mCurPos;
  uint64_t mEndPos;
  bool mClosed;
};

#endif // nsTemporaryFileInputStream_h__
