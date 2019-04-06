/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/MruCache.h"
#include "nsString.h"

using namespace mozilla;

// A few MruCache implementations to use during testing.
struct IntMap : public MruCache<int, int, IntMap> {
  static HashNumber Hash(const KeyType& aKey) { return aKey - 1; }
  static bool Match(const KeyType& aKey, const ValueType& aVal) {
    return aKey == aVal;
  }
};

struct UintPtrMap : public MruCache<uintptr_t, int*, UintPtrMap> {
  static HashNumber Hash(const KeyType& aKey) { return aKey - 1; }
  static bool Match(const KeyType& aKey, const ValueType& aVal) {
    return aKey == (KeyType)aVal;
  }
};

struct StringStruct {
  nsCString mKey;
  nsCString mOther;
};

struct StringStructMap
    : public MruCache<nsCString, StringStruct, StringStructMap> {
  static HashNumber Hash(const KeyType& aKey) {
    return *aKey.BeginReading() - 1;
  }
  static bool Match(const KeyType& aKey, const ValueType& aVal) {
    return aKey == aVal.mKey;
  }
};

// Helper for emulating convertable holders such as RefPtr.
template <typename T>
struct Convertable {
  T mItem;
  operator T() const { return mItem; }
};

// Helper to create a StringStructMap key.
static nsCString MakeStringKey(char aKey) {
  nsCString key;
  key.Append(aKey);
  return key;
}

TEST(MruCache, TestNullChecker)
{
  using mozilla::detail::EmptyChecker;

  {
    int test = 0;
    EXPECT_TRUE(EmptyChecker<decltype(test)>::IsNotEmpty(test));

    test = 42;
    EXPECT_TRUE(EmptyChecker<decltype(test)>::IsNotEmpty(test));
  }

  {
    const char* test = "abc";
    EXPECT_TRUE(EmptyChecker<decltype(test)>::IsNotEmpty(test));

    test = nullptr;
    EXPECT_FALSE(EmptyChecker<decltype(test)>::IsNotEmpty(test));
  }

  {
    int foo = 42;
    int* test = &foo;
    EXPECT_TRUE(EmptyChecker<decltype(test)>::IsNotEmpty(test));

    test = nullptr;
    EXPECT_FALSE(EmptyChecker<decltype(test)>::IsNotEmpty(test));
  }
}

TEST(MruCache, TestEmptyCache)
{
  {
    // Test a basic empty cache.
    IntMap mru;

    // Make sure the default values are set.
    for (int i = 1; i < 32; i++) {
      auto p = mru.Lookup(i);

      // Shouldn't be found.
      EXPECT_FALSE(p);
    }
  }

  {
    // Test an empty cache with pointer values.
    UintPtrMap mru;

    // Make sure the default values are set.
    for (uintptr_t i = 1; i < 32; i++) {
      auto p = mru.Lookup(i);

      // Shouldn't be found.
      EXPECT_FALSE(p);
    }
  }

  {
    // Test an empty cache with more complex structure.
    StringStructMap mru;

    // Make sure the default values are set.
    for (char i = 1; i < 32; i++) {
      const nsCString key = MakeStringKey(i);
      auto p = mru.Lookup(key);

      // Shouldn't be found.
      EXPECT_FALSE(p);
    }
  }
}

TEST(MruCache, TestPut)
{
  IntMap mru;

  // Fill it up.
  for (int i = 1; i < 32; i++) {
    mru.Put(i, i);
  }

  // Now check each value.
  for (int i = 1; i < 32; i++) {
    auto p = mru.Lookup(i);

    // Should be found.
    EXPECT_TRUE(p);
    EXPECT_EQ(p.Data(), i);
  }
}

TEST(MruCache, TestPutConvertable)
{
  UintPtrMap mru;

  // Fill it up.
  for (uintptr_t i = 1; i < 32; i++) {
    Convertable<int*> val{(int*)i};
    mru.Put(i, val);
  }

  // Now check each value.
  for (uintptr_t i = 1; i < 32; i++) {
    auto p = mru.Lookup(i);

    // Should be found.
    EXPECT_TRUE(p);
    EXPECT_EQ(p.Data(), (int*)i);
  }
}

TEST(MruCache, TestOverwriting)
{
  // Test overwrting
  IntMap mru;

  // 1-31 should be overwritten by 32-63
  for (int i = 1; i < 63; i++) {
    mru.Put(i, i);
  }

  // Look them up.
  for (int i = 32; i < 63; i++) {
    auto p = mru.Lookup(i);

    // Should be found.
    EXPECT_TRUE(p);
    EXPECT_EQ(p.Data(), i);
  }
}

