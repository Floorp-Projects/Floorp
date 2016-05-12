/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/NotNull.h"
#include "mozilla/RefPtr.h"
#include "mozilla/UniquePtr.h"

using mozilla::WrapNotNull;
using mozilla::MakeUnique;
using mozilla::NotNull;
using mozilla::UniquePtr;

#define CHECK MOZ_RELEASE_ASSERT

class Blah
{
public:
  Blah() : mX(0) {}
  void blah() {};
  int mX;
};

// A simple smart pointer that implicity converts to and from T*.
template <typename T>
class MyPtr
{
  T* mRawPtr;

public:
  MyPtr() : mRawPtr(nullptr) {}
  MOZ_IMPLICIT MyPtr(T* aRawPtr) : mRawPtr(aRawPtr) {}

  T* get() const { return mRawPtr; }
  operator T*() const { return get(); }

  T* operator->() const { return get(); }
};

// A simple class that works with RefPtr. It keeps track of the maximum
// refcount value for testing purposes.
class MyRefType
{
  int mExpectedMaxRefCnt;
  int mMaxRefCnt;
  int mRefCnt;
public:
  explicit MyRefType(int aExpectedMaxRefCnt)
    : mExpectedMaxRefCnt(aExpectedMaxRefCnt)
    , mMaxRefCnt(0)
    , mRefCnt(0)
  {}

  ~MyRefType() {
    CHECK(mMaxRefCnt == mExpectedMaxRefCnt);
  }

  uint32_t AddRef() {
    mRefCnt++;
    if (mRefCnt > mMaxRefCnt) {
      mMaxRefCnt = mRefCnt;
    }
    return mRefCnt;
  }

  uint32_t Release() {
    CHECK(mRefCnt > 0);
    if (mRefCnt == 1) {
      delete this;
    }
    mRefCnt--;
    return mRefCnt;
  }
};

void f_i(int* aPtr) {}
void f_my(MyPtr<int> aPtr) {}

void f_nni(NotNull<int*> aPtr) {}
void f_nnmy(NotNull<MyPtr<int>> aPtr) {}

