/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsAtomTable_h__
#define nsAtomTable_h__

#include "nsIAtom.h"

/**
 * A threadsafely-refcounted implementation of nsIAtom.  Note that
 * AtomImpl objects are sometimes converted into PermanentAtomImpl
 * objects using placement new and just overwriting the vtable pointer.
 */

class AtomImpl : public nsIAtom {
public:
  AtomImpl();
  virtual ~AtomImpl();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  virtual PRBool IsPermanent();

  void* operator new(size_t size, const nsAString& aString);

  void operator delete(void* ptr) {
    ::operator delete(ptr);
  }

  // for |#ifdef NS_BUILD_REFCNT_LOGGING| access to reference count
  nsrefcnt GetRefCount() { return mRefCnt; }

  // Actually more; 0 terminated. This slot is reserved for the
  // terminating zero.
  PRUnichar mString[1];
};

/**
 * A non-refcounted implementation of nsIAtom.
 */

class PermanentAtomImpl : public AtomImpl {
public:
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual PRBool IsPermanent();

  void* operator new(size_t size, const nsAReadableString& aString) {
    return AtomImpl::operator new(size, aString);
  }
  void* operator new(size_t size, AtomImpl* aAtom);

};

void NS_PurgeAtomTable();

#endif // nsAtomTable_h__
