/* -*-  Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Test that var caches are updated before callbacks.

#include "gtest/gtest.h"
#include "Preferences.h"

namespace mozilla {

template<typename T, typename U>
struct Closure {
  U* location;
  T expected;
  bool called;
};

template<typename T, typename U>
void VarChanged(const char* aPrefName, void* aData)
{
  auto closure = static_cast<Closure<T, U>*>(aData);
  ASSERT_EQ(*closure->location, closure->expected);
  ASSERT_FALSE(closure->called);
  closure->called = true;
}

void SetFunc(const char* prefName, bool value)
{
  nsresult rv = Preferences::SetBool(prefName, value);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void SetFunc(const char* prefName, int32_t value)
{
  nsresult rv = Preferences::SetInt(prefName, value);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void SetFunc(const char* prefName, uint32_t value)
{
  nsresult rv = Preferences::SetUint(prefName, value);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void SetFunc(const char* prefName, float value)
{
  nsresult rv = Preferences::SetFloat(prefName, value);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void AddVarCacheFunc(bool* var, const char* prefName)
{
  nsresult rv = Preferences::AddBoolVarCache(var, prefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void AddVarCacheFunc(int32_t* var, const char* prefName)
{
  nsresult rv = Preferences::AddIntVarCache(var, prefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void AddVarCacheFunc(uint32_t* var, const char* prefName)
{
  nsresult rv = Preferences::AddUintVarCache(var, prefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void AddVarCacheFunc(Atomic<uint32_t,Relaxed>* var,
                     const char* prefName)
{
  nsresult rv = Preferences::AddAtomicUintVarCache(var, prefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

void AddVarCacheFunc(float* var, const char* prefName)
{
  nsresult rv = Preferences::AddFloatVarCache(var, prefName);
  ASSERT_TRUE(NS_SUCCEEDED(rv));
}

template<typename T, typename U = T>
void RunTest(const char* prefName1, const char* prefName2,
             T value1, T value2)
{
  static U var1, var2;
  static Closure<T, U> closure1, closure2;
  nsresult rv;

  ASSERT_STRNE(prefName1, prefName2);
  ASSERT_NE(value1, value2);

  //
  // Call Add*VarCache first.
  //
  SetFunc(prefName1, value1);

  AddVarCacheFunc(&var1, prefName1);
  ASSERT_EQ(var1, value1);

  closure1 = { &var1, value2 };
  rv = Preferences::RegisterCallback(VarChanged<T, U>, prefName1, &closure1);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  ASSERT_FALSE(closure1.called);
  SetFunc(prefName1, value2);
  ASSERT_EQ(var1, value2);
  ASSERT_TRUE(closure1.called);

  //
  // Call RegisterCallback first.
  //
  SetFunc(prefName2, value1);

  closure2 = { &var2, value2 };
  rv = Preferences::RegisterCallback(VarChanged<T, U>, prefName2, &closure2);
  ASSERT_TRUE(NS_SUCCEEDED(rv));

  AddVarCacheFunc(&var2, prefName2);
  ASSERT_EQ(var2, value1);

  ASSERT_FALSE(closure2.called);
  SetFunc(prefName2, value2);
  ASSERT_EQ(var2, value2);
  ASSERT_TRUE(closure2.called);
}

TEST(CallbackAndVarCacheOrder, Bool) {
  RunTest<bool>("test_pref.bool.1", "test_pref.bool.2", false, true);
}

TEST(CallbackAndVarCacheOrder, Int) {
  RunTest<int32_t>("test_pref.int.1", "test_pref.int.2", -2, 3);
}

TEST(CallbackAndVarCacheOrder, Uint) {
  RunTest<uint32_t>("test_pref.uint.1", "test_pref.uint.2", 4u, 5u);
}

TEST(CallbackAndVarCacheOrder, AtomicUint) {
  RunTest<uint32_t,Atomic<uint32_t,Relaxed>>("test_pref.atomic_uint.1",
                                             "test_pref.atomic_uint.2",
                                             6u, 7u);
}

TEST(CallbackAndVarCacheOrder, Float) {
  RunTest<float>("test_pref.float.1", "test_pref.float.2", -8.0f, 9.0f);
}

} // namespace mozilla
