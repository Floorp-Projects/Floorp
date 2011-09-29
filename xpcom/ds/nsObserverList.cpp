/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "nsObserverList.h"

#include "nsAutoPtr.h"
#include "nsCOMArray.h"
#include "nsISimpleEnumerator.h"

nsresult
nsObserverList::AddObserver(nsIObserver* anObserver, bool ownsWeak)
{
    NS_ASSERTION(anObserver, "Null input");

    if (!ownsWeak) {
        ObserverRef* o = mObservers.AppendElement(anObserver);
        if (!o)
            return NS_ERROR_OUT_OF_MEMORY;

        return NS_OK;
    }
        
    nsCOMPtr<nsIWeakReference> weak = do_GetWeakReference(anObserver);
    if (!weak)
        return NS_NOINTERFACE;

    ObserverRef *o = mObservers.AppendElement(weak);
    if (!o)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

nsresult
nsObserverList::RemoveObserver(nsIObserver* anObserver)
{
    NS_ASSERTION(anObserver, "Null input");

    if (mObservers.RemoveElement(static_cast<nsISupports*>(anObserver)))
        return NS_OK;

    nsCOMPtr<nsIWeakReference> observerRef = do_GetWeakReference(anObserver);
    if (!observerRef)
        return NS_ERROR_FAILURE;

    if (!mObservers.RemoveElement(observerRef))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
nsObserverList::GetObserverList(nsISimpleEnumerator** anEnumerator)
{
    nsRefPtr<nsObserverEnumerator> e(new nsObserverEnumerator(this));
    if (!e)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*anEnumerator = e);
    return NS_OK;
}

void
nsObserverList::FillObserverArray(nsCOMArray<nsIObserver> &aArray)
{
    aArray.SetCapacity(mObservers.Length());

    nsTArray<ObserverRef> observers(mObservers);

    for (PRInt32 i = observers.Length() - 1; i >= 0; --i) {
        if (observers[i].isWeakRef) {
            nsCOMPtr<nsIObserver> o(do_QueryReferent(observers[i].asWeak()));
            if (o) {
                aArray.AppendObject(o);
            }
            else {
                // the object has gone away, remove the weakref
                mObservers.RemoveElement(observers[i].asWeak());
            }
        }
        else {
            aArray.AppendObject(observers[i].asObserver());
        }
    }
}

void
nsObserverList::NotifyObservers(nsISupports *aSubject,
                                const char *aTopic,
                                const PRUnichar *someData)
{
    nsCOMArray<nsIObserver> observers;
    FillObserverArray(observers);

    for (PRInt32 i = 0; i < observers.Count(); ++i) {
        observers[i]->Observe(aSubject, aTopic, someData);
    }
}

NS_IMPL_ISUPPORTS1(nsObserverEnumerator, nsISimpleEnumerator)

nsObserverEnumerator::nsObserverEnumerator(nsObserverList* aObserverList)
    : mIndex(0)
{
    aObserverList->FillObserverArray(mObservers);
}

NS_IMETHODIMP
nsObserverEnumerator::HasMoreElements(bool *aResult)
{
    *aResult = (mIndex < mObservers.Count());
    return NS_OK;
}

NS_IMETHODIMP
nsObserverEnumerator::GetNext(nsISupports* *aResult)
{
    if (mIndex == mObservers.Count()) {
        NS_ERROR("Enumerating after HasMoreElements returned false.");
        return NS_ERROR_UNEXPECTED;
    }

    NS_ADDREF(*aResult = mObservers[mIndex]);
    ++mIndex;
    return NS_OK;
}
