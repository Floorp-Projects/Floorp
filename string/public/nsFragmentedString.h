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
 * The Original Code is Mozilla XPCOM.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author:
 *   Scott Collins <scc@mozilla.org>
 *
 * Contributor(s):
 */

#ifndef nsFragmentedString_h___
#define nsFragmentedString_h___

  // WORK IN PROGRESS

#ifndef nsAWritableString_h___
#include "nsAWritableString.h"
#endif

template <class CharT>
class basic_nsFragmentedString
      : public basic_nsAWritableString<CharT>
    /*
      ...
    */
  {
    protected:
      virtual const void* Implementation() const;

      virtual const CharT* GetReadableFragment( nsReadableFragment<CharT>&, nsFragmentRequest, PRUint32 ) const;
      virtual CharT* GetWritableFragment( nsWritableFragment<CharT>&, nsFragmentRequest, PRUint32 );

    public:
      virtual PRUint32 Length() const;

      virtual void SetCapacity( PRUint32 aNewCapacity );
      virtual void SetLength( PRUint32 aNewLength );

    private:
      
      // ...
  };

  /**
   * |SetLength|
   */


  /**
   * |SetCapacity|.
   *
   * If a client tries to increase the capacity of multi-fragment string, perhaps a single
   * empty fragment of the appropriate size should be appended.
   */



#endif // !defined(nsFragmentedString_h___)
