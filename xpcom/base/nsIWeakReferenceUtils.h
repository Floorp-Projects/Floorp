/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@mozilla.org> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIWeakReferenceUtils_h__
#define nsIWeakReferenceUtils_h__

#ifndef nsCOMPtr_h__
#include "nsCOMPtr.h"
#endif

typedef nsCOMPtr<nsIWeakReference> nsWeakPtr;

/**
 *
 */

// a type-safe shortcut for calling the |QueryReferent()| member function
// T must inherit from nsIWeakReference, but the cast may be ambiguous.
template <class T, class DestinationType>
inline
nsresult
CallQueryReferent( T* aSource, DestinationType** aDestination )
  {
    NS_PRECONDITION(aSource, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");

    return aSource->QueryReferent(NS_GET_IID(DestinationType),
                                  NS_REINTERPRET_CAST(void**, aDestination));
  }


class NS_EXPORT nsQueryReferent : public nsCOMPtr_helper
  {
    public:
      nsQueryReferent( nsIWeakReference* aWeakPtr, nsresult* error )
          : mWeakPtr(aWeakPtr),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      virtual nsresult operator()( const nsIID& aIID, void** ) const;

    private:
      nsIWeakReference*  mWeakPtr;
      nsresult*          mErrorPtr;
  };

inline
const nsQueryReferent
do_QueryReferent( nsIWeakReference* aRawPtr, nsresult* error = 0 )
  {
    return nsQueryReferent(aRawPtr, error);
  }



class NS_COM nsGetWeakReference : public nsCOMPtr_helper
  {
    public:
      nsGetWeakReference( nsISupports* aRawPtr, nsresult* error )
          : mRawPtr(aRawPtr),
            mErrorPtr(error)
        {
          // nothing else to do here
        }

      virtual nsresult operator()( const nsIID&, void** ) const;

    private:
      nsISupports*  mRawPtr;
      nsresult*     mErrorPtr;
  };

  /**
   * |do_GetWeakReference| is a convenience function that bundles up all the work needed
   * to get a weak reference to an arbitrary object, i.e., the |QueryInterface|, test, and
   * call through to |GetWeakReference|, and put it into your |nsCOMPtr|.
   * It is specifically designed to cooperate with |nsCOMPtr| (or |nsWeakPtr|) like so:
   * |nsWeakPtr myWeakPtr = do_GetWeakReference(aPtr);|.
   */
inline
const nsGetWeakReference
do_GetWeakReference( nsISupports* aRawPtr, nsresult* error = 0 )
  {
    return nsGetWeakReference(aRawPtr, error);
  }

inline
void
do_GetWeakReference( nsIWeakReference* aRawPtr, nsresult* error = 0 )
  {
    // This signature exists soley to _stop_ you from doing a bad thing.
    //  Saying |do_GetWeakReference()| on a weak reference itself,
    //  is very likely to be a programmer error.
  }

template <class T>
inline
void
do_GetWeakReference( already_AddRefed<T>& )
  {
    // This signature exists soley to _stop_ you from doing the bad thing.
    //  Saying |do_GetWeakReference()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }

template <class T>
inline
void
do_GetWeakReference( already_AddRefed<T>&, nsresult* )
  {
    // This signature exists soley to _stop_ you from doing the bad thing.
    //  Saying |do_GetWeakReference()| on a pointer that is not otherwise owned by
    //  someone else is an automatic leak.  See <http://bugzilla.mozilla.org/show_bug.cgi?id=8221>.
  }



  /**
   * Deprecated, use |do_GetWeakReference| instead.
   */
extern NS_COM
nsIWeakReference*
NS_GetWeakReference( nsISupports* , nsresult* aResult=0 );

#endif
