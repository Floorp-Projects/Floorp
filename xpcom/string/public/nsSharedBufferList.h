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

  /**
   *
   */
class nsSharedBufferList
  {
    public:

      class Buffer
          : public nsSharedBufferHandle<PRUnichar>
        {
          friend class nsSharedBufferList;

          public:

          private:
            Buffer* mPrev;
            Buffer* mNext;
        };

      struct Position
        {
          nsSharedBufferList::Buffer*     mBuffer;
          nsSharedBufferList::PRUnichar*  mPosInBuffer;
        };



    public:
      nsSharedBufferList() : mFirstBuffer(0), mLastBuffer(0), mTotalLength(0) { }
      virtual ~nsSharedBufferList();

    private:
        // pass by value is not supported
      nsSharedBufferList( const nsSharedBufferList& );  // NOT TO BE IMPLEMENTED
      void operator=( const nsSharedBufferList& );      // NOT TO BE IMPLEMENTED

    public:
      void    LinkBuffer( Buffer*, Buffer*, Buffer* );
      Buffer* UnlinkBuffer( Buffer* );
      void    SplitBuffer( const Position& );

      Buffer* First() { return mFirstBuffer; }
      Buffer* Last()  { return mLastBuffer; }

    protected:
      void    DestroyBuffers();

    protected:
      Buffer*   mFirstBuffer;
      Buffer*   mLastBuffer;

      PRUint32  mTotalLength;
  };

#endif // !defined(nsSharedBufferList_h___)
