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

#ifndef nsBufferHandle_h___
#define nsBufferHandle_h___

  /**
   *
   */
template <class CharT>
class nsBufferHandle
  {
    public:

      ptrdiff_t
      DataLength() const
        {
          return mDataEnd - mDataStart;
        }

      const CharT*
      DataStart() const
        {
          return mDataStart;
        }

      const CharT*
      DataEnd() const
        {
          return mDataEnd;
        }

    protected:
      const CharT*  mDataStart;
      const CharT*  mDataEnd;
  };



  /**
   *
   */
template <class CharT>
class nsSharedBufferHandle
    : public nsStringFragmentHandle<CharT>
  {
    protected:
      enum
        {
          kIsShared                       = 1<<31,
          kIsSingleAllocationWithBuffer   = 1<<30,
          kIsStorageDefinedSeparately     = 1<<29,

          kFlagsMask                      = kIsShared | kIsSingleAllocationWithBuffer | kIsStorageDefinedSeparately,
          kRefCountMask                   = ~kFlagsMask
        };

    public:
      ~nsSharedBufferHandle();

      void
      AcquireReference() const
        {
          set_refcount( get_refcount()+1 );
        }

      void
      ReleaseReference() const
        {
          if ( !set_refcount( get_refcount()-1 ) )
            delete this;
        }

      PRBool
      IsReferenced() const
        {
          return get_refcount() != 0;
        }

    protected:
      PRUint32  mFlags;

      PRUint32
      get_refcount() const
        {
          return mFlags & kRefCountMask;
        }

      PRUint32
      set_refcount( PRUint32 aNewRefCount )
        {
          NS_ASSERTION(aNewRefCount <= kRefCountMask, "aNewRefCount <= kRefCountMask");

          mFlags = (mFlags & kFlagsMask) | aNewRefCount;
          return aNewRefCount;
        }
  };


  // need a name for this
template <class CharT>
class nsXXXBufferHandle
    : public nsSharedBufferHandle<CharT>
  {
    public:
      nsXXXBufferHandle() { mFlags |= kIsStorageDefinedSeparately; }

    protected:
      const CharT*  mStorageStart;
      const CharT*  mStorageEnd;
  };

template <class CharT>
nsSharedBufferHandle<CharT>::~nsSharedBufferHandle()
    // really don't want this to be |inline|
  {
    NS_ASSERTION(!IsReferenced(), "!IsReferenced()");

    if ( !(mFlags & kIsSingleAllocationWithBuffer) )
      {
        CharT* string_storage = mDataStart;
        if ( mFlags & kIsStorageDefinedSeparately )
          string_storage = NS_STATIC_CAST(nsXXXBufferHandle*, this)->mStorageStart;
        nsMemory::Free(string_storage);
      }
  }


#endif // !defined(nsBufferHandle_h___)
