/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#ifndef __nsInterfaceRequestorUtils_h
#define __nsInterfaceRequestorUtils_h

#include "nsCOMPtr.h"

// a type-safe shortcut for calling the |GetInterface()| member function
// T must inherit from nsIInterfaceRequestor, but the cast may be ambiguous.
template <class T, class DestinationType>
inline
nsresult
CallGetInterface( T* aSource, DestinationType** aDestination )
  {
    NS_PRECONDITION(aSource, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return aSource->GetInterface(NS_GET_IID(DestinationType),
                                 NS_REINTERPRET_CAST(void**, aDestination));
  }

class NS_EXPORT nsGetInterface : public nsCOMPtr_helper
  {
    public:
      nsGetInterface( nsISupports* aSource, nsresult* error )
          : mSource(aSource),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      virtual nsresult operator()( const nsIID&, void** ) const;
      virtual ~nsGetInterface() {};

    private:
      nsCOMPtr<nsISupports> mSource;
      nsresult*             mErrorPtr;
  };

inline
const nsGetInterface
do_GetInterface( nsISupports* aSource, nsresult* error = 0 )
  {
    return nsGetInterface(aSource, error);
  }

#endif // __nsInterfaceRequestorUtils_h

