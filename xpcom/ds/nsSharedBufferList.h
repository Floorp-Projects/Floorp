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

#include "nsBufferHandle.h"
  // for |nsSharedBufferHandle|

#include "nscore.h"
  // for |PRUnichar|

  /**
   * This class forms the basis for several multi-fragment string classes, in
   * particular: |nsFragmentedString| (though not yet), and |nsSlidingString|/|nsSlidingSubstring|.
   *
   * This class is not templated.  It is provided only for |PRUnichar|-based strings.
   * If we turn out to have a need for multi-fragment ASCII strings, then perhaps we'll templatize
   * or else duplicate this class.
   */
class nsSharedBufferList
  {
    public:

      class Buffer
          : public nsXXXBufferHandle<PRUnichar>
        {
          public:
            Buffer( PRUnichar* aDataStart, PRUnichar* aDataEnd, PRUnichar* aStorageStart, PRUnichar* aStorageEnd ) : nsXXXBufferHandle<PRUnichar>(aDataStart, aDataEnd, aStorageStart, aStorageEnd) { }

            Buffer* mPrev;
            Buffer* mNext;
        };

      struct Position
        {
          Buffer*     mBuffer;
          PRUnichar*  mPosInBuffer;
        };



    public:
      nsSharedBufferList() : mFirstBuffer(0), mLastBuffer(0), mTotalDataLength(0) { }
      virtual ~nsSharedBufferList();

    private:
        // pass by value is not supported
      nsSharedBufferList( const nsSharedBufferList& );  // NOT TO BE IMPLEMENTED
      void operator=( const nsSharedBufferList& );      // NOT TO BE IMPLEMENTED

    public:
      void    LinkBuffer( Buffer*, Buffer*, Buffer* );
      Buffer* UnlinkBuffer( Buffer* );
      void    SplitBuffer( const Position& );

      static Buffer* NewBuffer( const PRUnichar*, PRUint32, PRUint32 = 0 );

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
