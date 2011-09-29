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
 * The Original Code is XPCOM Array implementation.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsArray.h"
#include "nsArrayEnumerator.h"
#include "nsWeakReference.h"
#include "nsThreadUtils.h"

// used by IndexOf()
struct findIndexOfClosure
{
    nsISupports *targetElement;
    PRUint32 startIndex;
    PRUint32 resultIndex;
};

static bool FindElementCallback(void* aElement, void* aClosure);

NS_INTERFACE_MAP_BEGIN(nsArray)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIMutableArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutableArray)
NS_INTERFACE_MAP_END

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(nsArrayCC)
  NS_INTERFACE_MAP_ENTRY(nsIArray)
  NS_INTERFACE_MAP_ENTRY(nsIMutableArray)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIMutableArray)
NS_INTERFACE_MAP_END

nsArray::~nsArray()
{
    Clear();
}


NS_IMPL_ADDREF(nsArray)
NS_IMPL_RELEASE(nsArray)

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsArrayCC)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsArrayCC)

NS_IMPL_CYCLE_COLLECTION_CLASS(nsArrayCC)
NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(nsArrayCC)
    tmp->Clear();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(nsArrayCC)
    NS_IMPL_CYCLE_COLLECTION_TRAVERSE_NSCOMARRAY(mArray)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMETHODIMP
nsArray::GetLength(PRUint32* aLength)
{
    *aLength = mArray.Count();
    return NS_OK;
}

NS_IMETHODIMP
nsArray::QueryElementAt(PRUint32 aIndex,
                        const nsIID& aIID,
                        void ** aResult)
{
    nsISupports * obj = mArray.SafeObjectAt(aIndex);
    if (!obj) return NS_ERROR_ILLEGAL_VALUE;

    // no need to worry about a leak here, because SafeObjectAt() 
    // doesn't addref its result
    return obj->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP
nsArray::IndexOf(PRUint32 aStartIndex, nsISupports* aElement,
                 PRUint32* aResult)
{
    // optimize for the common case by forwarding to mArray
    if (aStartIndex == 0) {
        PRUint32 idx = mArray.IndexOf(aElement);
        if (idx == PR_UINT32_MAX)
            return NS_ERROR_FAILURE;

        *aResult = idx;
        return NS_OK;
    }

    findIndexOfClosure closure = { aElement, aStartIndex, 0 };
    bool notFound = mArray.EnumerateForwards(FindElementCallback, &closure);
    if (notFound)
        return NS_ERROR_FAILURE;

    *aResult = closure.resultIndex;
    return NS_OK;
}

NS_IMETHODIMP
nsArray::Enumerate(nsISimpleEnumerator **aResult)
{
    return NS_NewArrayEnumerator(aResult, static_cast<nsIArray*>(this));
}

// nsIMutableArray implementation

NS_IMETHODIMP
nsArray::AppendElement(nsISupports* aElement, bool aWeak)
{
    bool result;
    if (aWeak) {
        nsCOMPtr<nsISupports> elementRef =
            getter_AddRefs(static_cast<nsISupports*>
                                      (NS_GetWeakReference(aElement)));
        NS_ASSERTION(elementRef, "AppendElement: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
        result = mArray.AppendObject(elementRef);
    }

    else {
        // add the object directly
        result = mArray.AppendObject(aElement);
    }
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArray::RemoveElementAt(PRUint32 aIndex)
{
    bool result = mArray.RemoveObjectAt(aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArray::InsertElementAt(nsISupports* aElement, PRUint32 aIndex, bool aWeak)
{
    nsCOMPtr<nsISupports> elementRef;
    if (aWeak) {
        elementRef =
            getter_AddRefs(static_cast<nsISupports*>
                                      (NS_GetWeakReference(aElement)));
        NS_ASSERTION(elementRef, "InsertElementAt: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
    } else {
        elementRef = aElement;
    }
    bool result = mArray.InsertObjectAt(elementRef, aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArray::ReplaceElementAt(nsISupports* aElement, PRUint32 aIndex, bool aWeak)
{
    nsCOMPtr<nsISupports> elementRef;
    if (aWeak) {
        elementRef =
            getter_AddRefs(static_cast<nsISupports*>
                                      (NS_GetWeakReference(aElement)));
        NS_ASSERTION(elementRef, "ReplaceElementAt: Trying to use weak references on an object that doesn't support it");
        if (!elementRef)
            return NS_ERROR_FAILURE;
    } else {
        elementRef = aElement;
    }
    bool result = mArray.ReplaceObjectAt(elementRef, aIndex);
    return result ? NS_OK : NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsArray::Clear()
{
    mArray.Clear();
    return NS_OK;
}

//
// static helper routines
//
bool
FindElementCallback(void *aElement, void* aClosure)
{
    findIndexOfClosure* closure =
        static_cast<findIndexOfClosure*>(aClosure);

    nsISupports* element =
        static_cast<nsISupports*>(aElement);
    
    // don't start searching until we're past the startIndex
    if (closure->resultIndex >= closure->startIndex &&
        element == closure->targetElement) {
        return PR_FALSE;    // stop! We found it
    }
    closure->resultIndex++;

    return PR_TRUE;
}

nsresult
nsArrayConstructor(nsISupports *aOuter, const nsIID& aIID, void **aResult)
{
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    nsCOMPtr<nsIArray> inst = NS_IsMainThread() ? new nsArrayCC : new nsArray;
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;

    return inst->QueryInterface(aIID, aResult); 
}
