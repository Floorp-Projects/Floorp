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

/* nsBufferHandle.h --- the collection of classes that describe the atomic hunks of strings */

#ifndef nsBufferHandle_h___
#define nsBufferHandle_h___

#ifndef nsStringDefines_h___
#include "nsStringDefines.h"
#endif

#include "prtypes.h"
  // for |PRBool|

#include "nsDebug.h"
  // for |NS_ASSERTION|

#include "nscore.h"
  // for |PRUnichar|, |NS_REINTERPRET_CAST|

#ifdef _MSC_VER
  // VC++ erroneously warns about incompatible linkage in these templates
  //  It's a lie, and it's bothersome.  This |#pragma| quiets the warning.
#pragma warning( disable: 4251 )
#endif

  /*
   * The classes in this file are collectively called `buffer handles'.
   * All buffer handles begin with a pointer-tuple that delimits the
   * useful content of a hunk of string.  A buffer handle that points to
   * a sharable hunk of string data additionally has a field which
   * multiplexes some flags and a reference count.
   *
   *
   *  ns[Const]BufferHandle       nsSharedBufferHandle
   *  +-----+-----+-----+-----+   +-----+-----+-----+-----+
   *  | mDataStart            |   | mDataStart            |
   *  +-----+-----+-----+-----+   +-----+-----+-----+-----+
   *  | mDataEnd              |   | mDataEnd              |
   *  +-----+-----+-----+-----+   +-----+-----+-----+-----+
   *                              | mFlags                |
   *                              +-----+-----+-----+-----+
   *                              | mStorageLength        |
   *                              +-----+-----+-----+-----+
   *                              . mAllocator            .
   *                              .........................
   *
   * Given only a |ns[Const]BufferHandle|, there is no legal way to tell
   * if it is sharable.  In all cases, the data might immediately follow
   * the handle in the same allocated block.  From the |mFlags| field,
   * you can tell exactly what configuration of a handle you actually
   * have.
   *
   * An |nsSharedBufferHandle| has the limitation that its |mDataStart|
   * must also be the beginning of the allocated storage.  However,
   * allowing otherwise would introduce significant additional
   * complexity for a feature that would better be handled by allowing
   * an owninng substring string class that owned a reference to the
   * buffer handle.
   */


  /**
   *
   * @status FROZEN
   */
template <class CharT>
class nsBufferHandle
  {
    public:
      typedef PRUint32                          size_type;

      nsBufferHandle() { }
      nsBufferHandle( CharT* aDataStart, CharT* aDataEnd ) : mDataStart(aDataStart), mDataEnd(aDataEnd) { }

      void          DataStart( CharT* aNewDataStart )       { mDataStart = aNewDataStart; }
      CharT*        DataStart()                             { return mDataStart; }
      const CharT*  DataStart() const                       { return mDataStart; }

      void          DataEnd( CharT* aNewDataEnd )           { mDataEnd = aNewDataEnd; }
      CharT*        DataEnd()                               { return mDataEnd; }
      const CharT*  DataEnd() const                         { return mDataEnd; }

      size_type     DataLength() const                      { return mDataEnd - mDataStart; }

    protected:
      CharT*  mDataStart;
      CharT*  mDataEnd;
  };

  /**
   *
   * @status FROZEN
   */
template <class CharT>
class nsConstBufferHandle
  {
    public:
      typedef PRUint32                          size_type;

      nsConstBufferHandle() { }
      nsConstBufferHandle( const CharT* aDataStart, const CharT* aDataEnd ) : mDataStart(aDataStart), mDataEnd(aDataEnd) { }

      void          DataStart( const CharT* aNewDataStart ) { mDataStart = aNewDataStart; }
      const CharT*  DataStart() const                       { return mDataStart; }

      void          DataEnd( const CharT* aNewDataEnd )     { mDataEnd = aNewDataEnd; }
      const CharT*  DataEnd() const                         { return mDataEnd; }

      size_type     DataLength() const                      { return mDataEnd - mDataStart; }

    protected:
      const CharT*  mDataStart;
      const CharT*  mDataEnd;
  };


  /**
   * string allocator stuff needs to move to its own file
   * also see http://bugzilla.mozilla.org/show_bug.cgi?id=70087
   */