void
TestNotNullWithMyPtr()
{
  int i4 = 4;
  int i5 = 5;

  MyPtr<int> my4 = &i4;
  MyPtr<int> my5 = &i5;

  NotNull<int*> nni4 = WrapNotNull(&i4);
  NotNull<int*> nni5 = WrapNotNull(&i5);
  NotNull<MyPtr<int>> nnmy4 = WrapNotNull(my4);

  //WrapNotNull(nullptr);                       // no wrapping from nullptr
  //WrapNotNull(0);                             // no wrapping from zero

  // NotNull<int*> construction combinations
  //NotNull<int*> nni4a;                        // no default
  //NotNull<int*> nni4a(nullptr);               // no nullptr
  //NotNull<int*> nni4a(0);                     // no zero
  //NotNull<int*> nni4a(&i4);                   // no int*
  //NotNull<int*> nni4a(my4);                   // no MyPtr<int>
  NotNull<int*> nni4b(WrapNotNull(&i4));        // WrapNotNull(int*)
  NotNull<int*> nni4c(WrapNotNull(my4));        // WrapNotNull(MyPtr<int>)
  NotNull<int*> nni4d(nni4);                    // NotNull<int*>
  NotNull<int*> nni4e(nnmy4);                   // NotNull<MyPtr<int>>
  CHECK(*nni4b == 4);
  CHECK(*nni4c == 4);
  CHECK(*nni4d == 4);
  CHECK(*nni4e == 4);

  // NotNull<MyPtr<int>> construction combinations
  //NotNull<MyPtr<int>> nnmy4a;                 // no default
  //NotNull<MyPtr<int>> nnmy4a(nullptr);        // no nullptr
  //NotNull<MyPtr<int>> nnmy4a(0);              // no zero
  //NotNull<MyPtr<int>> nnmy4a(&i4);            // no int*
  //NotNull<MyPtr<int>> nnmy4a(my4);            // no MyPtr<int>
  NotNull<MyPtr<int>> nnmy4b(WrapNotNull(&i4)); // WrapNotNull(int*)
  NotNull<MyPtr<int>> nnmy4c(WrapNotNull(my4)); // WrapNotNull(MyPtr<int>)
  NotNull<MyPtr<int>> nnmy4d(nni4);             // NotNull<int*>
  NotNull<MyPtr<int>> nnmy4e(nnmy4);            // NotNull<MyPtr<int>>
  CHECK(*nnmy4b == 4);
  CHECK(*nnmy4c == 4);
  CHECK(*nnmy4d == 4);
  CHECK(*nnmy4e == 4);

  // NotNull<int*> assignment combinations
  //nni4b = nullptr;                            // no nullptr
  //nni4b = 0;                                  // no zero
  //nni4a = &i4;                                // no int*
  //nni4a = my4;                                // no MyPtr<int>
  nni4b = WrapNotNull(&i4);                     // WrapNotNull(int*)
  nni4c = WrapNotNull(my4);                     // WrapNotNull(MyPtr<int>)
  nni4d = nni4;                                 // NotNull<int*>
  nni4e = nnmy4;                                // NotNull<MyPtr<int>>
  CHECK(*nni4b == 4);
  CHECK(*nni4c == 4);
  CHECK(*nni4d == 4);
  CHECK(*nni4e == 4);

  // NotNull<MyPtr<int>> assignment combinations
  //nnmy4a = nullptr;                           // no nullptr
  //nnmy4a = 0;                                 // no zero
  //nnmy4a = &i4;                               // no int*
  //nnmy4a = my4;                               // no MyPtr<int>
  nnmy4b = WrapNotNull(&i4);                    // WrapNotNull(int*)
  nnmy4c = WrapNotNull(my4);                    // WrapNotNull(MyPtr<int>)
  nnmy4d = nni4;                                // NotNull<int*>
  nnmy4e = nnmy4;                               // NotNull<MyPtr<int>>
  CHECK(*nnmy4b == 4);
  CHECK(*nnmy4c == 4);
  CHECK(*nnmy4d == 4);
  CHECK(*nnmy4e == 4);

  NotNull<MyPtr<int>> nnmy5 = WrapNotNull(&i5);
  CHECK(*nnmy5 == 5);
  CHECK(nnmy5 == &i5);       // NotNull<MyPtr<int>> == int*
  CHECK(nnmy5 == my5);       // NotNull<MyPtr<int>> == MyPtr<int>
  CHECK(nnmy5 == nni5);      // NotNull<MyPtr<int>> == NotNull<int*>
  CHECK(nnmy5 == nnmy5);     // NotNull<MyPtr<int>> == NotNull<MyPtr<int>>
  CHECK(&i5   == nnmy5);     // int*                == NotNull<MyPtr<int>>
  CHECK(my5   == nnmy5);     // MyPtr<int>          == NotNull<MyPtr<int>>
  CHECK(nni5  == nnmy5);     // NotNull<int*>       == NotNull<MyPtr<int>>
  CHECK(nnmy5 == nnmy5);     // NotNull<MyPtr<int>> == NotNull<MyPtr<int>>
  //CHECK(nni5 == nullptr);  // no comparisons with nullptr
  //CHECK(nullptr == nni5);  // no comparisons with nullptr
  //CHECK(nni5 == 0);        // no comparisons with zero
  //CHECK(0 == nni5);        // no comparisons with zero

  CHECK(*nnmy5 == 5);
  CHECK(nnmy5 != &i4);       // NotNull<MyPtr<int>> != int*
  CHECK(nnmy5 != my4);       // NotNull<MyPtr<int>> != MyPtr<int>
  CHECK(nnmy5 != nni4);      // NotNull<MyPtr<int>> != NotNull<int*>
  CHECK(nnmy5 != nnmy4);     // NotNull<MyPtr<int>> != NotNull<MyPtr<int>>
  CHECK(&i4   != nnmy5);     // int*                != NotNull<MyPtr<int>>
  CHECK(my4   != nnmy5);     // MyPtr<int>          != NotNull<MyPtr<int>>
  CHECK(nni4  != nnmy5);     // NotNull<int*>       != NotNull<MyPtr<int>>
  CHECK(nnmy4 != nnmy5);     // NotNull<MyPtr<int>> != NotNull<MyPtr<int>>
  //CHECK(nni4 != nullptr);  // no comparisons with nullptr
  //CHECK(nullptr != nni4);  // no comparisons with nullptr
  //CHECK(nni4 != 0);        // no comparisons with zero
  //CHECK(0 != nni4);        // no comparisons with zero

  // int* parameter
  f_i(&i4);             // identity int*                        --> int*
  f_i(my4);             // implicit MyPtr<int>                  --> int*
  f_i(my4.get());       // explicit MyPtr<int>                  --> int*
  f_i(nni4);            // implicit NotNull<int*>               --> int*
  f_i(nni4.get());      // explicit NotNull<int*>               --> int*
  //f_i(nnmy4);         // no implicit NotNull<MyPtr<int>>      --> int*
  f_i(nnmy4.get());     // explicit NotNull<MyPtr<int>>         --> int*
  f_i(nnmy4.get().get());// doubly-explicit NotNull<MyPtr<int>> --> int*

  // MyPtr<int> parameter
  f_my(&i4);            // implicit int*                         --> MyPtr<int>
  f_my(my4);            // identity MyPtr<int>                   --> MyPtr<int>
  f_my(my4.get());      // explicit MyPtr<int>                   --> MyPtr<int>
  //f_my(nni4);         // no implicit NotNull<int*>             --> MyPtr<int>
  f_my(nni4.get());     // explicit NotNull<int*>                --> MyPtr<int>
  f_my(nnmy4);          // implicit NotNull<MyPtr<int>>          --> MyPtr<int>
  f_my(nnmy4.get());    // explicit NotNull<MyPtr<int>>          --> MyPtr<int>
  f_my(nnmy4.get().get());// doubly-explicit NotNull<MyPtr<int>> --> MyPtr<int>

  // NotNull<int*> parameter
  f_nni(nni4);          // identity NotNull<int*>       --> NotNull<int*>
  f_nni(nnmy4);         // implicit NotNull<MyPtr<int>> --> NotNull<int*>

  // NotNull<MyPtr<int>> parameter
  f_nnmy(nni4);         // implicit NotNull<int*>       --> NotNull<MyPtr<int>>
  f_nnmy(nnmy4);        // identity NotNull<MyPtr<int>> --> NotNull<MyPtr<int>>

  //CHECK(nni4);        // disallow boolean conversion / unary expression usage
  //CHECK(nnmy4);       // ditto

  // '->' dereferencing.
  Blah blah;
  MyPtr<Blah> myblah = &blah;
  NotNull<Blah*> nnblah = WrapNotNull(&blah);
  NotNull<MyPtr<Blah>> nnmyblah = WrapNotNull(myblah);
  (&blah)->blah();          // int*
  myblah->blah();           // MyPtr<int>
  nnblah->blah();           // NotNull<int*>
  nnmyblah->blah();         // NotNull<MyPtr<int>>

  (&blah)->mX = 1;
  CHECK((&blah)->mX == 1);
  myblah->mX = 2;
  CHECK(myblah->mX == 2);
  nnblah->mX = 3;
  CHECK(nnblah->mX == 3);
  nnmyblah->mX = 4;
  CHECK(nnmyblah->mX == 4);

  // '*' dereferencing (lvalues and rvalues)
  *(&i4) = 7;               // int*
  CHECK(*(&i4) == 7);
  *my4 = 6;                 // MyPtr<int>
  CHECK(*my4 == 6);
  *nni4 = 5;                // NotNull<int*>
  CHECK(*nni4 == 5);
  *nnmy4 = 4;               // NotNull<MyPtr<int>>
  CHECK(*nnmy4 == 4);

  // Non-null arrays.
  static const int N = 20;
  int a[N];
  NotNull<int*> nna = WrapNotNull(a);
  for (int i = 0; i < N; i++) {
    nna[i] = i;
  }
  for (int i = 0; i < N; i++) {
    nna[i] *= 2;
  }
  for (int i = 0; i < N; i++) {
    CHECK(nna[i] == i * 2);
  }
}

void f_ref(NotNull<MyRefType*> aR)
{
  NotNull<RefPtr<MyRefType>> r = aR;
}

void
TestNotNullWithRefPtr()
{
  // This MyRefType object will have a maximum refcount of 5.
  NotNull<RefPtr<MyRefType>> r1 = WrapNotNull(new MyRefType(5));

  // At this point the refcount is 1.

  NotNull<RefPtr<MyRefType>> r2 = r1;

  // At this point the refcount is 2.

  NotNull<MyRefType*> r3 = r2;
  (void)r3;

  // At this point the refcount is still 2.

  RefPtr<MyRefType> r4 = r2;

  // At this point the refcount is 3.

  RefPtr<MyRefType> r5 = r3.get();

  // At this point the refcount is 4.

  // No change to the refcount occurs because of the argument passing. Within
  // f_ref() the refcount temporarily hits 5, due to the local RefPtr.
  f_ref(r2);

  // At this point the refcount is 4.

  // At function's end all RefPtrs are destroyed and the refcount drops to 0
  // and the MyRefType is destroyed.
}

int
main()
{
  TestNotNullWithMyPtr();
  TestNotNullWithRefPtr();

  return 0;
}

