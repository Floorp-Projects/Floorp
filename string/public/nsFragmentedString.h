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
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (original author)
 *
 */

#ifndef nsFragmentedString_h___
#define nsFragmentedString_h___

  // WORK IN PROGRESS

#ifndef nsAString_h___
#include "nsAString.h"
#endif

#ifndef nsSharedBufferList_h___
#include "nsSharedBufferList.h"
#endif


class nsFragmentedString
      : public nsAString
    /*
      ...
    */
  {
    protected:
      virtual const char_type* GetReadableFragment( const_fragment_type&, nsFragmentRequest, PRUint32 ) const;
      virtual       char_type* GetWritableFragment(       fragment_type&, nsFragmentRequest, PRUint32 );

    public:
      nsFragmentedString() { }

      virtual PRUint32 Length() const;

      virtual void SetLength( PRUint32 aNewLength );
      // virtual void SetCapacity( PRUint32 aNewCapacity );

      // virtual void Cut( PRUint32 cutStart, PRUint32 cutLength );

    protected:
      // virtual void do_AssignFromReadable( const abstract_string_type& );
      // virtual void do_AppendFromReadable( const abstract_string_type& );
      // virtual void do_InsertFromReadable( const abstract_string_type&, PRUint32 );
      // virtual void do_ReplaceFromReadable( PRUint32, PRUint32, const abstract_string_type& );

    private:
      nsSharedBufferList  mBufferList;
  };

#endif // !defined(nsFragmentedString_h___)
