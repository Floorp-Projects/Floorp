/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Tests that generational garbage collection post-barriers are correctly
 * implemented for nsTArrays that contain JavaScript Values.
 */

#include "jsapi.h"
#include "nsTArray.h"

#include "gtest/gtest.h"

#include "js/TracingAPI.h"
#include "js/HeapAPI.h"

#include "mozilla/CycleCollectedJSContext.h"

using namespace JS;
using namespace mozilla;

template<class ArrayT>
static void
TraceArray(JSTracer* trc, void* data)
{
  ArrayT* array = static_cast<ArrayT *>(data);
  for (unsigned i = 0; i < array->Length(); ++i)
    JS::TraceEdge(trc, &array->ElementAt(i), "array-element");
}

/*
 * Use arrays with initial size much smaller than the final number of elements
 * to test that moving Heap<T> elements works correctly.
 */
const size_t ElementCount = 100;
const size_t InitialElements = ElementCount / 10;

template<class ArrayT>
static void
RunTest(JSContext* cx, ArrayT* array)
{
  JS_GC(cx);

  ASSERT_TRUE(array != nullptr);
  JS_AddExtraGCRootsTracer(cx, TraceArray<ArrayT>, array);

  /*
   * Create the array and fill it with new JS objects. With GGC these will be
   * allocated in the nursery.
   */
  RootedValue value(cx);
  const char* property = "foo";
  for (size_t i = 0; i < ElementCount; ++i) {
    RootedObject obj(cx, JS_NewPlainObject(cx));
    ASSERT_FALSE(JS::ObjectIsTenured(obj));
    value = Int32Value(i);
    ASSERT_TRUE(JS_SetProperty(cx, obj, property, value));
    ASSERT_TRUE(array->AppendElement(obj, fallible));
  }

  /*
   * If postbarriers are not working, we will crash here when we try to mark
   * objects that have been moved to the tenured heap.
   */
  JS_GC(cx);

  /*
   * Sanity check that our array contains what we expect.
   */
  for (size_t i = 0; i < ElementCount; ++i) {
    RootedObject obj(cx, array->ElementAt(i));
    ASSERT_TRUE(JS::ObjectIsTenured(obj));
    ASSERT_TRUE(JS_GetProperty(cx, obj, property, &value));
    ASSERT_TRUE(value.isInt32());
    ASSERT_EQ(static_cast<int32_t>(i), value.toInt32());
  }

  JS_RemoveExtraGCRootsTracer(cx, TraceArray<ArrayT>, array);
}

static void
CreateGlobalAndRunTest(JSContext* cx)
{
  static const JSClassOps GlobalClassOps = {
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, JS_GlobalObjectTraceHook
  };

  static const JSClass GlobalClass = {
    "global", JSCLASS_GLOBAL_FLAGS,
    &GlobalClassOps
  };

  JS::RealmOptions options;
  JS::PersistentRootedObject global(cx);
  global = JS_NewGlobalObject(cx, &GlobalClass, nullptr, JS::FireOnNewGlobalHook, options);
  ASSERT_TRUE(global != nullptr);

  JS::Realm* oldRealm = JS::EnterRealm(cx, global);

  typedef Heap<JSObject*> ElementT;

  {
    nsTArray<ElementT>* array = new nsTArray<ElementT>(InitialElements);
    RunTest(cx, array);
    delete array;
  }

  {
    FallibleTArray<ElementT>* array = new FallibleTArray<ElementT>(InitialElements);
    RunTest(cx, array);
    delete array;
  }

  {
    AutoTArray<ElementT, InitialElements> array;
    RunTest(cx, &array);
  }

  JS::LeaveRealm(cx, oldRealm);
}

TEST(GCPostBarriers, nsTArray) {
  CycleCollectedJSContext* ccjscx = CycleCollectedJSContext::Get();
  ASSERT_TRUE(ccjscx != nullptr);
  JSContext* cx = ccjscx->Context();
  ASSERT_TRUE(cx != nullptr);

  JS_BeginRequest(cx);

  CreateGlobalAndRunTest(cx);

  JS_EndRequest(cx);
}
