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

#include "pratom.h"
#include "nsAutoLock.h"
#include "nsIObserver.h"
#include "nsCOMPtr.h"
#include "nsIWeakReference.h"
#include "nsArrayEnumerator.h"

nsObserverList::nsObserverList(nsresult &rv)
{
    MOZ_COUNT_CTOR(nsObserverList);
    mLock = PR_NewLock();
    if (!mLock)
        rv = NS_ERROR_OUT_OF_MEMORY;
}

nsObserverList::~nsObserverList(void)
{
    MOZ_COUNT_DTOR(nsObserverList);
    if (mLock)
        PR_DestroyLock(mLock);
}

nsresult
nsObserverList::AddObserver(nsIObserver* anObserver, PRBool ownsWeak)
{
    NS_ENSURE_ARG(anObserver);

    nsAutoLock lock(mLock);

    nsCOMPtr<nsISupports> observerRef;
    if (ownsWeak) {
        nsCOMPtr<nsISupportsWeakReference>
            weakRefFactory(do_QueryInterface(anObserver));
        NS_ASSERTION(weakRefFactory,
                     "Doesn't implement nsISupportsWeakReference");
        if (weakRefFactory)
            weakRefFactory->
                GetWeakReference((nsIWeakReference**)(nsISupports**)
                                 getter_AddRefs(observerRef));
    } else {
        observerRef = anObserver;
    }
    if (!observerRef)
        return NS_ERROR_FAILURE;

    if (!mObservers.AppendObject(observerRef))
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

nsresult
nsObserverList::RemoveObserver(nsIObserver* anObserver)
{
    NS_ENSURE_ARG(anObserver);

    nsAutoLock lock(mLock);

    if (mObservers.RemoveObject(anObserver))
        return NS_OK;

    nsCOMPtr<nsISupportsWeakReference>
        weakRefFactory(do_QueryInterface(anObserver));
    if (!weakRefFactory)
        return NS_ERROR_FAILURE;

    nsCOMPtr<nsIWeakReference> observerRef;
    weakRefFactory->GetWeakReference(getter_AddRefs(observerRef));

    if (!observerRef)
        return NS_ERROR_FAILURE;

    if (!mObservers.RemoveObject(observerRef))
        return NS_ERROR_FAILURE;

    return NS_OK;
}

nsresult
nsObserverList::GetObserverList(nsISimpleEnumerator** anEnumerator)
{
    nsAutoLock lock(mLock);

    return NS_NewArrayEnumerator(anEnumerator, mObservers, PR_TRUE);
}
