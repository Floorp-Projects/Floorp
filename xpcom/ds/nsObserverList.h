/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsObserverList_h___
#define nsObserverList_h___

#include "nsISupports.h"
#include "nsTArray.h"
#include "nsCOMPtr.h"
#include "nsCOMArray.h"
#include "nsIObserver.h"
#include "nsIWeakReference.h"
#include "nsHashKeys.h"
#include "nsISimpleEnumerator.h"

struct ObserverRef
{
  ObserverRef(const ObserverRef& o) :
    isWeakRef(o.isWeakRef), ref(o.ref) { }
  
  ObserverRef(nsIObserver* aObserver) : isWeakRef(PR_FALSE), ref(aObserver) { }
  ObserverRef(nsIWeakReference* aWeak) : isWeakRef(PR_TRUE), ref(aWeak) { }

  bool isWeakRef;
  nsCOMPtr<nsISupports> ref;

  nsIObserver* asObserver() {
    NS_ASSERTION(!isWeakRef, "Isn't a strong ref.");
    return static_cast<nsIObserver*>((nsISupports*) ref);
  }

  nsIWeakReference* asWeak() {
    NS_ASSERTION(isWeakRef, "Isn't a weak ref.");
    return static_cast<nsIWeakReference*>((nsISupports*) ref);
  }

  bool operator==(nsISupports* b) const { return ref == b; }
};

class nsObserverList : public nsCharPtrHashKey
{
public:
  nsObserverList(const char *key) : nsCharPtrHashKey(key)
  { MOZ_COUNT_CTOR(nsObserverList); }

  ~nsObserverList() { MOZ_COUNT_DTOR(nsObserverList); }

  nsresult AddObserver(nsIObserver* anObserver, bool ownsWeak);
  nsresult RemoveObserver(nsIObserver* anObserver);

  void NotifyObservers(nsISupports *aSubject,
                       const char *aTopic,
                       const PRUnichar *someData);
  nsresult GetObserverList(nsISimpleEnumerator** anEnumerator);

  // Fill an array with the observers of this category.
  // The array is filled in last-added-first order.
  void FillObserverArray(nsCOMArray<nsIObserver> &aArray);

private:
  nsTArray<ObserverRef> mObservers;
};

class nsObserverEnumerator : public nsISimpleEnumerator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR

    nsObserverEnumerator(nsObserverList* aObserverList);

private:
    ~nsObserverEnumerator() { }

    PRInt32 mIndex; // Counts up from 0
    nsCOMArray<nsIObserver> mObservers;
};

#endif /* nsObserverList_h___ */
