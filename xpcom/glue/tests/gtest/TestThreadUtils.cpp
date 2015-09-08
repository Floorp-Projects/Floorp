/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsThreadUtils.h"
#include "gtest/gtest.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

namespace TestThreadUtils {

static bool gDebug = false;
static int gAlive, gZombies;
static int gAllConstructions, gConstructions, gCopyConstructions,
           gMoveConstructions, gDestructions, gAssignments, gMoves;
struct Spy
{
  static void ClearActions()
  {
    gAllConstructions = gConstructions = gCopyConstructions
        = gMoveConstructions = gDestructions = gAssignments = gMoves = 0;
  }
  static void ClearAll()
  {
    ClearActions();
    gAlive = 0;
  }

  explicit Spy(int aID) : mID(aID)
  {
    ++gAlive; ++gAllConstructions; ++gConstructions;
    if (gDebug) { printf("Spy[%d@%p]()\n", mID, this); }
  }
  Spy(const Spy& o) : mID(o.mID)
  {
    ++gAlive; ++gAllConstructions; ++gCopyConstructions;
    if (gDebug) { printf("Spy[%d@%p](&[%d@%p])\n", mID, this, o.mID, &o); }
  }
  Spy(Spy&& o) : mID(o.mID)
  {
    o.mID = -o.mID;
    ++gZombies; ++gAllConstructions; ++gMoveConstructions;
    if (gDebug) { printf("Spy[%d@%p](&&[%d->%d@%p])\n", mID, this, -o.mID, o.mID, &o); }
  }
  ~Spy()
  {
    if (mID >= 0) { --gAlive; } else { --gZombies; } ++gDestructions;
    if (gDebug) { printf("~Spy[%d@%p]()\n", mID, this); }
    mID = 0;
  }
  Spy& operator=(const Spy& o)
  {
    ++gAssignments;
    if (gDebug) { printf("Spy[%d->%d@%p] = &[%d@%p]\n", mID, o.mID, this, o.mID, &o); }
    mID = o.mID;
    return *this;
  };
  Spy& operator=(Spy&& o)
  {
    --gAlive; ++gZombies;
    ++gMoves;
    if (gDebug) { printf("Spy[%d->%d@%p] = &&[%d->%d@%p]\n", mID, o.mID, this, o.mID, -o.mID, &o); }
    mID = o.mID; o.mID = -o.mID;
    return *this;
  };

  int mID; // ID given at construction, or negation if was moved from; 0 when destroyed.
};

struct ISpyWithISupports : public nsISupports
{
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)
  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};
NS_DEFINE_STATIC_IID_ACCESSOR(ISpyWithISupports, NS_IFOO_IID)
struct SpyWithISupports : public ISpyWithISupports, public Spy
{
private:
  virtual ~SpyWithISupports() = default;
public:
  explicit SpyWithISupports(int aID) : Spy(aID) {};
  NS_DECL_ISUPPORTS
  NS_IMETHOD_(nsrefcnt) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return mID; }
};
NS_IMPL_ISUPPORTS(SpyWithISupports, ISpyWithISupports)


class IThreadUtilsObject : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IThreadUtilsObject, NS_IFOO_IID)

