/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that var caches are updated before callbacks.

#include "gtest/gtest.h"
#include "Preferences.h"

namespace mozilla {

template<typename T, typename U>
struct Closure
{
  U* mLocation;
  T mExpected;
  bool mCalled;
};

template<typename T, typename U>
void
VarChanged(const char* aPrefName, void* aData)
{
  auto closure = static_cast<Closure<T, U>*>(aData);
  ASSERT_EQ(*closure->mLocation, closure->mExpected);
  ASSERT_FALSE(closure->mCalled);
  closure->mCalled = true;
}

void
SetFunc(const char* aPrefName, bool aValue)
{
  nsresult rv = Preferences::SetBool(aPrefName, aValue);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
SetFunc(const char* aPrefName, int32_t aValue)
{
  nsresult rv = Preferences::SetInt(aPrefName, aValue);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
SetFunc(const char* aPrefName, uint32_t aValue)
{
  nsresult rv = Preferences::SetUint(aPrefName, aValue);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
SetFunc(const char* aPrefName, float aValue)
{
  nsresult rv = Preferences::SetFloat(aPrefName, aValue);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(bool* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddBoolVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(Atomic<bool, Relaxed>* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddAtomicBoolVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(Atomic<bool, ReleaseAcquire>* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddAtomicBoolVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(int32_t* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddIntVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(Atomic<int32_t, Relaxed>* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddAtomicIntVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(uint32_t* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddUintVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(Atomic<uint32_t, Relaxed>* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddAtomicUintVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(Atomic<uint32_t, ReleaseAcquire>* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddAtomicUintVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void
AddVarCacheFunc(float* aVar, const char* aPrefName)
{
  nsresult rv = Preferences::AddFloatVarCache(aVar, aPrefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

template<typename T, typename U = T>
void
RunTest(const char* aPrefName1, const char* aPrefName2, T aValue1, T aValue2)
{
  static U var1, var2;
  static Closure<T, U> closure1, closure2;
  nsresult rv;

  ASSERT_STRNE(aPrefName1, aPrefName2);
  ASSERT_NE(aValue1, aValue2);

  // Call Add*VarCache first.

  SetFunc(aPrefName1, aValue1);

  AddVarCacheFunc(&var1, aPrefName1);
  ASSERT_EQ(var1, aValue1);

  closure1 = { &var1, aValue2 };
  rv = Preferences::RegisterCallback(VarChanged<T, U>, aPrefName1, &closure1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(closure1.mCalled);
  SetFunc(aPrefName1, aValue2);
  ASSERT_EQ(var1, aValue2);
  ASSERT_TRUE(closure1.mCalled);

  // Call RegisterCallback first.

  SetFunc(aPrefName2, aValue1);

  closure2 = { &var2, aValue2 };
  rv = Preferences::RegisterCallback(VarChanged<T, U>, aPrefName2, &closure2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  AddVarCacheFunc(&var2, aPrefName2);
  ASSERT_EQ(var2, aValue1);

  ASSERT_FALSE(closure2.mCalled);
  SetFunc(aPrefName2, aValue2);
  ASSERT_EQ(var2, aValue2);
  ASSERT_TRUE(closure2.mCalled);
}

TEST(CallbackAndVarCacheOrder, Bool)
{
  RunTest<bool>("test_pref.bool.1", "test_pref.bool.2", false, true);
}

TEST(CallbackAndVarCacheOrder, AtomicBoolRelaxed)
{
  RunTest<bool, Atomic<bool, Relaxed>>(
    "test_pref.atomic_bool.1", "test_pref.atomic_bool.2", false, true);
 }

TEST(CallbackAndVarCacheOrder, AtomicBoolReleaseAcquire)
{
  RunTest<bool, Atomic<bool, ReleaseAcquire>>(
    "test_pref.atomic_bool.3", "test_pref.atomic_bool.4", false, true);
}

TEST(CallbackAndVarCacheOrder, Int)
{
  RunTest<int32_t>("test_pref.int.1", "test_pref.int.2", -2, 3);
}

TEST(CallbackAndVarCacheOrder, AtomicInt)
{
  RunTest<int32_t, Atomic<int32_t, Relaxed>>(
    "test_pref.atomic_int.1", "test_pref.atomic_int.2", -3, 4);
}

TEST(CallbackAndVarCacheOrder, Uint)
{
  RunTest<uint32_t>("test_pref.uint.1", "test_pref.uint.2", 4u, 5u);
}

TEST(CallbackAndVarCacheOrder, AtomicUintRelaxed)
{
  RunTest<uint32_t, Atomic<uint32_t, Relaxed>>(
    "test_pref.atomic_uint.1", "test_pref.atomic_uint.2", 6u, 7u);
}

TEST(CallbackAndVarCacheOrder, AtomicUintReleaseAcquire)
{
  RunTest<uint32_t, Atomic<uint32_t, ReleaseAcquire>>(
    "test_pref.atomic_uint.3", "test_pref.atomic_uint.4", 8u, 9u);
}

TEST(CallbackAndVarCacheOrder, Float)
{
  RunTest<float>("test_pref.float.1", "test_pref.float.2", -10.0f, 11.0f);
}

} // namespace mozilla
