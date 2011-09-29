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

#ifndef nsAtomTable_h__
#define nsAtomTable_h__

#include "nsIAtom.h"
#include "nsStringBuffer.h"

/**
 * Note that AtomImpl objects are sometimes converted into PermanentAtomImpl
 * objects using placement new and just overwriting the vtable pointer.
 */

class AtomImpl : public nsIAtom {
public:
  AtomImpl(const nsAString& aString);

  // This is currently only used during startup when creating a permanent atom
  // from NS_RegisterStaticAtoms
  AtomImpl(nsStringBuffer* aData, PRUint32 aLength);

protected:
  // This is only intended to be used when a normal atom is turned into a
  // permanent one.
  AtomImpl() {
    // We can't really assert that mString is a valid nsStringBuffer string,
    // so do the best we can do and check for some consistencies.
    NS_ASSERTION((mLength + 1) * sizeof(PRUnichar) <=
                 nsStringBuffer::FromData(mString)->StorageSize() &&
                 mString[mLength] == 0,
                 "Not initialized atom");
  }

  // We don't need a virtual destructor here because PermanentAtomImpl
  // deletions aren't handled through Release().
  ~AtomImpl();

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIATOM

  enum { REFCNT_PERMANENT_SENTINEL = PR_UINT32_MAX };

  virtual bool IsPermanent();

  // We can't use the virtual function in the base class destructor.
  bool IsPermanentInDestructor() {
    return mRefCnt == REFCNT_PERMANENT_SENTINEL;
  }

  // for |#ifdef NS_BUILD_REFCNT_LOGGING| access to reference count
  nsrefcnt GetRefCount() { return mRefCnt; }
};

/**
 * A non-refcounted implementation of nsIAtom.
 */

class PermanentAtomImpl : public AtomImpl {
public:
  PermanentAtomImpl(const nsAString& aString)
    : AtomImpl(aString)
  {}
  PermanentAtomImpl(nsStringBuffer* aData, PRUint32 aLength)
    : AtomImpl(aData, aLength)
  {}
  PermanentAtomImpl()
  {}

  ~PermanentAtomImpl();
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual bool IsPermanent();

  void* operator new(size_t size, AtomImpl* aAtom) CPP_THROW_NEW;
  void* operator new(size_t size) CPP_THROW_NEW
  {
    return ::operator new(size);
  }

};

void NS_PurgeAtomTable();

#endif // nsAtomTable_h__
