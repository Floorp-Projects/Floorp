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

// a class that's nsISupports-specific, so that we can contain the
// work of this class in the XPCOM dll
class NS_COM nsCOMArray_base
{
protected:
    nsCOMArray_base() {}
    nsCOMArray_base(PRInt32 aCount) : mArray(aCount) {}

    nsISupports* ObjectAt(PRInt32 aIndex) const {
        return NS_STATIC_CAST(nsISupports*, mArray.ElementAt(aIndex));
    }
    
    nsISupports* operator[](PRInt32 aIndex) const {
        return ObjectAt(aIndex);
    }

    PRInt32 IndexOf(nsISupports* aObject) {
        return mArray.IndexOf(aObject);
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
    
    // any method which is not a direct forward to mArray should
    // avoid inline bodies, so that the compiler doesn't inline them
    // all over the place
    PRBool InsertObjectAt(nsISupports* aObject, PRInt32 aIndex);
    PRBool ReplaceObjectAt(nsISupports* aObject, PRInt32 aIndex);
    PRBool AppendObject(nsISupports *aObject);
    PRBool RemoveObject(nsISupports *aObject);
    PRBool RemoveObjectAt(PRInt32 aIndex);

 private:
    
    static PRBool ClearObjectsCallback(void *aElement, void*);
    
    // the actual storage
    nsVoidArray mArray;
    
    nsCOMArray_base(const nsCOMArray_base& other);
    nsCOMArray_base& operator=(const nsCOMArray_base& other);
};

// a non-XPCOM, refcounting array of XPCOM objects
// used as a member variable or stack variable - this object is NOT
// refcounted, but the objects that it holds are
template <class T>
class nsCOMArray : protected nsCOMArray_base
{
 public:
    nsCOMArray() {}
    nsCOMArray(PRInt32 aCount) : nsCOMArray_base(aCount) {}

    ~nsCOMArray() {}

    // these do NOT refcount on the way out, for speed
    T* ObjectAt(PRInt32 aIndex) const {
        return NS_STATIC_CAST(T*,nsCOMArray_base::ObjectAt(aIndex));
    }
    
    T* operator[](PRInt32 aIndex) const {
        return ObjectAt(aIndex);
    }

    PRInt32 IndexOf(T* aObject) {
        return nsCOMArray_base::IndexOf(aObject);
    }
    
    PRBool InsertObjectAt(T* aObject, PRInt32 aIndex) {
        return nsCOMArray_base::InsertObjectAt(aObject, aIndex);
    }
    
    PRBool ReplaceObjectAt(T* aObject, PRInt32 aIndex) {
        return nsCOMArray_base::ReplaceObjectAt(aObject, aIndex);
    }

    // override nsVoidArray stuff so that they can be accessed by
    // other methods
    PRInt32 Count() const {
        return nsCOMArray_base::Count();
    }

    void Clear() {
        nsCOMArray_base::Clear();
    }

    PRBool EnumerateForwards(nsVoidArrayEnumFunc aFunc, void* aData) {
        return nsCOMArray_base::EnumerateForwards(aFunc, aData);
    }
    
    PRBool AppendObject(T *aObject) {
        return nsCOMArray_base::AppendObject(aObject);
    }

    PRBool RemoveObject(T *aObject) {
        return nsCOMArray_base::RemoveObject(aObject);
    }
    
    PRBool RemoveObjectAt(PRInt32 aIndex) {
        return nsCOMArray_base::RemoveObjectAt(aIndex);
    }


 private:
    
    nsCOMArray(const nsCOMArray& other);
    nsCOMArray& operator=(const nsCOMArray& other);
};


#endif
