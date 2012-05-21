/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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


// {0e70a320-be02-11d1-8031-006008159b5a}
#define NS_IBAR_IID \
  {0x0e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class IBar : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBAR_IID)
};

NS_DEFINE_STATIC_IID_ACCESSOR(IBar, NS_IBAR_IID)

class Bar : public IBar {
public:

  explicit Bar(nsCOMArray<IBar>& aArray);
  ~Bar();

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  static PRInt32 sReleaseCalled;

private:
  nsCOMArray<IBar>& mArray;
};

PRInt32 Bar::sReleaseCalled = 0;

typedef nsCOMArray<IBar> Array2;

Bar::Bar(Array2& aArray)
  : mArray(aArray)
{
}

Bar::~Bar()
{
  if (mArray.RemoveObject(this)) {
    fail("We should never manage to remove the object here");
  }
}

NS_IMPL_ADDREF(Bar)
NS_IMPL_QUERY_INTERFACE1(Bar, IBar)

NS_IMETHODIMP_(nsrefcnt)
Bar::Release(void)
{
  ++Bar::sReleaseCalled;
  NS_PRECONDITION(0 != mRefCnt, "dup release");
  NS_ASSERT_OWNINGTHREAD(_class);
  --mRefCnt;
  NS_LOG_RELEASE(this, mRefCnt, "Bar");
  if (mRefCnt == 0) {
    mRefCnt = 1; /* stabilize */
    delete this;
    return 0;
  }
  return mRefCnt;
}


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

  PRInt32 base;
  {
    Array2 arr2;

    IBar *thirdObject,
         *fourthObject,
         *fifthObject,
         *ninthObject;
    for (PRInt32 i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar = new Bar(arr2);
      switch (i) {
      case 2:
        thirdObject = bar; break;
      case 3:
        fourthObject = bar; break;
      case 4:
        fifthObject = bar; break;
      case 8:
        ninthObject = bar; break;
      }
      arr2.AppendObject(bar);
    }

    base = Bar::sReleaseCalled;

    arr2.SetCount(10);
    if (Bar::sReleaseCalled != base + 10) {
      fail("Release called multiple times for SetCount");
    }
    if (arr2.Count() != 10) {
      fail("SetCount(10) should remove exactly ten objects");
    }

    arr2.RemoveObjectAt(9);
    if (Bar::sReleaseCalled != base + 11) {
      fail("Release called multiple times for RemoveObjectAt");
    }
    if (arr2.Count() != 9) {
      fail("RemoveObjectAt should remove exactly one object");
    }

    arr2.RemoveObject(ninthObject);
    if (Bar::sReleaseCalled != base + 12) {
      fail("Release called multiple times for RemoveObject");
    }
    if (arr2.Count() != 8) {
      fail("RemoveObject should remove exactly one object");
    }

    arr2.RemoveObjectsAt(2, 3);
    if (Bar::sReleaseCalled != base + 15) {
      fail("Release called more or less than three times for RemoveObjectsAt");
    }
    if (arr2.Count() != 5) {
      fail("RemoveObjectsAt should remove exactly three objects");
    }
    for (PRInt32 j = 0; j < arr2.Count(); ++j) {
      if (arr2.ObjectAt(j) == thirdObject) {
        fail("RemoveObjectsAt should have removed thirdObject");
      }
      if (arr2.ObjectAt(j) == fourthObject) {
        fail("RemoveObjectsAt should have removed fourthObject");
      }
      if (arr2.ObjectAt(j) == fifthObject) {
        fail("RemoveObjectsAt should have removed fifthObject");
      }
    }

    arr2.RemoveObjectsAt(4, 1);
    if (Bar::sReleaseCalled != base + 16) {
      fail("Release called more or less than one time for RemoveObjectsAt");
    }
    if (arr2.Count() != 4) {
      fail("RemoveObjectsAt should work for removing the last element");
    }

    arr2.Clear();
    if (Bar::sReleaseCalled != base + 20) {
      fail("Release called multiple times for Clear");
    }
  }

  Bar::sReleaseCalled = 0;

  {
    Array2 arr2;

    for (PRInt32 i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar  = new Bar(arr2);
      arr2.AppendObject(bar);
    }

    base = Bar::sReleaseCalled;

    // Let arr2 be destroyed
  }
  if (Bar::sReleaseCalled != base + 20) {
    fail("Release called multiple times for nsCOMArray::~nsCOMArray");
  }

  return rv;
}
