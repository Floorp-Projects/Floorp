/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsCOMArray_h__
#define nsCOMArray_h__

#include "nsVoidArray.h"
#include "nsISupports.h"

// a non-XPCOM, refcounting array of XPCOM objects
// used as a member variable or stack variable - this object is NOT
// refcounted, but the objects that it holds are
template <class T>
class NS_COM nsCOMArray
{
 public:
    nsCOMArray() {}
    nsCOMArray(PRInt32 aCount) : mArray(aCount) {}

    ~nsCOMArray() {}

    // these do NOT refcount on the way out, for speed
    T* ObjectAt(PRInt32 aIndex) const {
        return NS_STATIC_CAST(T*,mArray.ElementAt(aIndex));
    }
    
    T* operator[](PRInt32 aIndex) const {
        return ObjectAt(aIndex);
    }

    PRInt32 IndexOf(T* aObject) {
        return mArray.IndexOf(aObject);
    }
    
    PRBool InsertObjectAt(T* aObject, PRInt32 aIndex) {
        PRBool result = mArray.InsertElementAt(aObject, aIndex);
        if (result)
            NS_ADDREF(aObject);
        return result;
    }
    
    PRBool ReplaceObjectAt(T* aObject, PRInt32 aIndex) {
        T *oldObject = ObjectAt(aIndex);
        NS_IF_RELEASE(oldObject);
        PRBool result = mArray.ReplaceElementAt(aObject, aIndex);
        if (result)
            NS_ADDREF(aObject);
        return result;
    }

    // override nsVoidArray stuff so that they can be accessed by
    // other methods
    PRInt32 Count() const {
        return mArray.Count();
    }

    void Clear() {
        mArray.EnumerateForwards(ClearObjectsCallback, nsnull);
        mArray.Clear();
    }

    PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData) {
        return mArray.EnumerateForwards(aFunc, aData);
    }
    
    PRBool AppendObject(T *aObject) {
        PRBool result = InsertObjectAt(aObject, Count());
        if (result)
            NS_ADDREF(aObject);
        return result;
    }

    PRBool RemoveObject(T *aObject) {
        PRBool result = mArray.RemoveElement(aObject);
        if (result)
            NS_RELEASE(aObject);
        return result;
    }
    
    PRBool RemoveObjectAt(PRInt32 aIndex) {
        T* element = ObjectAt(aIndex);
        if (element) {
            PRBool result = mArray.RemoveElementAt(aIndex);
            if (result)
                NS_RELEASE(element);
            return result;
        }
        return PR_FALSE;
    }


 private:
    
    static PRBool ClearObjectsCallback(void *aElement, void*) {
        T* element = NS_STATIC_CAST(nsISupports*, aElement);
        NS_RELEASE(element);
        return PR_TRUE;
    }

    nsCOMArray(const nsCOMArray& other);
    nsCOMArray& operator=(const nsCOMArray& other);
    nsVoidArray mArray;
};


#endif
