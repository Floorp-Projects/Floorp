/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
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
 * The Initial Developer of the Original Code is the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2010
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

#include "TestHarness.h"
#include "nsCOMArray.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class IFoo : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(PRInt32) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class Foo : public IFoo {
public:

  Foo(PRInt32 aID);
  ~Foo();

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() { return mRefCnt; }
  NS_IMETHOD_(PRInt32) ID() { return mID; }

  static PRInt32 gCount;

  PRInt32 mID;
};

PRInt32 Foo::gCount = 0;

Foo::Foo(PRInt32 aID)
{
  mID = aID;
  ++gCount;
}

Foo::~Foo()
{
  --gCount;
}

NS_IMPL_ISUPPORTS1(Foo, IFoo)


typedef nsCOMArray<IFoo> Array;


int main(int argc, char **argv)
{
  ScopedXPCOM xpcom("nsCOMArrayTests");
  if (xpcom.failed()) {
    return 1;
  }

  int rv = 0;

  Array arr;

  for (PRInt32 i = 0; i < 20; ++i) {
    nsCOMPtr<IFoo> foo = new Foo(i);
    arr.AppendObject(foo);
  }

  if (arr.Count() != 20 || Foo::gCount != 20) {
    fail("nsCOMArray::AppendObject failed");
    rv = 1;
  }

  arr.SetCount(10);

  if (arr.Count() != 10 || Foo::gCount != 10) {
    fail("nsCOMArray::SetCount shortening of array failed");
    rv = 1;
  }

  arr.SetCount(30);

  if (arr.Count() != 30 || Foo::gCount != 10) {
    fail("nsCOMArray::SetCount lengthening of array failed");
    rv = 1;
  }

  for (PRInt32 i = 0; i < 10; ++i) {
	if (arr[i] == nsnull) {
      fail("nsCOMArray elements should be non-null");
      rv = 1;
	  break;
    }
  }

  for (PRInt32 i = 10; i < 30; ++i) {
	if (arr[i] != nsnull) {
      fail("nsCOMArray elements should be null");
      rv = 1;
	  break;
    }
  }

  return rv;
}
