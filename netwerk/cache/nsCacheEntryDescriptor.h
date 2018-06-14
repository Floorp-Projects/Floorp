/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef _nsCacheEntryDescriptor_h_
#define _nsCacheEntryDescriptor_h_

#include "nsICacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsCacheService.h"
#include "zlib.h"
#include "mozilla/Mutex.h"

/******************************************************************************
* nsCacheEntryDescriptor
*******************************************************************************/
class nsCacheEntryDescriptor final :
    public PRCList,
    public nsICacheEntryDescriptor
{
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSICACHEENTRYDESCRIPTOR
    NS_DECL_NSICACHEENTRYINFO

    friend class nsAsyncDoomEvent;
    friend class nsCacheService;

    nsCacheEntryDescriptor(nsCacheEntry * entry, nsCacheAccessMode  mode);

    /**
     * utility method to attempt changing data size of associated entry
     */
    nsresult  RequestDataSizeChange(int32_t deltaSize);

    /**
     * methods callbacks for nsCacheService
     */
    nsCacheEntry * CacheEntry(void)      { return mCacheEntry; }
    bool           ClearCacheEntry(void)
    {
      NS_ASSERTION(mInputWrappers.IsEmpty(), "Bad state");
      NS_ASSERTION(!mOutputWrapper, "Bad state");

      bool doomEntry = false;
      bool asyncDoomPending;
      {
        mozilla::MutexAutoLock lock(mLock);
        asyncDoomPending = mAsyncDoomPending;
      }

      if (asyncDoomPending && mCacheEntry) {
        doomEntry = true;
        mDoomedOnClose = true;
      }
      mCacheEntry = nullptr;

      return doomEntry;
    }

private:
    virtual ~nsCacheEntryDescriptor();

     /*************************************************************************
      * input stream wrapper class -
      *
      * The input stream wrapper references the descriptor, but the descriptor
      * doesn't need any references to the stream wrapper.
      *************************************************************************/
     class nsInputStreamWrapper : public nsIInputStream {
         friend class nsCacheEntryDescriptor;

     private:
         nsCacheEntryDescriptor    * mDescriptor;
         nsCOMPtr<nsIInputStream>    mInput;
         uint32_t                    mStartOffset;
         bool                        mInitialized;
         mozilla::Mutex              mLock;
     public:
         NS_DECL_THREADSAFE_ISUPPORTS
         NS_DECL_NSIINPUTSTREAM

         nsInputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
             , mLock("nsInputStreamWrapper.mLock")
         {
             NS_ADDREF(mDescriptor);
         }

     private:
         virtual ~nsInputStreamWrapper()
         {
             NS_IF_RELEASE(mDescriptor);
         }

         nsresult LazyInit();
         nsresult EnsureInit();
         nsresult Read_Locked(char *buf, uint32_t count, uint32_t *countRead);
         nsresult Close_Locked();
         void CloseInternal();
     };


     class nsDecompressInputStreamWrapper : public nsInputStreamWrapper {
     private:
         unsigned char* mReadBuffer;
         uint32_t mReadBufferLen;
         z_stream mZstream;
         bool mStreamInitialized;
         bool mStreamEnded;
     public:
         NS_DECL_ISUPPORTS_INHERITED

         nsDecompressInputStreamWrapper(nsCacheEntryDescriptor * desc,
                                      uint32_t off)
          : nsInputStreamWrapper(desc, off)
          , mReadBuffer(nullptr)
          , mReadBufferLen(0)
          , mZstream{}
          , mStreamInitialized(false)
          , mStreamEnded(false)
         {
         }
         NS_IMETHOD Read(char* buf, uint32_t count, uint32_t * result) override;
         NS_IMETHOD Close() override;
     private:
         virtual ~nsDecompressInputStreamWrapper()
         {
             Close();
         }
         nsresult InitZstream();
         nsresult EndZstream();
     };


     /*************************************************************************
      * output stream wrapper class -
      *
      * The output stream wrapper references the descriptor, but the descriptor
      * doesn't need any references to the stream wrapper.
      *************************************************************************/
     class nsOutputStreamWrapper : public nsIOutputStream {
         friend class nsCacheEntryDescriptor;

     protected:
         nsCacheEntryDescriptor *    mDescriptor;
         nsCOMPtr<nsIOutputStream>   mOutput;
         uint32_t                    mStartOffset;
         bool                        mInitialized;
         mozilla::Mutex              mLock;
     public:
         NS_DECL_THREADSAFE_ISUPPORTS
         NS_DECL_NSIOUTPUTSTREAM

         nsOutputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
             , mLock("nsOutputStreamWrapper.mLock")
         {
             NS_ADDREF(mDescriptor); // owning ref
         }

     private:
         virtual ~nsOutputStreamWrapper()
         {
             Close();

             NS_ASSERTION(!mOutput, "Bad state");
             NS_ASSERTION(!mDescriptor, "Bad state");
         }

         nsresult LazyInit();
         nsresult EnsureInit();
         nsresult OnWrite(uint32_t count);
         nsresult Write_Locked(const char * buf,
                               uint32_t count,
                               uint32_t * result);
         nsresult Close_Locked();
         void CloseInternal();
     };


     class nsCompressOutputStreamWrapper : public nsOutputStreamWrapper {
     private:
         unsigned char* mWriteBuffer;
         uint32_t mWriteBufferLen;
         z_stream mZstream;
         bool mStreamInitialized;
         bool mStreamEnded;
         uint32_t mUncompressedCount;
     public:
         NS_DECL_ISUPPORTS_INHERITED

         nsCompressOutputStreamWrapper(nsCacheEntryDescriptor * desc,
                                       uint32_t off)
          : nsOutputStreamWrapper(desc, off)
          , mWriteBuffer(nullptr)
          , mWriteBufferLen(0)
          , mZstream{}
          , mStreamInitialized(false)
          , mStreamEnded(false)
          , mUncompressedCount(0)
         {
         }
         NS_IMETHOD Write(const char* buf, uint32_t count, uint32_t * result) override;
         NS_IMETHOD Close() override;
     private:
         virtual ~nsCompressOutputStreamWrapper()
         {
             Close();
         }
         nsresult InitZstream();
         nsresult WriteBuffer();
     };

 private:
     /**
      * nsCacheEntryDescriptor data members
      */
     nsCacheEntry          * mCacheEntry; // we are a child of the entry
     nsCacheAccessMode       mAccessGranted;
     nsTArray<nsInputStreamWrapper*> mInputWrappers;
     nsOutputStreamWrapper * mOutputWrapper;
     mozilla::Mutex          mLock;
     bool                    mAsyncDoomPending;
     bool                    mDoomedOnClose;
     bool                    mClosingDescriptor;
};


#endif // _nsCacheEntryDescriptor_h_
