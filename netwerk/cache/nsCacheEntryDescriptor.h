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
#include "nsVoidArray.h"

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
    friend class nsCacheService;

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
    bool           ClearCacheEntry(void)
    {
      NS_ASSERTION(mInputWrappers.Count() == 0, "Bad state");
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
         NS_DECL_ISUPPORTS
         NS_DECL_NSIINPUTSTREAM

         nsInputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
             , mLock("nsInputStreamWrapper.mLock")
         {
             NS_ADDREF(mDescriptor);
         }
         virtual ~nsInputStreamWrapper()
         {
             nsCOMPtr<nsCacheEntryDescriptor> desc;
             {
                 nsCacheServiceAutoLock lock(LOCK_TELEM(
                                             NSINPUTSTREAMWRAPPER_DESTRUCTOR));
                 desc.swap(mDescriptor);
                 if (desc) {
                     NS_ASSERTION(desc->mInputWrappers.IndexOf(this) != -1,
                                  "Wrapper not found in array!");
                     desc->mInputWrappers.RemoveElement(this);
                 }
             }

         }

     private:
         nsresult LazyInit();
         nsresult EnsureInit() { return mInitialized ? NS_OK : LazyInit(); }
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
         friend class nsCacheEntryDescriptor;

     protected:
         nsCacheEntryDescriptor *    mDescriptor;
         nsCOMPtr<nsIOutputStream>   mOutput;
         uint32_t                    mStartOffset;
         bool                        mInitialized;
         mozilla::Mutex              mLock;
     public:
         NS_DECL_ISUPPORTS
         NS_DECL_NSIOUTPUTSTREAM

         nsOutputStreamWrapper(nsCacheEntryDescriptor * desc, uint32_t off)
             : mDescriptor(desc)
             , mStartOffset(off)
             , mInitialized(false)
             , mLock("nsOutputStreamWrapper.mLock")
         {
             NS_ADDREF(mDescriptor); // owning ref
         }
         virtual ~nsOutputStreamWrapper()
         { 
             // XXX _HACK_ the storage stream needs this!
             Close();
             nsCOMPtr<nsCacheEntryDescriptor> desc;
             {
                 nsCacheServiceAutoLock lock(LOCK_TELEM(
                                             NSOUTPUTSTREAMWRAPPER_DESTRUCTOR));
                 desc.swap(mDescriptor);
                 if (desc) {
                     desc->mOutputWrapper = nullptr;
                 }
                 mOutput = nullptr;
             }
         }

     private:
         nsresult LazyInit();
         nsresult EnsureInit() { return mInitialized ? NS_OK : LazyInit(); }
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
         NS_DECL_ISUPPORTS

         nsCompressOutputStreamWrapper(nsCacheEntryDescriptor * desc, 
                                       uint32_t off)
          : nsOutputStreamWrapper(desc, off)
          , mWriteBuffer(0)
          , mWriteBufferLen(0)
          , mStreamInitialized(false)
          , mStreamEnded(false)
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
     nsVoidArray             mInputWrappers;
     nsOutputStreamWrapper * mOutputWrapper;
     mozilla::Mutex          mLock;
     bool                    mAsyncDoomPending;
     bool                    mDoomedOnClose;
     bool                    mClosingDescriptor;
};


#endif // _nsCacheEntryDescriptor_h_