TEST(MruCache, TestRemove)
{
  {
    IntMap mru;

    // Fill it up.
    for (int i = 1; i < 32; i++) {
      mru.Put(i, i);
    }

    // Now remove each value.
    for (int i = 1; i < 32; i++) {
      // Should be present.
      auto p = mru.Lookup(i);
      EXPECT_TRUE(p);

      mru.Remove(i);

      // Should no longer match.
      p = mru.Lookup(i);
      EXPECT_FALSE(p);
    }
  }

  {
    UintPtrMap mru;

    // Fill it up.
    for (uintptr_t i = 1; i < 32; i++) {
      mru.Put(i, (int*)i);
    }

    // Now remove each value.
    for (uintptr_t i = 1; i < 32; i++) {
      // Should be present.
      auto p = mru.Lookup(i);
      EXPECT_TRUE(p);

      mru.Remove(i);

      // Should no longer match.
      p = mru.Lookup(i);
      EXPECT_FALSE(p);
    }
  }

  {
    StringStructMap mru;

    // Fill it up.
    for (char i = 1; i < 32; i++) {
      const nsCString key = MakeStringKey(i);
      mru.Put(key, StringStruct{key, NS_LITERAL_CSTRING("foo")});
    }

    // Now remove each value.
    for (char i = 1; i < 32; i++) {
      const nsCString key = MakeStringKey(i);

      // Should be present.
      auto p = mru.Lookup(key);
      EXPECT_TRUE(p);

      mru.Remove(key);

      // Should no longer match.
      p = mru.Lookup(key);
      EXPECT_FALSE(p);
    }
  }
}

TEST(MruCache, TestClear)
{
  IntMap mru;

  // Fill it up.
  for (int i = 1; i < 32; i++) {
    mru.Put(i, i);
  }

  // Empty it.
  mru.Clear();

  // Now check each value.
  for (int i = 1; i < 32; i++) {
    auto p = mru.Lookup(i);

    // Should not be found.
    EXPECT_FALSE(p);
  }
}

TEST(MruCache, TestLookupMissingAndSet)
{
  IntMap mru;

  // Value not found.
  auto p = mru.Lookup(1);
  EXPECT_FALSE(p);

  // Set it.
  p.Set(1);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 1);

  // Look it up again.
  p = mru.Lookup(1);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 1);

  // Test w/ a convertable value.
  p = mru.Lookup(2);
  EXPECT_FALSE(p);

  // Set it.
  Convertable<int> val{2};
  p.Set(val);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 2);

  // Look it up again.
  p = mru.Lookup(2);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 2);
}

TEST(MruCache, TestLookupAndOverwrite)
{
  IntMap mru;

  // Set 1.
  mru.Put(1, 1);

  // Lookup a key that maps the 1's entry.
  auto p = mru.Lookup(32);
  EXPECT_FALSE(p);  // not a match

  // Now overwrite the entry.
  p.Set(32);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 32);

  // 1 should be gone now.
  p = mru.Lookup(1);
  EXPECT_FALSE(p);

  // 32 should be found.
  p = mru.Lookup(32);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 32);
}

TEST(MruCache, TestLookupAndRemove)
{
  IntMap mru;

  // Set 1.
  mru.Put(1, 1);

  auto p = mru.Lookup(1);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 1);

  // Now remove it.
  p.Remove();
  EXPECT_FALSE(p);

  p = mru.Lookup(1);
  EXPECT_FALSE(p);
}

TEST(MruCache, TestLookupNotMatchedAndRemove)
{
  IntMap mru;

  // Set 1.
  mru.Put(1, 1);

  // Lookup a key that matches 1's entry.
  auto p = mru.Lookup(32);
  EXPECT_FALSE(p);

  // Now attempt to remove it.
  p.Remove();

  // Make sure 1 is still there.
  p = mru.Lookup(1);
  EXPECT_TRUE(p);
  EXPECT_EQ(p.Data(), 1);
}

TEST(MruCache, TestLookupAndSetWithMove)
{
  StringStructMap mru;

  const nsCString key = MakeStringKey((char)1);
  StringStruct val{key, NS_LITERAL_CSTRING("foo")};

  auto p = mru.Lookup(key);
  EXPECT_FALSE(p);
  p.Set(std::move(val));

  EXPECT_TRUE(p.Data().mKey == key);
  EXPECT_TRUE(p.Data().mOther == NS_LITERAL_CSTRING("foo"));
}