template <class CharT>
class nsStringAllocator
  {
    public:
      // more later
      virtual void Deallocate( CharT* ) const = 0;
  };

  /**
   * the following two routines must be provided by the client embedding strings
   */
NS_COM nsStringAllocator<char>&      StringAllocator_char();
NS_COM nsStringAllocator<PRUnichar>& StringAllocator_wchar_t();


  /**
   * this traits class lets templated clients pick the appropriate non-template global allocator
   */
template <class T>
struct nsStringAllocatorTraits
  {
  };

NS_SPECIALIZE_TEMPLATE
struct nsStringAllocatorTraits<char>
  {
    static nsStringAllocator<char>&      global_string_allocator() { return StringAllocator_char(); }
  };

NS_SPECIALIZE_TEMPLATE
struct nsStringAllocatorTraits<PRUnichar>
  {
    static nsStringAllocator<PRUnichar>& global_string_allocator() { return StringAllocator_wchar_t(); }
  };

// end of string allocator stuff that needs to move

#ifdef DEBUG

// A small utility class for verifying that all reference counting of
// shared buffer handles (which are not threadsafe) occurs on a single
// thread.
class NS_COM nsSingleThreadVerifier
  {
    public:
      nsSingleThreadVerifier();
      void verifyThread() const;
    protected:
      void* mThread;
  };

#endif

template <class CharT>
class nsSharedBufferHandle
    : public nsBufferHandle<CharT>
  {
    public:
      typedef PRUint32                          size_type;

      enum
        {
          kIsImmutable                    = 0x01000000, // if this is set, the buffer cannot be modified even if its refcount is 1
          kIsSingleAllocationWithBuffer   = 0x02000000, // handle and buffer are one piece, no separate deallocation is possible for the buffer
          kIsUserAllocator                = 0x04000000, // can't |delete|, call a hook instead

            // the following flags are opaque to the string library itself
          kIsNULL                         = 0x80000000, // the most common request of external clients is a scheme by which they can express `NULL'-ness
          kImplementationFlagsMask        = 0xF0000000, // 4 bits for use by site-implementations, e.g., for `NULL'-ness

          kFlagsMask                      = 0xFF000000,
          kRefCountMask                   = 0x00FFFFFF
        };

    public:
      nsSharedBufferHandle( CharT* aDataStart, CharT* aDataEnd, size_type aStorageLength, PRBool isSingleAllocation )
          : nsBufferHandle<CharT>(aDataStart, aDataEnd),
            mFlags(0),
            mStorageLength(aStorageLength)
        {
          if ( isSingleAllocation )
            mFlags |= kIsSingleAllocationWithBuffer;
        }

      ~nsSharedBufferHandle();

      void
      AcquireReference() const
        {
          nsSharedBufferHandle<CharT>* mutable_this = NS_CONST_CAST(nsSharedBufferHandle<CharT>*, this);
          mutable_this->set_refcount( get_refcount()+1 );
        }

      void ReleaseReference() const;

      PRBool
      IsReferenced() const
        {
          return get_refcount() != 0;
        }

      PRBool
      IsMutable() const
        {
          // (get_refcount() == 1) && !(GetImplementationFlags() & kIsImmutable)
          return (mFlags & (kRefCountMask | kIsImmutable) == 1);
        }

      void StorageLength( size_type aNewStorageLength )
        {
          mStorageLength = aNewStorageLength;
        }

      size_type
      StorageLength() const
        {
          return mStorageLength;
        }

      PRUint32
      GetImplementationFlags() const
        {
          return mFlags & kImplementationFlagsMask;
        }

      void
      SetImplementationFlags( PRUint32 aNewFlags )
        {
          mFlags = (mFlags & ~kImplementationFlagsMask) | (aNewFlags & kImplementationFlagsMask);
        }

    protected:
      PRUint32  mFlags;
      size_type mStorageLength;
#ifdef DEBUG
      nsSingleThreadVerifier mSingleThreadVerifier;
#endif

      PRUint32
      get_refcount() const
        {
#ifdef DEBUG
          mSingleThreadVerifier.verifyThread();
#endif
          return mFlags & kRefCountMask;
        }

      PRUint32
      set_refcount( PRUint32 aNewRefCount )
        {
#ifdef DEBUG
          mSingleThreadVerifier.verifyThread();
#endif

          NS_ASSERTION(aNewRefCount <= kRefCountMask, "aNewRefCount <= kRefCountMask");

          mFlags = (mFlags & kFlagsMask) | aNewRefCount;
          return aNewRefCount;
        }

      nsStringAllocator<CharT>& get_allocator() const;
  };


