/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/CycleCollectedJSRuntime.h"
#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"

#include "nsCycleCollectionParticipant.h"
#include "nsCycleCollector.h"

#include "js/GCAPI.h"

#include "gtest/gtest.h"

using namespace mozilla;

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
    return kind == MultiZone ? FlagMultiZoneJSHolder
                             : FlagMaybeSingleZoneJSHolder;
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

static void TestRemoveWhileIterating(HolderKind kind, size_t count) {
  JSHolderMap map;
  Vector<UniquePtr<MyHolder>, 0, InfallibleAllocPolicy> holders;
  Maybe<JSHolderMap::Iter> iter;

  for (size_t i = 0; i < count; i++) {
    MOZ_ALWAYS_TRUE(holders.emplaceBack(MakeUnique<MyHolder>(kind)));
  }

  // Iterate a map with one entry but remove it before we get to it.
  MyHolder* holder = holders[0].get();
  map.Put(holder, holder, ZoneForKind(kind));
  iter.emplace(map);
  ASSERT_FALSE(iter->Done());
  ASSERT_EQ(map.Extract(holder), holder);
  iter->UpdateForRemovals();
  ASSERT_TRUE(iter->Done());

  // Check UpdateForRemovals is safe to call on a done iterator.
  iter->UpdateForRemovals();
  ASSERT_TRUE(iter->Done());
  iter.reset();

  // Add many holders and remove them mid way through iteration.

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    map.Put(holder, holder, ZoneForKind(kind));
  }

  iter.emplace(map);
  for (size_t i = 0; i < count / 2; i++) {
    iter->Next();
    ASSERT_FALSE(iter->Done());
  }

  for (size_t i = 0; i < count; i++) {
    MyHolder* holder = holders[i].get();
    ASSERT_EQ(map.Extract(holder), holder);
  }

  iter->UpdateForRemovals();

  ASSERT_TRUE(iter->Done());
  iter.reset();

  ASSERT_EQ(CountEntries(map), 0u);
}

TEST(JSHolderMap, TestRemoveWhileIterating)
{
  TestRemoveWhileIterating(SingleZone, 10000);
  TestRemoveWhileIterating(MultiZone, 10000);
}

class ObjectHolder final {
 public:
  ObjectHolder() { HoldJSObjects(this); }

  NS_INLINE_DECL_CYCLE_COLLECTING_NATIVE_REFCOUNTING(ObjectHolder)
  NS_DECL_CYCLE_COLLECTION_SCRIPT_HOLDER_NATIVE_CLASS(ObjectHolder)

  void SetObject(JSObject* aObject) { mObject = aObject; }

  void ClearObject() { mObject = nullptr; }

  JSObject* GetObject() const { return mObject; }
  JSObject* GetObjectUnbarriered() const { return mObject.unbarrieredGet(); }

  bool ObjectIsGray() const {
    JSObject* obj = mObject.unbarrieredGet();
    MOZ_RELEASE_ASSERT(obj);
    return JS::GCThingIsMarkedGray(JS::GCCellPtr(obj));
  }

 private:
  JS::Heap<JSObject*> mObject;

  ~ObjectHolder() { DropJSObjects(this); }
};

NS_IMPL_CYCLE_COLLECTION_CLASS(ObjectHolder)

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN(ObjectHolder)
  tmp->ClearObject();
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN(ObjectHolder)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_TRACE_BEGIN(ObjectHolder)
  NS_IMPL_CYCLE_COLLECTION_TRACE_JS_MEMBER_CALLBACK(mObject)
NS_IMPL_CYCLE_COLLECTION_TRACE_END

NS_IMPL_CYCLE_COLLECTION_ROOT_NATIVE(ObjectHolder, AddRef)
NS_IMPL_CYCLE_COLLECTION_UNROOT_NATIVE(ObjectHolder, Release)

// Test GC things stored in JS holders are marked as gray roots by the GC.
static void TestHoldersAreMarkedGray(JSContext* cx) {
  RefPtr holder(new ObjectHolder);

  JSObject* obj = JS_NewPlainObject(cx);
  ASSERT_TRUE(obj);
  holder->SetObject(obj);
  obj = nullptr;

  JS_GC(cx);

  ASSERT_TRUE(holder->ObjectIsGray());
}

// Test GC things stored in JS holders are updated by compacting GC.
static void TestHoldersAreMoved(JSContext* cx, bool singleZone) {
  JS::RootedObject obj(cx, JS_NewPlainObject(cx));
  ASSERT_TRUE(obj);

  // Set a property so we can check we have the same object at the end.
  const char* PropertyName = "answer";
  const int32_t PropertyValue = 42;
  JS::RootedValue value(cx, JS::Int32Value(PropertyValue));
  ASSERT_TRUE(JS_SetProperty(cx, obj, PropertyName, value));

  // Ensure the object is tenured.
  JS_GC(cx);

  RefPtr<ObjectHolder> holder(new ObjectHolder);
  holder->SetObject(obj);

  uintptr_t original = uintptr_t(obj.get());

  if (singleZone) {
    JS::PrepareZoneForGC(cx, js::GetContextZone(cx));
  } else {
    JS::PrepareForFullGC(cx);
  }

  JS::NonIncrementalGC(cx, JS::GCOptions::Shrink, JS::GCReason::DEBUG_GC);

  // Shrinking DEBUG_GC should move all GC things.
  ASSERT_NE(uintptr_t(holder->GetObject()), original);

  // Both root and holder should have been updated.
  ASSERT_EQ(obj, holder->GetObject());

  // Check it's the object we expect.
  value.setUndefined();
  ASSERT_TRUE(JS_GetProperty(cx, obj, PropertyName, &value));
  ASSERT_EQ(value, JS::Int32Value(PropertyValue));
}

TEST(JSHolderMap, GCIntegration)
{
  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  ASSERT_NE(ccjscx, nullptr);
  JSContext* cx = ccjscx->Context();
  ASSERT_NE(cx, nullptr);

  static const JSClass GlobalClass = {"global", JSCLASS_GLOBAL_FLAGS,
                                      &JS::DefaultGlobalClassOps};

  JS::RealmOptions options;
  JS::RootedObject global(cx);
  global = JS_NewGlobalObject(cx, &GlobalClass, nullptr,
                              JS::FireOnNewGlobalHook, options);
  ASSERT_NE(global, nullptr);

  JSAutoRealm ar(cx, global);

  TestHoldersAreMarkedGray(cx);
  TestHoldersAreMoved(cx, true);
  TestHoldersAreMoved(cx, false);
}
