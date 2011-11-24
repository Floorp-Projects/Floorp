/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott Collins <scc@netscape.com>
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// nsWeakReference.cpp

#include "mozilla/Attributes.h"

#include "nsWeakReference.h"
#include "nsCOMPtr.h"

class nsWeakReference MOZ_FINAL : public nsIWeakReference
  {
    public:
    // nsISupports...
      NS_DECL_ISUPPORTS

    // nsIWeakReference...
      NS_DECL_NSIWEAKREFERENCE

    private:
      friend class nsSupportsWeakReference;

      nsWeakReference( nsSupportsWeakReference* referent )
          : mReferent(referent)
          // ...I can only be constructed by an |nsSupportsWeakReference|
        {
          // nothing else to do here
        }

      ~nsWeakReference()
           // ...I will only be destroyed by calling |delete| myself.
        {
          if ( mReferent )
            mReferent->NoticeProxyDestruction();
        }

      void
      NoticeReferentDestruction()
          // ...called (only) by an |nsSupportsWeakReference| from _its_ dtor.
        {
          mReferent = 0;
        }

      nsSupportsWeakReference*  mReferent;
  };

nsresult
nsQueryReferent::operator()( const nsIID& aIID, void** answer ) const
  {
    nsresult status;
    if ( mWeakPtr )
      {
        if ( NS_FAILED(status = mWeakPtr->QueryReferent(aIID, answer)) )
          *answer = 0;
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( mErrorPtr )
      *mErrorPtr = status;
    return status;
  }

NS_COM_GLUE nsIWeakReference*  // or else |already_AddRefed<nsIWeakReference>|
NS_GetWeakReference( nsISupports* aInstancePtr, nsresult* aErrorPtr )
  {
    nsresult status;

    nsIWeakReference* result = nsnull;

    if ( aInstancePtr )
      {
        nsCOMPtr<nsISupportsWeakReference> factoryPtr = do_QueryInterface(aInstancePtr, &status);
        NS_ASSERTION(factoryPtr, "Oops!  You're asking for a weak reference to an object that doesn't support that.");
        if ( factoryPtr )
          {
            status = factoryPtr->GetWeakReference(&result);
          }
        // else, |status| has already been set by |do_QueryInterface|
      }
    else
      status = NS_ERROR_NULL_POINTER;

    if ( aErrorPtr )
      *aErrorPtr = status;
    return result;
  }

NS_COM_GLUE nsresult
nsSupportsWeakReference::GetWeakReference( nsIWeakReference** aInstancePtr )
  {
    if ( !aInstancePtr )
      return NS_ERROR_NULL_POINTER;

    if ( !mProxy )
      mProxy = new nsWeakReference(this);
    *aInstancePtr = mProxy;

    nsresult status;
    if ( !*aInstancePtr )
      status = NS_ERROR_OUT_OF_MEMORY;
    else
      {
        NS_ADDREF(*aInstancePtr);
        status = NS_OK;
      }

    return status;
  }

NS_IMPL_ISUPPORTS1(nsWeakReference, nsIWeakReference)

NS_IMETHODIMP
nsWeakReference::QueryReferent( const nsIID& aIID, void** aInstancePtr )
  {
    return mReferent ? mReferent->QueryInterface(aIID, aInstancePtr) : NS_ERROR_NULL_POINTER;
  }

void
nsSupportsWeakReference::ClearWeakReferences()
	{
		if ( mProxy )
			{
				mProxy->NoticeReferentDestruction();
				mProxy = 0;
			}
	}

