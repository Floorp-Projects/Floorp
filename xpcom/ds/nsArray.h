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
 * The Initial Developer of the Original Code
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

#ifndef nsArray_h__
#define nsArray_h__

#include "nsIArray.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"

#define NS_ARRAY_CLASSNAME \
  "nsIArray implementation"

// {35C66FD1-95E9-4e0a-80C5-C3BD2B375481}
#define NS_ARRAY_CID \
{ 0x35c66fd1, 0x95e9, 0x4e0a, \
  { 0x80, 0xc5, 0xc3, 0xbd, 0x2b, 0x37, 0x54, 0x81 } }


// create a new, empty array
nsresult NS_COM
NS_NewArray(nsIMutableArray** aResult);

// The resulting array will hold an owning reference to every element
// in the original nsCOMArray<T>. This also means that any further
// changes to the original nsCOMArray<T> will not affect the new
// array, and that the original array can go away and the new array
// will still hold valid elements.
nsresult NS_COM
NS_NewArray(nsIMutableArray** aResult, const nsCOMArray_base& base);

// adapter class to map nsIArray->nsCOMArray
// do NOT declare this as a stack or member variable, use
// nsCOMArray instead!
// if you need to convert a nsCOMArray->nsIArray, see NS_NewArray above
class nsArray : public nsIMutableArray
{
public:
    nsArray() { }
    nsArray(const nsCOMArray_base& aBaseArray) : mArray(aBaseArray)
    { }
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIARRAY
    NS_DECL_NSIMUTABLEARRAY

private:
    ~nsArray();

    nsCOMArray_base mArray;
};


// helper class for do_QueryElementAt
class NS_COM nsQueryArrayElementAt : public nsCOMPtr_helper
  {
    public:
      nsQueryArrayElementAt(nsIArray* aArray, PRUint32 aIndex,
                            nsresult* aErrorPtr)
          : mArray(aArray),
            mIndex(aIndex),
            mErrorPtr(aErrorPtr)
        {
          // nothing else to do here
        }

      virtual nsresult NS_FASTCALL operator()(const nsIID& aIID, void**) const;

    private:
      nsIArray*  mArray;
      PRUint32   mIndex;
      nsresult*  mErrorPtr;
  };

inline
const nsQueryArrayElementAt
do_QueryElementAt(nsIArray* aArray, PRUint32 aIndex, nsresult* aErrorPtr = 0)
  {
    return nsQueryArrayElementAt(aArray, aIndex, aErrorPtr);
  }


#endif
