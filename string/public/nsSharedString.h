/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 */

#ifndef _nsSharedString_h__
#define _nsSharedString_h__

  // WORK IN PROGRESS

#include "nsAReadableString.h"

template <class CharT>
class basic_nsSharedString
      : public basic_nsAReadableString<CharT>
    /*
      ...
    */
  {
    public:

      nsrefcnt
      AddRef() const
        {
          return ++mRefCount;
        }

      nsrefcnt
      Release() const
        {
          nsrefcnt result = --mRefCount;
          if ( !mRefCount )
            delete this;
          return result;
        }

    private:
      mutable nsrefcnt mRefCount;
  };

template <class CharT>
class nsSharedStringPtr
  {
    public:
      // ...

    private:
      basic_nsSharedString<CharT>*  mPtr;
  };



typedef basic_nsSharedString<PRUnichar>     nsSharedString;
typedef basic_nsSharedStringPtr<PRUnichar>  nsSharedStringPtr;

typedef basic_nsSharedString<char>          nsSharedCString;
typedef basic_nsSharedStringPtr<char>       nsSharedCStringPtr;


#endif // !defined(_nsSharedString_h__)
