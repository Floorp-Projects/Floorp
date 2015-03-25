/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "TestHarness.h"
#include "nsCOMArray.h"
#include "mozilla/Attributes.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

class IFoo : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(MozExternalRefCountType) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class Foo final : public IFoo {
  ~Foo();

public:

  explicit Foo(int32_t aID);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(MozExternalRefCountType) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return mID; }

  static int32_t gCount;

  int32_t mID;
};

int32_t Foo::gCount = 0;

Foo::Foo(int32_t aID)
{
  mID = aID;
  ++gCount;
}

Foo::~Foo()
{
  --gCount;
}

NS_IMPL_ISUPPORTS(Foo, IFoo)


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

class Bar final : public IBar {
public:

  explicit Bar(nsCOMArray<IBar>& aArray);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  static int32_t sReleaseCalled;

private:
  ~Bar();

  nsCOMArray<IBar>& mArray;
};

int32_t Bar::sReleaseCalled = 0;

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
NS_IMPL_QUERY_INTERFACE(Bar, IBar)

NS_IMETHODIMP_(MozExternalRefCountType)
Bar::Release(void)
{
  ++Bar::sReleaseCalled;
  MOZ_ASSERT(int32_t(mRefCnt) > 0, "dup release");
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

  for (int32_t i = 0; i < 20; ++i) {
    nsCOMPtr<IFoo> foo = new Foo(i);
    arr.AppendObject(foo);
  }

  if (arr.Count() != 20 || Foo::gCount != 20) {
    fail("nsCOMArray::AppendObject failed");
    rv = 1;
  }

  arr.TruncateLength(10);

  if (arr.Count() != 10 || Foo::gCount != 10) {
    fail("nsCOMArray::TruncateLength shortening of array failed");
    rv = 1;
  }

  arr.SetCount(30);

  if (arr.Count() != 30 || Foo::gCount != 10) {
    fail("nsCOMArray::SetCount lengthening of array failed");
    rv = 1;
  }

  for (int32_t i = 0; i < 10; ++i) {
	if (arr[i] == nullptr) {
      fail("nsCOMArray elements should be non-null");
      rv = 1;
	  break;
    }
  }

  for (int32_t i = 10; i < 30; ++i) {
	if (arr[i] != nullptr) {
      fail("nsCOMArray elements should be null");
      rv = 1;
	  break;
    }
  }

  int32_t base;
  {
    Array2 arr2;

    IBar *thirdObject = nullptr,
         *fourthObject = nullptr,
         *fifthObject = nullptr,
         *ninthObject = nullptr;
    for (int32_t i = 0; i < 20; ++i) {
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
    for (int32_t j = 0; j < arr2.Count(); ++j) {
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

  {
    Array2 arr2;

    IBar *thirdElement = nullptr,
         *fourthElement = nullptr,
         *fifthElement = nullptr,
         *ninthElement = nullptr;
    for (int32_t i = 0; i < 20; ++i) {
      nsCOMPtr<IBar> bar = new Bar(arr2);
      switch (i) {
      case 2:
        thirdElement = bar; break;
      case 3:
        fourthElement = bar; break;
      case 4:
        fifthElement = bar; break;
      case 8:
        ninthElement = bar; break;
      }
      arr2.AppendElement(bar);
    }

    base = Bar::sReleaseCalled;

    arr2.TruncateLength(10);
    if (Bar::sReleaseCalled != base + 10) {
      fail("Release called multiple times for TruncateLength");
    }
    if (arr2.Length() != 10) {
      fail("TruncateLength(10) should remove exactly ten objects");
    }

    arr2.RemoveElementAt(9);
    if (Bar::sReleaseCalled != base + 11) {
      fail("Release called multiple times for RemoveElementAt");
    }
    if (arr2.Length() != 9) {
      fail("RemoveElementAt should remove exactly one object");
    }

    arr2.RemoveElement(ninthElement);
    if (Bar::sReleaseCalled != base + 12) {
      fail("Release called multiple times for RemoveElement");
    }
    if (arr2.Length() != 8) {
      fail("RemoveElement should remove exactly one object");
    }

    arr2.RemoveElementsAt(2, 3);
    if (Bar::sReleaseCalled != base + 15) {
      fail("Release called more or less than three times for RemoveElementsAt");
    }
    if (arr2.Length() != 5) {
      fail("RemoveElementsAt should remove exactly three objects");
    }
    for (uint32_t j = 0; j < arr2.Length(); ++j) {
      if (arr2.ElementAt(j) == thirdElement) {
        fail("RemoveElementsAt should have removed thirdElement");
      }
      if (arr2.ElementAt(j) == fourthElement) {
        fail("RemoveElementsAt should have removed fourthElement");
      }
      if (arr2.ElementAt(j) == fifthElement) {
        fail("RemoveElementsAt should have removed fifthElement");
      }
    }

    arr2.RemoveElementsAt(4, 1);
    if (Bar::sReleaseCalled != base + 16) {
      fail("Release called more or less than one time for RemoveElementsAt");
    }
    if (arr2.Length() != 4) {
      fail("RemoveElementsAt should work for removing the last element");
    }

    arr2.Clear();
    if (Bar::sReleaseCalled != base + 20) {
      fail("Release called multiple times for Clear");
    }
  }

  Bar::sReleaseCalled = 0;

  {
    Array2 arr2;

    for (int32_t i = 0; i < 20; ++i) {
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
