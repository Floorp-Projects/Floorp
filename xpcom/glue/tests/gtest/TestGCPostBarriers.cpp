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

#include "js/Tracer.h"
#include "js/HeapAPI.h"

#include "mozilla/CycleCollectedJSRuntime.h"

using namespace JS;
using namespace mozilla;

typedef nsTArray<Heap<JSObject*> > ArrayT;

static void
TraceArray(JSTracer* trc, void* data)
{
  ArrayT* array = static_cast<ArrayT *>(data);
  for (unsigned i = 0; i < array->Length(); ++i)
    JS_CallHeapObjectTracer(trc, &array->ElementAt(i), "array-element");
}

static void
RunTest(JSRuntime* rt, JSContext* cx)
{
  JS_GC(rt);

  /*
   * Create an array with space for half the final number of elements to test
   * that moving Heap<T> elements works correctly.
   */
  const int elements = 100;
  ArrayT* array = new ArrayT(elements / 2);
  ASSERT_TRUE(array != nullptr);
  JS_AddExtraGCRootsTracer(rt, TraceArray, array);

  /*
   * Create the array and fill it with new JS objects. With GGC these will be
   * allocated in the nursery.
   */
  RootedValue value(cx);
  const char* property = "foo";
  JS::shadow::Runtime* srt = reinterpret_cast<JS::shadow::Runtime*>(rt);
  for (int i = 0; i < elements; ++i) {
    RootedObject obj(cx, JS_NewObject(cx, nullptr, JS::NullPtr(), JS::NullPtr()));
#ifdef JSGC_GENERATIONAL
    ASSERT_TRUE(js::gc::IsInsideNursery(srt, obj));
#else
    ASSERT_FALSE(js::gc::IsInsideNursery(srt, obj));
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
  for (int i = 0; i < elements; ++i) {
    RootedObject obj(cx, array->ElementAt(i));
    ASSERT_FALSE(js::gc::IsInsideNursery(srt, obj));
    ASSERT_TRUE(JS_GetProperty(cx, obj, property, &value));
    ASSERT_TRUE(value.isInt32());
    ASSERT_EQ(i, value.toInt32());
  }

  JS_RemoveExtraGCRootsTracer(rt, TraceArray, array);
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
  JS::RootedObject global(cx);
  global = JS_NewGlobalObject(cx, &GlobalClass, nullptr, JS::FireOnNewGlobalHook, options);
  ASSERT_TRUE(global != nullptr);

  JS_AddNamedObjectRoot(cx, global.address(), "test-global");
  JSCompartment *oldCompartment = JS_EnterCompartment(cx, global);

  RunTest(rt, cx);

  JS_LeaveCompartment(cx, oldCompartment);
  JS_RemoveObjectRoot(cx, global.address());
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
