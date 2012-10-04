/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsWeakReference_h__
#define nsWeakReference_h__

// nsWeakReference.h

// See mfbt/WeakPtr.h for a more typesafe C++ implementation of weak references

#include "nsIWeakReferenceUtils.h"

class nsWeakReference;

// Set IMETHOD_VISIBILITY to empty so that the class-level NS_COM declaration
// controls member method visibility.
#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_COM_GLUE

class NS_COM_GLUE nsSupportsWeakReference : public nsISupportsWeakReference
  {
    public:
      nsSupportsWeakReference()
          : mProxy(0)
        {
          // nothing else to do here
        }

      NS_DECL_NSISUPPORTSWEAKREFERENCE

    protected:
      inline ~nsSupportsWeakReference();

    private:
      friend class nsWeakReference;

      void
      NoticeProxyDestruction()
          // ...called (only) by an |nsWeakReference| from _its_ dtor.
        {
          mProxy = 0;
        }

      nsWeakReference* mProxy;

		protected:

			void ClearWeakReferences();
			bool HasWeakReferences() const {return mProxy != 0;}
  };

#undef  IMETHOD_VISIBILITY
#define IMETHOD_VISIBILITY NS_VISIBILITY_HIDDEN

inline
nsSupportsWeakReference::~nsSupportsWeakReference()
  {
  	ClearWeakReferences();
  }

#endif
