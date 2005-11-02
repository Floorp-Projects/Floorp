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

static PRBool AddRefObjects(void* aElement, void*);
static PRBool ReleaseObjects(void* aElement, void*);


// implementations of non-trivial methods in nsCOMArray_base

// copy constructor - we can't just memcpy here, because
// we have to make sure we own our own array buffer, and that each
// object gets another AddRef()
nsCOMArray_base::nsCOMArray_base(const nsCOMArray_base& aOther)
{
    PRInt32 count = aOther.Count();
    // make sure we do only one allocation
    mArray.SizeTo(count);
    
    PRInt32 i;
    for (i=0; i<count; i++) {
        ReplaceObjectAt(aOther[i], i);
    }
}

PRBool
nsCOMArray_base::InsertObjectAt(nsISupports* aObject, PRInt32 aIndex) {
    PRBool result = mArray.InsertElementAt(aObject, aIndex);
    if (result)
        NS_IF_ADDREF(aObject);
    return result;
}

PRBool
nsCOMArray_base::ReplaceObjectAt(nsISupports* aObject, PRInt32 aIndex)
{
    nsISupports *oldObject = ObjectAt(aIndex);
    if (oldObject) {
        PRBool result = mArray.ReplaceElementAt(aObject, aIndex);

        // ReplaceElementAt could fail, such as if the array grows
        // so only release the existing object if the replacement succeeded
        if (result) {
            NS_IF_RELEASE(oldObject);
            NS_IF_ADDREF(aObject);
        }
        return result;
    }
    return PR_FALSE;
}


PRBool
nsCOMArray_base::AppendObject(nsISupports *aObject)
{
    PRBool result = InsertObjectAt(aObject, Count());
    if (result)
        NS_IF_ADDREF(aObject);
    return result;
}

PRBool
nsCOMArray_base::RemoveObject(nsISupports *aObject)
{
    PRBool result = mArray.RemoveElement(aObject);
    if (result)
        NS_IF_RELEASE(aObject);
    return result;
}

PRBool
nsCOMArray_base::RemoveObjectAt(PRInt32 aIndex)
{
    nsISupports* element = ObjectAt(aIndex);
    if (element) {
        PRBool result = mArray.RemoveElementAt(aIndex);
        if (result)
            NS_IF_RELEASE(element);
        return result;
    }
    return PR_FALSE;
}


// useful for copy constructors
PRBool
AddRefObjects(void* aElement, void*)
{
    nsISupports* element = NS_STATIC_CAST(nsISupports*,aElement);
    NS_IF_ADDREF(element);
    return PR_TRUE;
}

// useful for destructors
PRBool
ReleaseObjects(void* aElement, void*)
{
    nsISupports* element = NS_STATIC_CAST(nsISupports*, aElement);
    NS_IF_RELEASE(element);
    return PR_TRUE;
}

void
nsCOMArray_base::Clear()
{
    mArray.EnumerateForwards(ReleaseObjects, nsnull);
    mArray.Clear();
}