struct ThreadUtilsObject : public IThreadUtilsObject
{
  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IThreadUtilsObject implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() override { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() override { return 0; }

  int mCount; // Number of calls + arguments processed.
  int mA0, mA1, mA2, mA3;
  Spy mSpy; const Spy* mSpyPtr;
  ThreadUtilsObject()
    : mCount(0)
    , mA0(0), mA1(0), mA2(0), mA3(0)
    , mSpy(1), mSpyPtr(nullptr)
  {}
private:
  virtual ~ThreadUtilsObject() = default;
public:
  void Test0() { mCount += 1; }
  void Test1i(int a0) { mCount += 2; mA0 = a0; }
  void Test2i(int a0, int a1) { mCount += 3; mA0 = a0; mA1 = a1; }
  void Test3i(int a0, int a1, int a2)
  {
    mCount += 4; mA0 = a0; mA1 = a1; mA2 = a2;
  }
  void Test4i(int a0, int a1, int a2, int a3)
  {
    mCount += 5; mA0 = a0; mA1 = a1; mA2 = a2; mA3 = a3;
  }
  void Test1pi(int* ap)
  {
    mCount += 2; mA0 = ap ? *ap : -1;
  }
  void Test1pci(const int* ap)
  {
    mCount += 2; mA0 = ap ? *ap : -1;
  }
  void Test1ri(int& ar)
  {
    mCount += 2; mA0 = ar;
  }
  void Test1rri(int&& arr)
  {
    mCount += 2; mA0 = arr;
  }
  void Test1upi(mozilla::UniquePtr<int> aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }
  void Test1rupi(mozilla::UniquePtr<int>& aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }
  void Test1rrupi(mozilla::UniquePtr<int>&& aup)
  {
    mCount += 2; mA0 = aup ? *aup : -1;
  }

  void Test1s(Spy) { mCount += 2; }
  void Test1ps(Spy*) { mCount += 2; }
  void Test1rs(Spy&) { mCount += 2; }
  void Test1rrs(Spy&&) { mCount += 2; }
  void Test1ups(mozilla::UniquePtr<Spy>) { mCount += 2; }
  void Test1rups(mozilla::UniquePtr<Spy>&) { mCount += 2; }
  void Test1rrups(mozilla::UniquePtr<Spy>&&) { mCount += 2; }

  // Possible parameter passing styles:
  void TestByValue(Spy s)
  {
    if (gDebug) { printf("TestByValue(Spy[%d@%p])\n", s.mID, &s); }
    mSpy = s;
  };
  void TestByConstLRef(const Spy& s)
  {
    if (gDebug) { printf("TestByConstLRef(Spy[%d@%p]&)\n", s.mID, &s); }
    mSpy = s;
  };
  void TestByRRef(Spy&& s)
  {
    if (gDebug) { printf("TestByRRef(Spy[%d@%p]&&)\n", s.mID, &s); }
    mSpy = mozilla::Move(s);
  };
  void TestByLRef(Spy& s)
  {
    if (gDebug) { printf("TestByLRef(Spy[%d@%p]&)\n", s.mID, &s); }
    mSpy = s;
    mSpyPtr = &s;
  };
  void TestByPointer(Spy* p)
  {
    if (p) {
      if (gDebug) { printf("TestByPointer(&Spy[%d@%p])\n", p->mID, p); }
      mSpy = *p;
    } else {
      if (gDebug) { printf("TestByPointer(nullptr)\n"); }
    }
    mSpyPtr = p;
  };
  void TestByPointerToConst(const Spy* p)
  {
    if (p) {
      if (gDebug) { printf("TestByPointerToConst(&Spy[%d@%p])\n", p->mID, p); }
      mSpy = *p;
    } else {
      if (gDebug) { printf("TestByPointerToConst(nullptr)\n"); }
    }
    mSpyPtr = p;
  };
};

NS_IMPL_ISUPPORTS(ThreadUtilsObject, IThreadUtilsObject)

class ThreadUtilsRefCountedFinal final
{
public:
  ThreadUtilsRefCountedFinal() : m_refCount(0) {}
  ~ThreadUtilsRefCountedFinal() {}
  // 'AddRef' and 'Release' methods with different return types, to verify
  // that the return type doesn't influence storage selection.
  long AddRef(void) { return ++m_refCount; }
  void Release(void) { --m_refCount; }
private:
  long m_refCount;
};

class ThreadUtilsRefCountedBase
{
public:
  ThreadUtilsRefCountedBase() : m_refCount(0) {}
  virtual ~ThreadUtilsRefCountedBase() {}
  // 'AddRef' and 'Release' methods with different return types, to verify
  // that the return type doesn't influence storage selection.
  virtual void AddRef(void) { ++m_refCount; }
  virtual MozExternalRefCountType Release(void) { return --m_refCount; }
private:
  MozExternalRefCountType m_refCount;
};

class ThreadUtilsRefCountedDerived
  : public ThreadUtilsRefCountedBase
{};

class ThreadUtilsNonRefCounted
{};

} // namespace TestThreadUtils

