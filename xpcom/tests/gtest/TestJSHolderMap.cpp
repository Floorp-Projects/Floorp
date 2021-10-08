/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include "nsISupportsUtils.h"
#include "nsCycleCollectionParticipant.h"

#include "gtest/gtest.h"

using mozilla::JSHolderMap;
using mozilla::Maybe;
using mozilla::UniquePtr;
using mozilla::Vector;

enum HolderKind { SingleZone, MultiZone };

class MyHolder final : public nsScriptObjectTracer {
 public:
  explicit MyHolder(HolderKind kind = SingleZone, size_t value = 0)
      : nsScriptObjectTracer(FlagsForKind(kind)), value(value) {}

  const size_t value;

  NS_IMETHOD_(void) Root(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) Unlink(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) Unroot(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void) DeleteCycleCollectable(void*) override { MOZ_CRASH(); }
  NS_IMETHOD_(void)
  Trace(void* aPtr, const TraceCallbacks& aCb, void* aClosure) override {
    MOZ_CRASH();
  }
  NS_IMETHOD TraverseNative(void* aPtr,
                            nsCycleCollectionTraversalCallback& aCb) override {
    MOZ_CRASH();
  }

  NS_DECL_CYCLE_COLLECTION_CLASS_NAME_METHOD(MyHolder)

 private:
  Flags FlagsForKind(HolderKind kind) {
    return kind == MultiZone ? FlagMultiZoneJSHolder : 0;
  }
};

static size_t CountEntries(JSHolderMap& map) {
  size_t count = 0;
  for (JSHolderMap::Iter i(map); !i.Done(); i.Next()) {
    MOZ_RELEASE_ASSERT(i->mHolder);
    MOZ_RELEASE_ASSERT(i->mTracer);
    count++;
  }
  return count;
}

JS::Zone* DummyZone = reinterpret_cast<JS::Zone*>(1);

JS::Zone* ZoneForKind(HolderKind kind) {
  return kind == MultiZone ? nullptr : DummyZone;
}

TEST(JSHolderMap, Empty)
{
  JSHolderMap map;
  ASSERT_EQ(CountEntries(map), 0u);
}

static void TestAddAndRemove(HolderKind kind) {
  JSHolderMap map;

  MyHolder holder(kind);
  nsScriptObjectTracer* tracer = &holder;

  ASSERT_FALSE(map.Has(&holder));
  ASSERT_EQ(map.Extract(&holder), nullptr);

  map.Put(&holder, tracer, ZoneForKind(kind));
  ASSERT_TRUE(map.Has(&holder));
  ASSERT_EQ(CountEntries(map), 1u);
  ASSERT_EQ(map.Get(&holder), tracer);

  ASSERT_EQ(map.Extract(&holder), tracer);
  ASSERT_EQ(map.Extract(&holder), nullptr);
  ASSERT_FALSE(map.Has(&holder));
  ASSERT_EQ(CountEntries(map), 0u);
}

TEST(JSHolderMap, AddAndRemove)
{
  TestAddAndRemove(SingleZone);
  TestAddAndRemove(MultiZone);
}

static void TestIterate(HolderKind kind) {
  JSHolderMap map;

  MyHolder holder(kind, 0);
  nsScriptObjectTracer* tracer = &holder;

  Maybe<JSHolderMap::Iter> iter;

  // Iterate an empty map.
  iter.emplace(map);
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Iterate a map with one entry.
  map.Put(&holder, tracer, ZoneForKind(kind));
  iter.emplace(map);
  ASSERT_FALSE(iter->Done());
  ASSERT_EQ(iter->Get().mHolder, &holder);
  iter->Next();
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Iterate a map with 10 entries.
  constexpr size_t count = 10;
  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  bool seen[count] = {};
  for (size_t i = 1; i < count; i++) {
    MOZ_ALWAYS_TRUE(
        holders.emplaceBack(mozilla::MakeUnique<MyHolder>(kind, i)));
    map.Put(holders.back().get(), tracer, ZoneForKind(kind));
  }
  for (iter.emplace(map); !iter->Done(); iter->Next()) {
    MyHolder* holder = static_cast<MyHolder*>(iter->Get().mHolder);
    size_t value = holder->value;
    ASSERT_TRUE(value < count);
    ASSERT_FALSE(seen[value]);
    seen[value] = true;
  }
  for (const auto& s : seen) {
    ASSERT_TRUE(s);
  }
}

TEST(JSHolderMap, Iterate)
{
  TestIterate(SingleZone);
  TestIterate(MultiZone);
}

static void TestAddRemoveMany(HolderKind kind, size_t count) {
  JSHolderMap map;

  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  for (size_t i = 0; i < count; i++) {
    MOZ_ALWAYS_TRUE(holders.emplaceBack(mozilla::MakeUnique<MyHolder>(kind)));
  }

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    map.Put(holder, holder, ZoneForKind(kind));
  }

  ASSERT_EQ(CountEntries(map), count);

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    ASSERT_EQ(map.Extract(holder), holder);
  }

  ASSERT_EQ(CountEntries(map), 0u);
}

TEST(JSHolderMap, TestAddRemoveMany)
{
  TestAddRemoveMany(SingleZone, 10000);
  TestAddRemoveMany(MultiZone, 10000);
}
