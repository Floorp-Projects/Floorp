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

#ifndef nsSlidingString_h___
#define nsSlidingString_h___

#ifndef nsAString_h___
#include "nsAString.h"
#endif

#ifndef nsSharedBufferList_h___
#include "nsSharedBufferList.h"
#endif


  /**
   * Maintains the sequence from the prev-most referenced buffer to the last buffer.
   * As prev-most buffers become un-referenced, they are unlinked from the list
   * and destroyed.
   *
   * One or more |nsSlidingSubstring|s may reference into the list.  Each |nsSlidingSubstring|
   * holds a reference into the prev-most buffer intersecting the 
   * substring it describes.  The destructor of a |nsSlidingSubstring| releases this
   * reference, allowing the buffer list to destroy the contiguous prefix of
   * unreferenced buffers.
   *
   * A single instance of |nsSlidingString| may reference this list.
   * Through that interface, new data can be appended onto the next-most end
   * of the list.  |nsSlidingString| also the client to advance its starting point.
   * 
   */
class NS_COM nsSlidingSharedBufferList
    : public nsSharedBufferList
  {
    public:
      nsSlidingSharedBufferList( Buffer* aBuffer ) : nsSharedBufferList(aBuffer), mRefCount(0) { }

      void  AcquireReference()    { ++mRefCount; }
      void  ReleaseReference()    { if ( !--mRefCount ) delete this; }

      void  DiscardUnreferencedPrefix( Buffer* );

    private:
      PRUint32 mRefCount;
  };




class nsSlidingString;

  /**
   * a substring over a buffer list, this 
   */
class NS_COM nsSlidingSubstring
     : virtual public nsAPromiseString
  {
    friend class nsSlidingString;

    public:
      typedef nsSlidingSharedBufferList::Buffer   Buffer;
      typedef nsSlidingSharedBufferList::Position Position;

      nsSlidingSubstring()
          : mStart(0,0),
            mEnd(0,0),
            mBufferList(0),
            mLength(0)
        {
          // nothing else to do here
        }

      nsSlidingSubstring( const nsSlidingSubstring& );  // copy-constructor
      nsSlidingSubstring( const nsSlidingSubstring& aString, const nsAString::const_iterator& aStart, const nsAString::const_iterator& aEnd );
      nsSlidingSubstring( const nsSlidingString& );
      nsSlidingSubstring( const nsSlidingString& aString, const nsAString::const_iterator& aStart, const nsAString::const_iterator& aEnd );
      explicit nsSlidingSubstring( const nsAString& );
        // copy the supplied string into a new buffer ... there will be no modifying instance over this buffer list

      void Rebind( const nsSlidingSubstring& );
      void Rebind( const nsSlidingSubstring&, const nsAString::const_iterator&, const nsAString::const_iterator& );
      void Rebind( const nsSlidingString& );
      void Rebind( const nsSlidingString&, const nsAString::const_iterator&, const nsAString::const_iterator& );
      void Rebind( const nsAString& );

     ~nsSlidingSubstring();

      virtual PRUint32 Length() const { return mLength; }

    protected:
      nsSlidingSubstring( nsSlidingSharedBufferList* aBufferList );
      virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
      virtual       PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) { return 0; }

    private:
        // can't assign into me, I'm a read-only reference
      void operator=( const nsSlidingSubstring& );  // NOT TO BE IMPLEMENTED

      void
      init_range_from_buffer_list()
          // used only from constructors
        {
          mStart.PointBefore(mBufferList->GetFirstBuffer());
          mEnd.PointAfter(mBufferList->GetLastBuffer());
          mLength = PRUint32(Position::Distance(mStart, mEnd));
        }

      void
      acquire_ownership_of_buffer_list() const
          // used only from constructors and |Rebind|, requires |mStart| already be initialized
        {
          mBufferList->AcquireReference();
          mStart.mBuffer->AcquireNonOwningReference();
        }

      void
      release_ownership_of_buffer_list()
        {
          if ( mBufferList )
            {
              mStart.mBuffer->ReleaseNonOwningReference();
              mBufferList->DiscardUnreferencedPrefix(mStart.mBuffer);
              mBufferList->ReleaseReference();
            }
        }

    protected:
      Position                    mStart;
      Position                    mEnd;
      nsSlidingSharedBufferList*  mBufferList;
      PRUint32                    mLength;
  };




  /**
   * An |nsSlidingSharedBufferList| may be modified by zero or one instances of this class.
   *  
   */
class NS_COM nsSlidingString
    : virtual public nsAPromiseString,
      private nsSlidingSubstring
  {
    friend class nsSlidingSubstring;

    public:
      nsSlidingString( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd );
        // ...created by consuming ownership of a buffer ... |aStorageStart| must point to something
        //  that it will be OK for the slidking string to call |nsMemory::Free| on

      virtual PRUint32 Length() const { return mLength; }

        // you are giving ownership to the string, it takes and keeps your buffer, deleting it (with |nsMemory::Free|) when done
      void AppendBuffer( PRUnichar* aStorageStart, PRUnichar* aDataEnd, PRUnichar* aStorageEnd );

//    void Append( ... ); do you want some |Append|s that copy the supplied data?

      void DiscardPrefix( const nsAString::const_iterator& );
        // any other way you want to do this?

    protected:
      virtual const PRUnichar* GetReadableFragment( nsReadableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) const;
      virtual       PRUnichar* GetWritableFragment( nsWritableFragment<PRUnichar>&, nsFragmentRequest, PRUint32 ) { return 0; }

      void InsertReadable( const nsAString&, const nsAString::const_iterator& ); // ...to implement |nsScannerString::UngetReadable| 

    private:

      void
      acquire_ownership_of_buffer_list() const
          // used only from constructors and |Rebind|, requires |mStart| already be initialized
        {
          mBufferList->AcquireReference();
          mStart.mBuffer->AcquireNonOwningReference();
        }

      nsSlidingString( const nsSlidingString& );  // NOT TO BE IMPLEMENTED
      void operator=( const nsSlidingString& );   // NOT TO BE IMPLEMENTED
  };

#endif // !defined(nsSlidingString_h___)
