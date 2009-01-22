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
 * The Original Code is String Enumerator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

#include "nsIStringEnumerator.h"
#include "nsString.h"
#include "nsTArray.h"

// nsIStringEnumerator/nsIUTF8StringEnumerator implementations
//
// Currently all implementations support both interfaces. The
// constructors below provide the most common interface for the given
// type (i.e. nsIStringEnumerator for PRUnichar* strings, and so
// forth) but any resulting enumerators can be queried to the other
// type. Internally, the enumerators will hold onto the type that was
// passed in and do conversion if GetNext() for the other type of
// string is called.

// There are a few different types of enumerators:

//
// These enumerators hold a pointer to the array. Be careful
// because modifying the array may confuse the iterator, especially if
// you insert or remove elements in the middle of the array.
//

// The non-adopting enumerator requires that the array sticks around
// at least as long as the enumerator does. These are for constant
// string arrays that the enumerator does not own, this could be used
// in VERY specialized cases such as when the provider KNOWS that the
// string enumerator will be consumed immediately, or will at least
// outlast the array.
// For example:
//
// nsTArray<nsCString> array;
// array.AppendCString("abc");
// array.AppendCString("def");
// NS_NewStringEnumerator(&enumerator, &array, PR_TRUE);
//
// // call some internal method which iterates the enumerator
// InternalMethod(enumerator);
// NS_RELEASE(enumerator);
//
NS_COM nsresult
NS_NewStringEnumerator(nsIStringEnumerator** aResult NS_OUTPARAM,
                       const nsTArray<nsString>* aArray,
                       nsISupports* aOwner);
NS_COM nsresult
NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult NS_OUTPARAM,
                           const nsTArray<nsCString>* aArray);

NS_COM nsresult
NS_NewStringEnumerator(nsIStringEnumerator** aResult NS_OUTPARAM,
                       const nsTArray<nsString>* aArray);

// Adopting string enumerators assume ownership of the array and will
// call |operator delete| on the array when the enumerator is destroyed
// this is useful when the provider creates an array solely for the
// purpose of creating the enumerator.
// For example:
//
// nsTArray<nsCString>* array = new nsTArray<nsCString>;
// array->AppendString("abcd");
// NS_NewAdoptingStringEnumerator(&result, array);
NS_COM nsresult
NS_NewAdoptingStringEnumerator(nsIStringEnumerator** aResult NS_OUTPARAM,
                               nsTArray<nsString>* aArray);

NS_COM nsresult
NS_NewAdoptingUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult NS_OUTPARAM,
                                   nsTArray<nsCString>* aArray);


// these versions take a refcounted "owner" which will be addreffed
// when the enumerator is created, and destroyed when the enumerator
// is released. This allows providers to give non-owning pointers to
// ns*StringArray member variables without worrying about lifetime
// issues
// For example:
//
// nsresult MyClass::Enumerate(nsIUTF8StringEnumerator** aResult) {
//     mCategoryList->AppendString("abcd");
//     return NS_NewStringEnumerator(aResult, mCategoryList, this);
// }
//
NS_COM nsresult
NS_NewUTF8StringEnumerator(nsIUTF8StringEnumerator** aResult NS_OUTPARAM,
                           const nsTArray<nsCString>* aArray,
                           nsISupports* aOwner);
