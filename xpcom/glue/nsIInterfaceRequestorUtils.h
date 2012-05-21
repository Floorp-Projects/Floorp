/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

    return aSource->GetInterface(NS_GET_TEMPLATE_IID(DestinationType),
                                 reinterpret_cast<void**>(aDestination));
  }

class NS_COM_GLUE nsGetInterface : public nsCOMPtr_helper
  {
    public:
      nsGetInterface( nsISupports* aSource, nsresult* error )
          : mSource(aSource),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      virtual nsresult NS_FASTCALL operator()( const nsIID&, void** ) const;

    private:
      nsISupports*          mSource;
      nsresult*             mErrorPtr;
  };

inline
const nsGetInterface
do_GetInterface( nsISupports* aSource, nsresult* error = 0 )
  {
    return nsGetInterface(aSource, error);
  }

#endif // __nsInterfaceRequestorUtils_h

