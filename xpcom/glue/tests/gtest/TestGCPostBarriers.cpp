/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
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

#include "mozilla/CycleCollectedJSRuntime.h"

using namespace JS;
using namespace mozilla;

template <class ArrayT>
static void
TraceArray(JSTracer* trc, void* data)
{
  ArrayT* array = static_cast<ArrayT *>(data);
  for (unsigned i = 0; i < array->Length(); ++i)
    JS_CallHeapObjectTracer(trc, &array->ElementAt(i), "array-element");
}

/*
 * Use arrays with initial size much smaller than the final number of elements
 * to test that moving Heap<T> elements works correctly.
 */
const size_t ElementCount = 100;
const size_t InitialElements = ElementCount / 10;

template <class ArrayT>
static void
RunTest(JSRuntime* rt, JSContext* cx, ArrayT* array)
{
  JS_GC(rt);

  ASSERT_TRUE(array != nullptr);
  JS_AddExtraGCRootsTracer(rt, TraceArray<ArrayT>, array);

  /*
   * Create the array and fill it with new JS objects. With GGC these will be
   * allocated in the nursery.
   */
  RootedValue value(cx);
  const char* property = "foo";
  for (size_t i = 0; i < ElementCount; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
#ifdef JSGC_GENERATIONAL
    ASSERT_TRUE(js::gc::IsInsideNursery(AsCell(obj)));
#else
    ASSERT_FALSE(js::gc::IsInsideNursery(AsCell(obj)));
#endif
    value = Int32Value(i);
    ASSERT_TRUE(JS_SetProperty(cx, obj, property, value));
    array->AppendElement(obj);
  }

  /*
   * If postbarriers are not working, we will crash here when we try to mark
   * objects that have been moved to the tenured heap.
   */
  JS_GC(rt);

  /*
   * Sanity check that our array contains what we expect.
   */
  for (size_t i = 0; i < ElementCount; ++i) {
    RootedObject obj(cx, array->ElementAt(i));
    ASSERT_FALSE(js::gc::IsInsideNursery(AsCell(obj)));
    ASSERT_TRUE(JS_GetProperty(cx, obj, property, &value));
    ASSERT_TRUE(value.isInt32());
    ASSERT_EQ(static_cast<int32_t>(i), value.toInt32());
  }

  JS_RemoveExtraGCRootsTracer(rt, TraceArray<ArrayT>, array);
}

static void
CreateGlobalAndRunTest(JSRuntime* rt, JSContext* cx)
{
  static const JSClass GlobalClass = {
    "global", JSCLASS_GLOBAL_FLAGS,
    JS_PropertyStub, JS_DeletePropertyStub, JS_PropertyStub, JS_StrictPropertyStub,
    JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
    nullptr, nullptr, nullptr, nullptr,
    JS_GlobalObjectTraceHook
  };

  JS::CompartmentOptions options;
  options.setVersion(JSVERSION_LATEST);
  JS::PersistentRootedObject global(cx);
  global = JS_NewGlobalObject(cx, &GlobalClass, nullptr, JS::FireOnNewGlobalHook, options);
  ASSERT_TRUE(global != nullptr);

  JSCompartment *oldCompartment = JS_EnterCompartment(cx, global);

  typedef Heap<JSObject*> ElementT;

  {
    nsTArray<ElementT>* array = new nsTArray<ElementT>(InitialElements);
    RunTest(rt, cx, array);
    delete array;
  }

  {
    FallibleTArray<ElementT>* array = new FallibleTArray<ElementT>(InitialElements);
    RunTest(rt, cx, array);
    delete array;
  }

  {
    nsAutoTArray<ElementT, InitialElements> array;
    RunTest(rt, cx, &array);
  }

  {
    AutoFallibleTArray<ElementT, InitialElements> array;
    RunTest(rt, cx, &array);
  }

  JS_LeaveCompartment(cx, oldCompartment);
}

TEST(GCPostBarriers, nsTArray) {
  CycleCollectedJSRuntime* ccrt = CycleCollectedJSRuntime::Get();
  ASSERT_TRUE(ccrt != nullptr);
  JSRuntime* rt = ccrt->Runtime();
  ASSERT_TRUE(rt != nullptr);

  JSContext *cx = JS_NewContext(rt, 8192);
  ASSERT_TRUE(cx != nullptr);
  JS_BeginRequest(cx);

  CreateGlobalAndRunTest(rt, cx);

  JS_EndRequest(cx);
  JS_DestroyContext(cx);
}