template <class CharT>
class nsSharedBufferHandleWithAllocator
    : public nsSharedBufferHandle<CharT>
  {
    public:
      // why is this needed again?
      typedef PRUint32                          size_type;

      nsSharedBufferHandleWithAllocator( CharT* aDataStart, CharT* aDataEnd, size_type aStorageLength, nsStringAllocator<CharT>& aAllocator )
          : nsSharedBufferHandle<CharT>(aDataStart, aDataEnd,
                                        aStorageLength, PR_FALSE),
            mAllocator(aAllocator)
        {
          this->mFlags |= this->kIsUserAllocator;
        }

      nsStringAllocator<CharT>& get_allocator() const { return mAllocator; }

    protected:
      nsStringAllocator<CharT>& mAllocator;
  };


// Derive from this class to implement a |Destroy| method.
template <class CharT>
class nsSharedBufferHandleWithDestroy
    : public nsSharedBufferHandle<CharT>
  {
    public:
      // why is this needed again?
      typedef PRUint32                          size_type;

      nsSharedBufferHandleWithDestroy( CharT* aDataStart, CharT* aDataEnd, size_type aStorageLength)
          : nsSharedBufferHandle<CharT>(aDataStart, aDataEnd,
                                        aStorageLength, PR_FALSE)
        {
          this->mFlags |=
              this->kIsUserAllocator | this->kIsSingleAllocationWithBuffer;
        }

      virtual void Destroy() = 0;

      ~nsSharedBufferHandleWithDestroy() { }


  };

template <class CharT>
class nsNonDestructingSharedBufferHandle
    : public nsSharedBufferHandleWithDestroy<CharT>
  {
    public:
      // why is this needed again?
      typedef PRUint32                          size_type;

      nsNonDestructingSharedBufferHandle( CharT* aDataStart, CharT* aDataEnd, size_type aStorageLength)
          : nsSharedBufferHandleWithDestroy<CharT>(aDataStart, aDataEnd,
                                                   aStorageLength)
        {
        }

      virtual void Destroy()
        {
          // Oops, threads raced to set the refcount.  Set the refcount
          // back to 1.
          this->set_refcount(1);
        }

  };


template <class CharT>
nsStringAllocator<CharT>&
nsSharedBufferHandle<CharT>::get_allocator() const
    // really don't want this to be |inline|
  {
    if ( mFlags & kIsUserAllocator )
      {
        return NS_REINTERPRET_CAST(const nsSharedBufferHandleWithAllocator<CharT>*, this)->get_allocator();
      }

    return nsStringAllocatorTraits<CharT>::global_string_allocator();
  }


template <class CharT>
nsSharedBufferHandle<CharT>::~nsSharedBufferHandle()
    // really don't want this to be |inline|
  {
    NS_ASSERTION(!IsReferenced(), "!IsReferenced()");

    if ( !(mFlags & kIsSingleAllocationWithBuffer) )
      {
        CharT* string_storage = this->mDataStart;
        get_allocator().Deallocate(string_storage);
      }
  }

template <class CharT>
void
nsSharedBufferHandle<CharT>::ReleaseReference() const
  {
    nsSharedBufferHandle<CharT>* mutable_this = NS_CONST_CAST(nsSharedBufferHandle<CharT>*, this);
    if ( !mutable_this->set_refcount( get_refcount()-1 ) )
      {
        if ( ~mFlags & (kIsUserAllocator|kIsSingleAllocationWithBuffer) )
          delete mutable_this;
        else
          // If |kIsUserAllocator| and |kIsSingleAllocationWithBuffer|,
          // the handle is an |nsSharedBufferHandleWithDestroy<CharT>|,
          // so call its |Destroy| method.
          NS_STATIC_CAST(nsSharedBufferHandleWithDestroy<CharT>*,
                         mutable_this)->Destroy();
      }
  }


#endif // !defined(nsBufferHandle_h___)
