/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla strings.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#ifndef nsSharedBufferList_h___
#define nsSharedBufferList_h___

#ifndef nsBufferHandle_h___
#include "nsBufferHandle.h"
  // for |nsSharedBufferHandle|
#endif

#ifndef nscore_h___
#include "nscore.h"
  // for |PRUnichar|
#endif

#ifndef nsAString_h___
#include "nsAString.h"
#endif

#ifndef nsDependentSubstring_h___
#include "nsDependentSubstring.h"
#endif

#ifndef nsBufferHandleUtils_h___
#include "nsBufferHandleUtils.h"
  // for |NS_AllocateContiguousHandleWithData|
#endif


  /**
   * This class forms the basis for several multi-fragment string classes, in
   * particular: |nsFragmentedString| (though not yet), and |nsSlidingString|/|nsSlidingSubstring|.
   *
   * This class is not templated.  It is provided only for |PRUnichar|-based strings.
   * If we turn out to have a need for multi-fragment ASCII strings, then perhaps we'll templatize
   * or else duplicate this class.
   */
class NS_COM nsSharedBufferList
  {
    public:

      class Buffer
          : public nsFlexBufferHandle<PRUnichar>
        {
          public:
            Buffer( PRUnichar* aDataStart, PRUnichar* aDataEnd, PRUnichar* aStorageStart, PRUnichar* aStorageEnd, PRBool aIsSingleAllocation=PR_FALSE )
                : nsFlexBufferHandle<PRUnichar>(aDataStart, aDataEnd, aStorageStart, aStorageEnd)
              {
                if ( aIsSingleAllocation )
                  this->mFlags |= this->kIsSingleAllocationWithBuffer;
              }

              /**
               * These buffers will be `owned' by the list, and only the
               *  the list itself will be allowed to delete member |Buffer|s,
               *  therefore, we cannot use the inherited |AcquireReference|
               *  and |ReleaseReference|, as they use the model that the
               *  buffer manages its own lifetime.
               */
            void
            AcquireNonOwningReference() const
              {
                Buffer* mutable_this = NS_CONST_CAST(Buffer*, this);
                mutable_this->set_refcount( get_refcount()+1 );
              }

            void
            ReleaseNonOwningReference() const
              {
                Buffer* mutable_this = NS_CONST_CAST(Buffer*, this);
                mutable_this->set_refcount( get_refcount()-1 );
              }

            Buffer* mPrev;
            Buffer* mNext;

          private:
              // pass-by-value is explicitly denied
            Buffer( const Buffer& );          // NOT TO BE IMPLEMENTED
            void operator=( const Buffer& );  // NOT TO BE IMPLEMENTED
        };

      struct Position
        {
          Buffer*     mBuffer;
          PRUnichar*  mPosInBuffer;

          Position() { }
          Position( Buffer* aBuffer, PRUnichar* aPosInBuffer ) : mBuffer(aBuffer), mPosInBuffer(aPosInBuffer) { }

          // Position( const Position& );             -- auto-generated copy-constructor OK
          // Position& operator=( const Position& );  -- auto-generated copy-assignment OK
          // ~Position();                             -- auto-generated destructor OK

            // iff |aIter| is a valid iterator into a |nsSharedBufferList|
          explicit
          Position( const nsAString::const_iterator& aIter )
              : mBuffer( NS_CONST_CAST(Buffer*, NS_REINTERPRET_CAST(const Buffer*, aIter.fragment().mFragmentIdentifier)) ),
                mPosInBuffer( NS_CONST_CAST(PRUnichar*, aIter.get()) )
            {
              // nothing else to do here
            }

            // iff |aIter| is a valid iterator into a |nsSharedBufferList|
          Position&
          operator=( const nsAString::const_iterator& aIter )
            {
              mBuffer       = NS_CONST_CAST(Buffer*, NS_REINTERPRET_CAST(const Buffer*, aIter.fragment().mFragmentIdentifier));
              mPosInBuffer  = NS_CONST_CAST(PRUnichar*, aIter.get());
              return *this;
            }

          void PointTo( Buffer* aBuffer, PRUnichar* aPosInBuffer )  { mBuffer=aBuffer; mPosInBuffer=aPosInBuffer; }
          void PointBefore( Buffer* aBuffer )                       { PointTo(aBuffer, aBuffer->DataStart()); }
          void PointAfter( Buffer* aBuffer )                        { PointTo(aBuffer, aBuffer->DataEnd()); }

          // Position( const Position& );             -- automatically generated copy-constructor is OK
          // Position& operator=( const Position& );  -- automatically generated copy-assignment operator is OK

            // don't want to provide this as |operator-|, since that might imply O(1)
          static ptrdiff_t Distance( const Position&, const Position& );
        };



    public:
      nsSharedBufferList( Buffer* aBuffer = 0 )
          : mFirstBuffer(aBuffer),
            mLastBuffer(aBuffer),
            mTotalDataLength(0)
        {
          if ( aBuffer )
            {
              aBuffer->mPrev = aBuffer->mNext = 0;
              mTotalDataLength = aBuffer->DataLength();
            }
        }

      virtual ~nsSharedBufferList();

    private:
        // pass-by-value is explicitly denied
      nsSharedBufferList( const nsSharedBufferList& );  // NOT TO BE IMPLEMENTED
      void operator=( const nsSharedBufferList& );      // NOT TO BE IMPLEMENTED

    public:
      void    LinkBuffer( Buffer*, Buffer*, Buffer* );
      Buffer* UnlinkBuffer( Buffer* );

      enum SplitDisposition     // when splitting a buffer in two...
        {
          kSplitCopyRightData,  // copy the data right of the split point to a new buffer
          kSplitCopyLeastData,  // copy the smaller amount of data to the new buffer
          kSplitCopyLeftData    // copy the data left of the split point to a new buffer
        };
      
      void    SplitBuffer( const Position&, SplitDisposition = kSplitCopyLeastData );

      static
      Buffer*
      NewSingleAllocationBuffer( const PRUnichar* aData, PRUint32 aDataLength, PRUint32 aAdditionalCapacity = 1 )
        {
          typedef Buffer* Buffer_ptr;
          return NS_AllocateContiguousHandleWithData(Buffer_ptr(0), Substring(aData, aData+aDataLength), aAdditionalCapacity);
        }

      static
      Buffer*
      NewSingleAllocationBuffer( const nsAString& aReadable, PRUint32 aAdditionalCapacity = 1 )
        {
          typedef Buffer* Buffer_ptr;
          return NS_AllocateContiguousHandleWithData(Buffer_ptr(0), aReadable, aAdditionalCapacity);
        }

      static
      Buffer*
      NewWrappingBuffer( PRUnichar* aDataStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd )
        {
          return new Buffer(aDataStart, aDataEnd, aDataStart, aStorageEnd);
        }

      void    DiscardSuffix( PRUint32 );
        // need other discards: prefix, and by iterator or pointer or something

            Buffer* GetFirstBuffer()        { return mFirstBuffer; }
      const Buffer* GetFirstBuffer() const  { return mFirstBuffer; }
      
            Buffer* GetLastBuffer()         { return mLastBuffer; }
      const Buffer* GetLastBuffer() const   { return mLastBuffer; }
      
      ptrdiff_t     GetDataLength() const   { return mTotalDataLength; }

    protected:
      void    DestroyBuffers();

    protected:
      Buffer*   mFirstBuffer;
      Buffer*   mLastBuffer;
      ptrdiff_t mTotalDataLength;
  };

#endif // !defined(nsSharedBufferList_h___)
