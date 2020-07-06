/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "mozilla/SmallArrayLRUCache.h"

#include <algorithm>
#include <cstring>
#include <utility>

using Key = unsigned;

struct Value {
  Value() : m(unsigned(-1)) {}
  explicit Value(unsigned a) : m(a) {}

  bool operator==(const Value& aOther) const { return m == aOther.m; }
  bool operator!=(const Value& aOther) const { return m != aOther.m; }

  unsigned m;
};

constexpr static unsigned CacheSize = 8;

using TestCache = mozilla::SmallArrayLRUCache<Key, Value, CacheSize>;

// This struct embeds a given object type between two "guard" objects, to check
// if anything is written out of bounds.
template <typename T>
struct Boxed {
  constexpr static size_t GuardSize = std::max(sizeof(T), size_t(256));

  // A Guard is a character array with a pre-set content that can be checked for
  // unwanted changes.
  struct Guard {
    char mGuard[GuardSize];
    explicit Guard(char aValue) { memset(&mGuard, aValue, GuardSize); }
    void Check(char aValue) {
      for (const char& c : mGuard) {
        ASSERT_EQ(c, aValue);
      }
    }
  };

  Guard mGuardBefore;
  T mObject;
  Guard mGuardAfter;

  template <typename... Ts>
  explicit Boxed(Ts&&... aTs)
      : mGuardBefore(0x5a),
        mObject(std::forward<Ts>(aTs)...),
        mGuardAfter(0xa5) {
    Check();
  }

  ~Boxed() { Check(); }

  T& Object() { return mObject; }
  const T& Object() const { return mObject; }

  void Check() {
    mGuardBefore.Check(0x5a);
    mGuardAfter.Check(0xa5);
  }
};

TEST(SmallArrayLRUCache, FetchOrAdd_KeysFitInCache)
{
  // We're going to add-or-fetch between 1 and CacheSize keys, so they all fit
  // in the cache.
  for (Key keys = 1; keys <= CacheSize; ++keys) {
    Boxed<TestCache> boxedCache;
    TestCache& cache = boxedCache.Object();
    for (Key i = 0; i < keys; ++i) {
      bool valueFunctionCalled = false;
      Value v = cache.FetchOrAdd(i, [&]() {
        valueFunctionCalled = true;
        return Value{i};
      });
      ASSERT_EQ(v, Value{i});
      ASSERT_TRUE(valueFunctionCalled);
      boxedCache.Check();
    }

    // Fetching any key should never call the value function.
    for (Key i = 0; i < CacheSize * 3; ++i) {
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
      // Fetching the same key again will never call the function value.
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
    }
  }
}

TEST(SmallArrayLRUCache, Add_FetchOrAdd_KeysFitInCache)
{
  // We're going to add between 1 and CacheSize keys, so they all fit in the
  // cache.
  for (Key keys = 1; keys <= CacheSize; ++keys) {
    Boxed<TestCache> boxedCache;
    TestCache& cache = boxedCache.Object();
    for (Key i = 0; i < keys; ++i) {
      cache.Add(i, Value{i});
      boxedCache.Check();
    }

    // Fetching any key should never call the value function.
    for (Key i = 0; i < CacheSize * 3; ++i) {
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
      // Fetching the same key again will never call the function value.
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
    }
  }
}

TEST(SmallArrayLRUCache, FetchOrAdd_KeysDoNotFitInCache)
{
  // We're going to add-or-fetch strictly more than CacheSize keys, so they
  // cannot fit in the cache, only the last `CacheSize` ones are kept.
  for (Key keys = CacheSize + 1; keys <= CacheSize * 2; ++keys) {
    Boxed<TestCache> boxedCache;
    TestCache& cache = boxedCache.Object();
    for (Key i = 0; i < keys; ++i) {
      bool valueFunctionCalled = false;
      Value v = cache.FetchOrAdd(i, [&]() {
        valueFunctionCalled = true;
        return Value{i};
      });
      ASSERT_EQ(v, Value{i});
      ASSERT_TRUE(valueFunctionCalled);
      boxedCache.Check();
    }

    // Fetching keys from 0 should always call the function value:
    // - 0 is the oldest key, it must have been pushed out when `CacheSize`
    //   was added.
    // - Once we've fetched 0, it's pushed out the old (smallest) key.
    // Etc.
    for (Key i = 0; i < CacheSize * 3; ++i) {
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_TRUE(valueFunctionCalled);
        boxedCache.Check();
      }
      // Fetching the same key again will never call the function value.
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
    }
  }
}

TEST(SmallArrayLRUCache, Add_FetchOrAdd_KeysDoNotFitInCache)
{
  // We're going to add strictly more than CacheSize keys, so they cannot fit in
  // the cache, only the last `CacheSize` ones are kept.
  for (Key keys = CacheSize + 1; keys <= CacheSize * 2; ++keys) {
    Boxed<TestCache> boxedCache;
    TestCache& cache = boxedCache.Object();
    for (Key i = 0; i < keys; ++i) {
      cache.Add(i, Value{i});
      boxedCache.Check();
    }

    // Fetching keys from 0 should always call the function value:
    // - 0 is the oldest key, it must have been pushed out when `CacheSize`
    //   was added.
    // - Once we've fetched 0, it's pushed out the old (smallest) key.
    // Etc.
    for (Key i = 0; i < CacheSize * 3; ++i) {
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_TRUE(valueFunctionCalled);
        boxedCache.Check();
      }
      // Fetching the same key again will never call the function value.
      {
        bool valueFunctionCalled = false;
        Value v = cache.FetchOrAdd(i % keys, [&]() {
          valueFunctionCalled = true;
          return Value{i % keys};
        });
        ASSERT_EQ(v, Value{i % keys});
        ASSERT_FALSE(valueFunctionCalled);
        boxedCache.Check();
      }
    }
  }
}

TEST(SmallArrayLRUCache, Clear)
{
  Boxed<TestCache> boxedCache;
  TestCache& cache = boxedCache.Object();

  // First fetch will always call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }

  // Second fetch will never call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_FALSE(valueFunctionCalled);
    boxedCache.Check();
  }

  cache.Clear();

  // After Clear(), first fetch will always call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }

  // Next fetch will never call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_FALSE(valueFunctionCalled);
    boxedCache.Check();
  }
}

TEST(SmallArrayLRUCache, Shutdown)
{
  Boxed<TestCache> boxedCache;
  TestCache& cache = boxedCache.Object();

  // First fetch will always call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }

  // Second fetch will never call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_FALSE(valueFunctionCalled);
    boxedCache.Check();
  }

  cache.Shutdown();

  // After Shutdown(), any fetch will always call the function value.
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }
  cache.Add(42, Value{4242});
  boxedCache.Check();
  {
    bool valueFunctionCalled = false;
    Value v = cache.FetchOrAdd(42, [&]() {
      valueFunctionCalled = true;
      return Value{4242};
    });
    ASSERT_EQ(v, Value{4242});
    ASSERT_TRUE(valueFunctionCalled);
    boxedCache.Check();
  }
}
