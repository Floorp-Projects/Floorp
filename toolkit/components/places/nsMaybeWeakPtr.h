/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Places code.
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Ryner <bryner@brianryner.com> (original author)
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

#ifndef nsMaybeWeakPtr_h_
#define nsMaybeWeakPtr_h_

#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsTArray.h"

// nsMaybeWeakPtr is a helper object to hold a strong-or-weak reference
// to the template class.  It's pretty minimal, but sufficient.

class nsMaybeWeakPtr_base
{
protected:
  // Returns an addref'd pointer to the requested interface
  void* GetValueAs(const nsIID& iid) const;

  nsCOMPtr<nsISupports> mPtr;
};

template<class T>
class nsMaybeWeakPtr : private nsMaybeWeakPtr_base
{
public:
  nsMaybeWeakPtr(nsISupports *ref) { mPtr = ref; }
  nsMaybeWeakPtr(const nsCOMPtr<nsIWeakReference> &ref) { mPtr = ref; }
  nsMaybeWeakPtr(const nsCOMPtr<T> &ref) { mPtr = ref; }

  PRBool operator==(const nsMaybeWeakPtr<T> &other) const {
    return mPtr == other.mPtr;
  }

  operator const nsCOMPtr<T>() const { return GetValue(); }

protected:
  const nsCOMPtr<T> GetValue() const {
    return nsCOMPtr<T>(dont_AddRef(static_cast<T*>
                                              (GetValueAs(NS_GET_TEMPLATE_IID(T)))));
  }
};

// nsMaybeWeakPtrArray is an array of MaybeWeakPtr objects, that knows how to
// grab a weak reference to a given object if requested.  It only allows a
// given object to appear in the array once.

typedef nsTArray< nsMaybeWeakPtr<nsISupports> > isupports_array_type;
nsresult NS_AppendWeakElementBase(isupports_array_type *aArray,
                                  nsISupports *aElement, PRBool aWeak);
nsresult NS_RemoveWeakElementBase(isupports_array_type *aArray,
                                  nsISupports *aElement);

template<class T>
class nsMaybeWeakPtrArray : public nsTArray< nsMaybeWeakPtr<T> >
{
public:
  nsresult AppendWeakElement(T *aElement, PRBool aOwnsWeak)
  {
    return NS_AppendWeakElementBase(
      reinterpret_cast<isupports_array_type*>(this), aElement, aOwnsWeak);
  }

  nsresult RemoveWeakElement(T *aElement)
  {
    return NS_RemoveWeakElementBase(
      reinterpret_cast<isupports_array_type*>(this), aElement);
  }
};

// Call a method on each element in the array, but only if the element is
// non-null.

#define ENUMERATE_WEAKARRAY(array, type, method)                           \
  for (PRUint32 array_idx = 0; array_idx < array.Length(); ++array_idx) {  \
    const nsCOMPtr<type> &e = array.ElementAt(array_idx);                  \
    if (e)                                                                 \
      e->method;                                                           \
  }

#endif