TEST(ThreadUtils, main)
{
#ifndef XPCOM_GLUE_AVOID_NSPR
  using namespace TestThreadUtils;

  static_assert(!IsParameterStorageClass<int>::value,
                "'int' should not be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByValue<int>>::value,
                "StoreCopyPassByValue<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByConstLRef<int>>::value,
                "StoreCopyPassByConstLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByLRef<int>>::value,
                "StoreCopyPassByLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByRRef<int>>::value,
                "StoreCopyPassByRRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreRefPassByLRef<int>>::value,
                "StoreRefPassByLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreConstRefPassByConstLRef<int>>::value,
                "StoreConstRefPassByConstLRef<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StorensRefPtrPassByPtr<int>>::value,
                "StorensRefPtrPassByPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StorePtrPassByPtr<int>>::value,
                "StorePtrPassByPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreConstPtrPassByConstPtr<int>>::value,
                "StoreConstPtrPassByConstPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByConstPtr<int>>::value,
                "StoreCopyPassByConstPtr<int> should be recognized as Storage Class");
  static_assert(IsParameterStorageClass<StoreCopyPassByPtr<int>>::value,
                "StoreCopyPassByPtr<int> should be recognized as Storage Class");

  nsRefPtr<ThreadUtilsObject> rpt(new ThreadUtilsObject);
  int count = 0;

  // Test legacy functions.

  nsCOMPtr<nsIRunnable> r1 =
    NS_NewRunnableMethod(rpt, &ThreadUtilsObject::Test0);
  r1->Run();
  EXPECT_EQ(count += 1, rpt->mCount);

  r1 = NS_NewRunnableMethodWithArg<int>(rpt, &ThreadUtilsObject::Test1i, 11);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(11, rpt->mA0);

  // Test variadic function with simple POD arguments.

  r1 = NS_NewRunnableMethodWithArgs(rpt, &ThreadUtilsObject::Test0);
  r1->Run();
  EXPECT_EQ(count += 1, rpt->mCount);

  static_assert(
      mozilla::IsSame< ::detail::ParameterStorage<int>::Type,
                      StoreCopyPassByValue<int>>::value,
      "detail::ParameterStorage<int>::Type should be StoreCopyPassByValue<int>");
  static_assert(
      mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByValue<int>>::Type,
                      StoreCopyPassByValue<int>>::value,
      "detail::ParameterStorage<StoreCopyPassByValue<int>>::Type should be StoreCopyPassByValue<int>");

  r1 = NS_NewRunnableMethodWithArgs<int>(rpt, &ThreadUtilsObject::Test1i, 12);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(12, rpt->mA0);

  r1 = NS_NewRunnableMethodWithArgs<int, int>(
       rpt, &ThreadUtilsObject::Test2i, 21, 22);
  r1->Run();
  EXPECT_EQ(count += 3, rpt->mCount);
  EXPECT_EQ(21, rpt->mA0);
  EXPECT_EQ(22, rpt->mA1);

  r1 = NS_NewRunnableMethodWithArgs<int, int, int>(
       rpt, &ThreadUtilsObject::Test3i, 31, 32, 33);
  r1->Run();
  EXPECT_EQ(count += 4, rpt->mCount);
  EXPECT_EQ(31, rpt->mA0);
  EXPECT_EQ(32, rpt->mA1);
  EXPECT_EQ(33, rpt->mA2);

  r1 = NS_NewRunnableMethodWithArgs<int, int, int, int>(
       rpt, &ThreadUtilsObject::Test4i, 41, 42, 43, 44);
  r1->Run();
  EXPECT_EQ(count += 5, rpt->mCount);
  EXPECT_EQ(41, rpt->mA0);
  EXPECT_EQ(42, rpt->mA1);
  EXPECT_EQ(43, rpt->mA2);
  EXPECT_EQ(44, rpt->mA3);

  // More interesting types of arguments.

  // Passing a short to make sure forwarding works with an inexact type match.
  short int si = 11;
  r1 = NS_NewRunnableMethodWithArgs<int>(rpt, &ThreadUtilsObject::Test1i, si);
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(si, rpt->mA0);

  // Raw pointer, possible cv-qualified.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int*>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* const>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* const>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* volatile>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* volatile>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int* const volatile>::Type,
                                StorePtrPassByPtr<int>>::value,
                "detail::ParameterStorage<int* const volatile>::Type should be StorePtrPassByPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type::stored_type,
                                int*>::value,
                "detail::ParameterStorage<int*>::Type::stored_type should be int*");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int*>::Type::passed_type,
                                int*>::value,
                "detail::ParameterStorage<int*>::Type::passed_type should be int*");
  {
    int i = 12;
    r1 = NS_NewRunnableMethodWithArgs<int*>(rpt, &ThreadUtilsObject::Test1pi, &i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to const.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int*>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* const>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* const>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* volatile>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* volatile>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int* const volatile>::Type,
                                StoreConstPtrPassByConstPtr<int>>::value,
                "detail::ParameterStorage<const int* const volatile>::Type should be StoreConstPtrPassByConstPtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type::stored_type,
                                const int*>::value,
                "detail::ParameterStorage<const int*>::Type::stored_type should be const int*");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<const int*>::Type::passed_type,
                                const int*>::value,
                "detail::ParameterStorage<const int*>::Type::passed_type should be const int*");
  {
    int i = 1201;
    r1 = NS_NewRunnableMethodWithArgs<const int*>(rpt, &ThreadUtilsObject::Test1pci, &i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to copy.
  static_assert(mozilla::IsSame<StoreCopyPassByPtr<int>::stored_type,
                                int>::value,
                "StoreCopyPassByPtr<int>::stored_type should be int");
  static_assert(mozilla::IsSame<StoreCopyPassByPtr<int>::passed_type,
                                int*>::value,
                "StoreCopyPassByPtr<int>::passed_type should be int*");
  {
    int i = 1202;
    r1 = NS_NewRunnableMethodWithArgs<StoreCopyPassByPtr<int>>(
         rpt, &ThreadUtilsObject::Test1pi, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Raw pointer to const copy.
  static_assert(mozilla::IsSame<StoreCopyPassByConstPtr<int>::stored_type,
                                int>::value,
                "StoreCopyPassByConstPtr<int>::stored_type should be int");
  static_assert(mozilla::IsSame<StoreCopyPassByConstPtr<int>::passed_type,
                                const int*>::value,
                "StoreCopyPassByConstPtr<int>::passed_type should be const int*");
  {
    int i = 1203;
    r1 = NS_NewRunnableMethodWithArgs<StoreCopyPassByConstPtr<int>>(
         rpt, &ThreadUtilsObject::Test1pci, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // nsRefPtr to pointer.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StorensRefPtrPassByPtr<SpyWithISupports>>::Type,
                                StorensRefPtrPassByPtr<SpyWithISupports>>::value,
                "ParameterStorage<StorensRefPtrPassByPtr<SpyWithISupports>>::Type should be StorensRefPtrPassByPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<SpyWithISupports*>::Type,
                                StorensRefPtrPassByPtr<SpyWithISupports>>::value,
                "ParameterStorage<SpyWithISupports*>::Type should be StorensRefPtrPassByPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame<StorensRefPtrPassByPtr<SpyWithISupports>::stored_type,
                                nsRefPtr<SpyWithISupports>>::value,
                "StorensRefPtrPassByPtr<SpyWithISupports>::stored_type should be nsRefPtr<SpyWithISupports>");
  static_assert(mozilla::IsSame<StorensRefPtrPassByPtr<SpyWithISupports>::passed_type,
                                SpyWithISupports*>::value,
                "StorensRefPtrPassByPtr<SpyWithISupports>::passed_type should be SpyWithISupports*");
  // (more nsRefPtr tests below)

  // nsRefPtr for ref-countable classes that do not derive from ISupports.
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedFinal>::value,
                "ThreadUtilsRefCountedFinal has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedFinal*>::Type,
                                StorensRefPtrPassByPtr<ThreadUtilsRefCountedFinal>>::value,
                "ParameterStorage<ThreadUtilsRefCountedFinal*>::Type should be StorensRefPtrPassByPtr<ThreadUtilsRefCountedFinal>");
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedBase>::value,
                "ThreadUtilsRefCountedBase has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedBase*>::Type,
                                StorensRefPtrPassByPtr<ThreadUtilsRefCountedBase>>::value,
                "ParameterStorage<ThreadUtilsRefCountedBase*>::Type should be StorensRefPtrPassByPtr<ThreadUtilsRefCountedBase>");
  static_assert(::detail::HasRefCountMethods<ThreadUtilsRefCountedDerived>::value,
                "ThreadUtilsRefCountedDerived has AddRef() and Release()");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsRefCountedDerived*>::Type,
                                StorensRefPtrPassByPtr<ThreadUtilsRefCountedDerived>>::value,
                "ParameterStorage<ThreadUtilsRefCountedDerived*>::Type should be StorensRefPtrPassByPtr<ThreadUtilsRefCountedDerived>");

  static_assert(!::detail::HasRefCountMethods<ThreadUtilsNonRefCounted>::value,
                "ThreadUtilsNonRefCounted doesn't have AddRef() and Release()");
  static_assert(!mozilla::IsSame< ::detail::ParameterStorage<ThreadUtilsNonRefCounted*>::Type,
                                 StorensRefPtrPassByPtr<ThreadUtilsNonRefCounted>>::value,
                "ParameterStorage<ThreadUtilsNonRefCounted*>::Type should NOT be StorensRefPtrPassByPtr<ThreadUtilsNonRefCounted>");

  // Lvalue reference.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type,
                                StoreRefPassByLRef<int>>::value,
                "ParameterStorage<int&>::Type should be StoreRefPassByLRef<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::stored_type,
                                StoreRefPassByLRef<int>::stored_type>::value,
                "ParameterStorage<int&>::Type::stored_type should be StoreRefPassByLRef<int>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::stored_type,
                                int&>::value,
                "ParameterStorage<int&>::Type::stored_type should be int&");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&>::Type::passed_type,
                                int&>::value,
                "ParameterStorage<int&>::Type::passed_type should be int&");
  {
    int i = 13;
    r1 = NS_NewRunnableMethodWithArgs<int&>(rpt, &ThreadUtilsObject::Test1ri, i);
    r1->Run();
    EXPECT_EQ(count += 2, rpt->mCount);
    EXPECT_EQ(i, rpt->mA0);
  }

  // Rvalue reference -- Actually storing a copy and then moving it.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type,
                                StoreCopyPassByRRef<int>>::value,
                "ParameterStorage<int&&>::Type should be StoreCopyPassByRRef<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::stored_type,
                                StoreCopyPassByRRef<int>::stored_type>::value,
                "ParameterStorage<int&&>::Type::stored_type should be StoreCopyPassByRRef<int>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::stored_type,
                                int>::value,
                "ParameterStorage<int&&>::Type::stored_type should be int");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<int&&>::Type::passed_type,
                                int&&>::value,
                "ParameterStorage<int&&>::Type::passed_type should be int&&");
  {
    int i = 14;
    r1 = NS_NewRunnableMethodWithArgs<int&&>(
          rpt, &ThreadUtilsObject::Test1rri, mozilla::Move(i));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(14, rpt->mA0);

  // Null unique pointer, by semi-implicit store&move with "T&&" syntax.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type should be StoreCopyPassByRRef<UniquePtr<int>>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::stored_type,
                                mozilla::UniquePtr<int>>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::stored_type should be UniquePtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<mozilla::UniquePtr<int>&&>::Type::passed_type,
                                mozilla::UniquePtr<int>&&>::value,
                "ParameterStorage<UniquePtr<int>&&>::Type::passed_type should be UniquePtr<int>&&");
  {
    mozilla::UniquePtr<int> upi;
    r1 = NS_NewRunnableMethodWithArgs<mozilla::UniquePtr<int>&&>(
          rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);
  rpt->mA0 = 0;

  // Null unique pointer, by explicit store&move with "StoreCopyPassByRRef<T>" syntax.
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                StoreCopyPassByRRef<mozilla::UniquePtr<int>>::stored_type>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be StoreCopyPassByRRef<UniquePtr<int>>::stored_type");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::stored_type,
                                mozilla::UniquePtr<int>>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::stored_type should be UniquePtr<int>");
  static_assert(mozilla::IsSame< ::detail::ParameterStorage<StoreCopyPassByRRef<mozilla::UniquePtr<int>>>::Type::passed_type,
                                mozilla::UniquePtr<int>&&>::value,
                "ParameterStorage<StoreCopyPassByRRef<UniquePtr<int>>>::Type::passed_type should be UniquePtr<int>&&");
  {
    mozilla::UniquePtr<int> upi;
    r1 = NS_NewRunnableMethodWithArgs
         <StoreCopyPassByRRef<mozilla::UniquePtr<int>>>(
           rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);

  // Unique pointer as xvalue.
  {
    mozilla::UniquePtr<int> upi = mozilla::MakeUnique<int>(1);
    r1 = NS_NewRunnableMethodWithArgs<mozilla::UniquePtr<int>&&>(
           rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(1, rpt->mA0);

  {
    mozilla::UniquePtr<int> upi = mozilla::MakeUnique<int>(1);
    r1 = NS_NewRunnableMethodWithArgs
         <StoreCopyPassByRRef<mozilla::UniquePtr<int>>>
         (rpt, &ThreadUtilsObject::Test1upi, mozilla::Move(upi));
  }
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(1, rpt->mA0);

  // Unique pointer as prvalue.
  r1 = NS_NewRunnableMethodWithArgs<mozilla::UniquePtr<int>&&>(
         rpt, &ThreadUtilsObject::Test1upi, mozilla::MakeUnique<int>(2));
  r1->Run();
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(2, rpt->mA0);

  // Unique pointer as lvalue to lref.
  {
    mozilla::UniquePtr<int> upi;
    r1 = NS_NewRunnableMethodWithArgs<mozilla::UniquePtr<int>&>(
           rpt, &ThreadUtilsObject::Test1rupi, upi);
    // Passed as lref, so Run() must be called while local upi is still alive!
    r1->Run();
  }
  EXPECT_EQ(count += 2, rpt->mCount);
  EXPECT_EQ(-1, rpt->mA0);

  // Verify copy/move assumptions.

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by value\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r2;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(10)\n", __LINE__); }
      Spy s(10);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r2 = NS_NewRunnableMethodWithArgs<StoreCopyPassByValue<Spy>>(&TestByValue, s)\n", __LINE__); }
      r2 = NS_NewRunnableMethodWithArgs<StoreCopyPassByValue<Spy>>(
           rpt, &ThreadUtilsObject::TestByValue, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(10)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r2->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(10, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by value\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r3 = NS_NewRunnableMethodWithArgs<StoreCopyPassByValue<Spy>>(&TestByValue, Spy(11))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r3 =
      NS_NewRunnableMethodWithArgs<StoreCopyPassByValue<Spy>>(
        rpt, &ThreadUtilsObject::TestByValue, Spy(11));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r3->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(11, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  { // Store copy from xvalue, pass by value.
    nsCOMPtr<nsIRunnable> r4;
    {
      Spy s(12);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      Spy::ClearActions();
      r4 = NS_NewRunnableMethodWithArgs<StoreCopyPassByValue<Spy>>(
          rpt, &ThreadUtilsObject::TestByValue, mozilla::Move(s));
      EXPECT_LE(1, gMoveConstructions);
      EXPECT_EQ(1, gAlive);
      EXPECT_EQ(1, gZombies);
      Spy::ClearActions();
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(0, gZombies);
    Spy::ClearActions();
    r4->Run();
    EXPECT_LE(1, gCopyConstructions); // Another copy-construction in call.
    EXPECT_EQ(12, rpt->mSpy.mID);
    EXPECT_LE(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
  }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);
  // Won't test xvalues anymore, prvalues are enough to verify all rvalues.

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by const lvalue ref\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r5;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(20)\n", __LINE__); }
      Spy s(20);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r5 = NS_NewRunnableMethodWithArgs<StoreCopyPassByConstLRef<Spy>>(&TestByConstLRef, s)\n", __LINE__); }
      r5 = NS_NewRunnableMethodWithArgs<StoreCopyPassByConstLRef<Spy>>(
           rpt, &ThreadUtilsObject::TestByConstLRef, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(20)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r5->Run();
    EXPECT_EQ(0, gCopyConstructions); // No copies in call.
    EXPECT_EQ(20, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by const lvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r6 = NS_NewRunnableMethodWithArgs<StoreCopyPassByConstLRef<Spy>>(&TestByConstLRef, Spy(21))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r6 =
      NS_NewRunnableMethodWithArgs<StoreCopyPassByConstLRef<Spy>>(
        rpt, &ThreadUtilsObject::TestByConstLRef, Spy(21));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r6->Run();
    EXPECT_EQ(0, gCopyConstructions); // No copies in call.
    EXPECT_EQ(21, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from lvalue, pass by rvalue ref\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r7;
    { // Block around Spy lifetime.
      if (gDebug) { printf("%d - Spy s(30)\n", __LINE__); }
      Spy s(30);
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r7 = NS_NewRunnableMethodWithArgs<StoreCopyPassByRRef<Spy>>(&TestByRRef, s)\n", __LINE__); }
      r7 = NS_NewRunnableMethodWithArgs<StoreCopyPassByRRef<Spy>>(
           rpt, &ThreadUtilsObject::TestByRRef, s);
      EXPECT_EQ(2, gAlive);
      EXPECT_LE(1, gCopyConstructions); // At least 1 copy-construction.
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with Spy s(30)\n", __LINE__); }
    }
    EXPECT_EQ(1, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r7->Run();
    EXPECT_LE(1, gMoves); // Move in call.
    EXPECT_EQ(30, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(0, gAlive); // Spy inside Test is not counted.
    EXPECT_EQ(1, gZombies); // Our local spy should now be a zombie.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store copy from prvalue, pass by rvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - r8 = NS_NewRunnableMethodWithArgs<StoreCopyPassByRRef<Spy>>(&TestByRRef, Spy(31))\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r8 =
      NS_NewRunnableMethodWithArgs<StoreCopyPassByRRef<Spy>>(
        rpt, &ThreadUtilsObject::TestByRRef, Spy(31));
    EXPECT_EQ(1, gAlive);
    EXPECT_EQ(1, gConstructions);
    EXPECT_LE(1, gMoveConstructions);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r8->Run();
    EXPECT_LE(1, gMoves); // Move in call.
    EXPECT_EQ(31, rpt->mSpy.mID);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(0, gAlive); // Spy inside Test is not counted.
    EXPECT_EQ(1, gZombies); // Our local spy should now be a zombie.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store lvalue ref, pass lvalue ref\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(40)\n", __LINE__); }
    Spy s(40);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r9 = NS_NewRunnableMethodWithArgs<Spy&>(&TestByLRef, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r9 =
      NS_NewRunnableMethodWithArgs<Spy&>(
        rpt, &ThreadUtilsObject::TestByLRef, s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r9->Run();
    EXPECT_LE(1, gAssignments); // Assignment from reference in call.
    EXPECT_EQ(40, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store nsRefPtr, pass by pointer\n", __LINE__); }
  { // Block around nsCOMPtr lifetime.
    nsCOMPtr<nsIRunnable> r10;
    SpyWithISupports* ptr = 0;
    { // Block around nsRefPtr<Spy> lifetime.
      if (gDebug) { printf("%d - nsRefPtr<SpyWithISupports> s(new SpyWithISupports(45))\n", __LINE__); }
      nsRefPtr<SpyWithISupports> s(new SpyWithISupports(45));
      ptr = s.get();
      EXPECT_EQ(1, gConstructions);
      EXPECT_EQ(1, gAlive);
      if (gDebug) { printf("%d - r10 = NS_NewRunnableMethodWithArgs<StorensRefPtrPassByPtr<Spy>>(&TestByRRef, s.get())\n", __LINE__); }
      r10 = NS_NewRunnableMethodWithArgs<StorensRefPtrPassByPtr<SpyWithISupports>>(
            rpt, &ThreadUtilsObject::TestByPointer, s.get());
      EXPECT_LE(0, gAllConstructions);
      EXPECT_EQ(1, gAlive);
      Spy::ClearActions();
      if (gDebug) { printf("%d - End block with nsRefPtr<Spy> s\n", __LINE__); }
    }
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r10->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(45, rpt->mSpy.mID);
    EXPECT_EQ(ptr, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store pointer to lvalue, pass by pointer\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(55)\n", __LINE__); }
    Spy s(55);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r11 = NS_NewRunnableMethodWithArgs<Spy*>(&TestByPointer, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r11 =
      NS_NewRunnableMethodWithArgs<Spy*>(
        rpt, &ThreadUtilsObject::TestByPointer, &s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r11->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(55, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);

  Spy::ClearAll();
  if (gDebug) { printf("%d - Test: Store pointer to const lvalue, pass by pointer\n", __LINE__); }
  {
    if (gDebug) { printf("%d - Spy s(60)\n", __LINE__); }
    Spy s(60);
    EXPECT_EQ(1, gConstructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - r12 = NS_NewRunnableMethodWithArgs<Spy*>(&TestByPointer, s)\n", __LINE__); }
    nsCOMPtr<nsIRunnable> r12 =
      NS_NewRunnableMethodWithArgs<const Spy*>(
        rpt, &ThreadUtilsObject::TestByPointerToConst, &s);
    EXPECT_EQ(0, gAllConstructions);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive);
    Spy::ClearActions();
    if (gDebug) { printf("%d - Run()\n", __LINE__); }
    r12->Run();
    EXPECT_LE(1, gAssignments); // Assignment from pointee in call.
    EXPECT_EQ(60, rpt->mSpy.mID);
    EXPECT_EQ(&s, rpt->mSpyPtr);
    EXPECT_EQ(0, gDestructions);
    EXPECT_EQ(1, gAlive); // Spy inside Test is not counted.
    Spy::ClearActions();
    if (gDebug) { printf("%d - End block with r\n", __LINE__); }
  }
  if (gDebug) { printf("%d - After end block with r\n", __LINE__); }
  EXPECT_EQ(1, gDestructions);
  EXPECT_EQ(0, gAlive);
#endif // XPCOM_GLUE_AVOID_NSPR
}
