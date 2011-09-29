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
 * The Original Code is a COM aware array class.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Alec Flett <alecf@netscape.com>
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

#include "nsCOMArray.h"
#include "nsCOMPtr.h"

static bool ReleaseObjects(void* aElement, void*);

// implementations of non-trivial methods in nsCOMArray_base

// copy constructor - we can't just memcpy here, because
// we have to make sure we own our own array buffer, and that each
// object gets another AddRef()
nsCOMArray_base::nsCOMArray_base(const nsCOMArray_base& aOther)
{
    // make sure we do only one allocation
    mArray.SizeTo(aOther.Count());
    AppendObjects(aOther);
}

nsCOMArray_base::~nsCOMArray_base()
{
  Clear();
}

PRInt32
nsCOMArray_base::IndexOfObject(nsISupports* aObject) const {
    nsCOMPtr<nsISupports> supports = do_QueryInterface(aObject);
    NS_ENSURE_TRUE(supports, -1);

    PRInt32 i, count;
    PRInt32 retval = -1;
    count = mArray.Count();
    for (i = 0; i < count; ++i) {
        nsCOMPtr<nsISupports> arrayItem =
            do_QueryInterface(reinterpret_cast<nsISupports*>(mArray.ElementAt(i)));
        if (arrayItem == supports) {
            retval = i;
            break;
        }
    }
    return retval;
}

bool
nsCOMArray_base::InsertObjectAt(nsISupports* aObject, PRInt32 aIndex) {
    bool result = mArray.InsertElementAt(aObject, aIndex);
    if (result)
        NS_IF_ADDREF(aObject);
    return result;
}

bool
nsCOMArray_base::InsertObjectsAt(const nsCOMArray_base& aObjects, PRInt32 aIndex) {
    bool result = mArray.InsertElementsAt(aObjects.mArray, aIndex);
    if (result) {
        // need to addref all these
        PRInt32 count = aObjects.Count();
        for (PRInt32 i = 0; i < count; ++i) {
            NS_IF_ADDREF(aObjects.ObjectAt(i));
        }
    }
    return result;
}

bool
nsCOMArray_base::ReplaceObjectAt(nsISupports* aObject, PRInt32 aIndex)
{
    // its ok if oldObject is null here
    nsISupports *oldObject =
        reinterpret_cast<nsISupports*>(mArray.SafeElementAt(aIndex));

    bool result = mArray.ReplaceElementAt(aObject, aIndex);

    // ReplaceElementAt could fail, such as if the array grows
    // so only release the existing object if the replacement succeeded
    if (result) {
        // Make sure to addref first, in case aObject == oldObject
        NS_IF_ADDREF(aObject);
        NS_IF_RELEASE(oldObject);
    }
    return result;
}

bool
nsCOMArray_base::RemoveObject(nsISupports *aObject)
{
    bool result = mArray.RemoveElement(aObject);
    if (result)
        NS_IF_RELEASE(aObject);
    return result;
}

bool
nsCOMArray_base::RemoveObjectAt(PRInt32 aIndex)
{
    if (PRUint32(aIndex) < PRUint32(Count())) {
        nsISupports* element = ObjectAt(aIndex);

        bool result = mArray.RemoveElementAt(aIndex);
        NS_IF_RELEASE(element);
        return result;
    }

    return PR_FALSE;
}

bool
nsCOMArray_base::RemoveObjectsAt(PRInt32 aIndex, PRInt32 aCount)
{
    if (PRUint32(aIndex) + PRUint32(aCount) <= PRUint32(Count())) {
        nsVoidArray elementsToDestroy(aCount);
        for (PRInt32 i = 0; i < aCount; ++i) {
            elementsToDestroy.InsertElementAt(mArray.FastElementAt(aIndex + i), i);
        }
        bool result = mArray.RemoveElementsAt(aIndex, aCount);
        for (PRInt32 i = 0; i < aCount; ++i) {
            nsISupports* element = static_cast<nsISupports*> (elementsToDestroy.FastElementAt(i));
            NS_IF_RELEASE(element);
        }
        return result;
    }

    return PR_FALSE;
}

// useful for destructors
bool
ReleaseObjects(void* aElement, void*)
{
    nsISupports* element = static_cast<nsISupports*>(aElement);
    NS_IF_RELEASE(element);
    return PR_TRUE;
}

void
nsCOMArray_base::Clear()
{
    nsAutoVoidArray objects;
    objects = mArray;
    mArray.Clear();
    objects.EnumerateForwards(ReleaseObjects, nsnull);
}

bool
nsCOMArray_base::SetCount(PRInt32 aNewCount)
{
    NS_ASSERTION(aNewCount >= 0,"SetCount(negative index)");
    if (aNewCount < 0)
      return PR_FALSE;

    PRInt32 count = Count(), i;
    nsAutoVoidArray objects;
    if (count > aNewCount) {
        objects.SetCount(count - aNewCount);
        for (i = aNewCount; i < count; ++i) {
            objects.ReplaceElementAt(ObjectAt(i), i - aNewCount);
        }
    }
    bool result = mArray.SetCount(aNewCount);
    objects.EnumerateForwards(ReleaseObjects, nsnull);
    return result;
}

