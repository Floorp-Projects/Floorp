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
#include "nsIDiskCacheStreamInternal.h"
#include "zlib.h"
#include "mozilla/Mutex.h"

/******************************************************************************
* nsCacheEntryDescriptor
*******************************************************************************/
class nsCacheEntryDescriptor :
    public PRCList,
    public nsICacheEntryDescriptor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYDESCRIPTOR
    NS_DECL_NSICACHEENTRYINFO

    friend class nsAsyncDoomEvent;

    nsCacheEntryDescriptor(nsCacheEntry * entry, nsCacheAccessMode  mode);
    virtual ~nsCacheEntryDescriptor();
    
    /**
     * utility method to attempt changing data size of associated entry
     */
    nsresult  RequestDataSizeChange(int32_t deltaSize);
    
    /**
     * methods callbacks for nsCacheService
     */
    nsCacheEntry * CacheEntry(void)      { return mCacheEntry; }
    void           ClearCacheEntry(void)
    {
      bool asyncDoomPending;
      {
        mozilla::MutexAutoLock lock(mLock);
        asyncDoomPending = mAsyncDoomPending;
      }

      if (asyncDoomPending && mCacheEntry) {
        nsCacheService::gService->DoomEntry_Internal(mCacheEntry, true);
        mDoomedOnClose = true;
      }
      mCacheEntry = nullptr;
    }

    nsresult       CloseOutput(void)
    {
      nsresult rv = InternalCleanup(mOutput);
      mOutput = nullptr;
      return rv;
    }

private:
    nsresult       InternalCleanup(nsIOutputStream *stream)
    {
      if (stream) {
        nsCOMPtr<nsIDiskCacheStreamInternal> tmp (do_QueryInterface(stream));
        if (tmp)
          return tmp->CloseInternal();
        else
          return stream->Close();
      }
      return NS_OK;
    }


     /*************************************************************************
      * input stream wrapper class -
      *
      * The input stream wrapper references the descriptor, but the descriptor
      * doesn't need any references to the stream wrapper.
      *************************************************************************/
     class nsInputStreamWrapper : public nsIInputStream {
     private:
         nsCacheEntryDescriptor    * mDescriptor;
         nsCOMPtr<nsIInputStream>    mInput;
         uint32_t                    mStartOffset;
         bool                        mInitialized;
     public:
         NS_DECL_ISUPPORTS
         NS_DECL_NSIINPUTSTREAM

         nsInputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
         {
             NS_ADDREF(mDescriptor);
         }
         virtual ~nsInputStreamWrapper()
         {
             NS_RELEASE(mDescriptor);
         }

     private:
         nsresult LazyInit();
         nsresult EnsureInit() { return mInitialized ? NS_OK : LazyInit(); }
     };
     friend class nsInputStreamWrapper;


     class nsDecompressInputStreamWrapper : public nsInputStreamWrapper {
     private:
         unsigned char* mReadBuffer;
         uint32_t mReadBufferLen;
         z_stream mZstream;
         bool mStreamInitialized;
         bool mStreamEnded;
     public:
         NS_DECL_ISUPPORTS

         nsDecompressInputStreamWrapper(nsCacheEntryDescriptor * desc,
                                      uint32_t off)
          : nsInputStreamWrapper(desc, off)
          , mReadBuffer(0)
          , mReadBufferLen(0)
          , mStreamInitialized(false)
          , mStreamEnded(false)
         {
         }
         virtual ~nsDecompressInputStreamWrapper()
         {
             Close();
         }
         NS_IMETHOD Read(char* buf, uint32_t count, uint32_t * result);
         NS_IMETHOD Close();
     private:
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
     protected:
         nsCacheEntryDescriptor *    mDescriptor;
         nsCOMPtr<nsIOutputStream>   mOutput;
         uint32_t                    mStartOffset;
         bool                        mInitialized;
     public:
         NS_DECL_ISUPPORTS
         NS_DECL_NSIOUTPUTSTREAM

         nsOutputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
         {
             NS_ADDREF(mDescriptor); // owning ref
         }
         virtual ~nsOutputStreamWrapper()
         { 
             // XXX _HACK_ the storage stream needs this!
             Close();
             {
             nsCacheServiceAutoLock lock(LOCK_TELEM(NSOUTPUTSTREAMWRAPPER_CLOSE));
             mDescriptor->mOutput = nullptr;
             }
             NS_RELEASE(mDescriptor);
         }

     private:
         nsresult LazyInit();
         nsresult EnsureInit() { return mInitialized ? NS_OK : LazyInit(); }
         nsresult OnWrite(uint32_t count);
     };
     friend class nsOutputStreamWrapper;

     class nsCompressOutputStreamWrapper : public nsOutputStreamWrapper {
     private:
         unsigned char* mWriteBuffer;
         uint32_t mWriteBufferLen;
         z_stream mZstream;
         bool mStreamInitialized;
         uint32_t mUncompressedCount;
     public:
         NS_DECL_ISUPPORTS

         nsCompressOutputStreamWrapper(nsCacheEntryDescriptor * desc, 
                                       uint32_t off)
          : nsOutputStreamWrapper(desc, off)
          , mWriteBuffer(0)
          , mWriteBufferLen(0)
          , mStreamInitialized(false)
          , mUncompressedCount(0)
         {
         }
         virtual ~nsCompressOutputStreamWrapper()
         { 
             Close();
         }
         NS_IMETHOD Write(const char* buf, uint32_t count, uint32_t * result);
         NS_IMETHOD Close();
     private:
         nsresult InitZstream();
         nsresult WriteBuffer();
     };

 private:
     /**
      * nsCacheEntryDescriptor data members
      */
     nsCacheEntry          * mCacheEntry; // we are a child of the entry
     nsCacheAccessMode       mAccessGranted;
     nsIOutputStream       * mOutput;
     mozilla::Mutex          mLock;
     bool                    mAsyncDoomPending;
     bool                    mDoomedOnClose;
};


#endif // _nsCacheEntryDescriptor_h_
