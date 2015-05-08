/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryProfiler.h"

#include <cmath>
#include <cstdlib>

#include "mozilla/Compiler.h"
#include "mozilla/Move.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/unused.h"

#include "GCHeapProfilerImpl.h"
#include "GeckoProfiler.h"
#include "NativeProfilerImpl.h"
#include "UncensoredAllocator.h"
#include "js/TypeDecls.h"
#include "jsfriendapi.h"
#include "nsIDOMClassInfo.h"
#include "nsIGlobalObject.h"
#include "prtime.h"
#include "xpcprivate.h"

struct JSRuntime;

#if MOZ_USING_STLPORT
namespace std {
template<class T>
struct hash<T*>
{
  size_t operator()(T* v) const
  {
    return hash<void*>()(static_cast<void*>(v));
  }
};
} // namespace std
#endif

namespace mozilla {

#define MEMORY_PROFILER_SAMPLE_SIZE 65536
#define BACKTRACE_BUFFER_SIZE 16384

ProfilerImpl::ProfilerImpl()
  : mSampleSize(MEMORY_PROFILER_SAMPLE_SIZE)
{
  mLog1minusP = std::log(1.0 - 1.0 / mSampleSize);
  mRemainingBytes = std::floor(std::log(1.0 - DRandom()) / mLog1minusP);
}

u_vector<u_string>
ProfilerImpl::GetStacktrace()
{
  u_vector<u_string> trace;
  char* output = (char*)u_malloc(BACKTRACE_BUFFER_SIZE);

  profiler_get_backtrace_noalloc(output, BACKTRACE_BUFFER_SIZE);
  for (const char* p = output; *p; p += strlen(p) + 1) {
    trace.push_back(p);
  }

  u_free(output);
  return trace;
}

// Generate a random number in [0, 1).
double
ProfilerImpl::DRandom()
{
  return double(((uint64_t(std::rand()) & ((1 << 26) - 1)) << 27) +
                (uint64_t(std::rand()) & ((1 << 27) - 1)))
    / (uint64_t(1) << 53);
}

size_t
ProfilerImpl::AddBytesSampled(uint32_t aBytes)
{
  size_t nSamples = 0;
  while (mRemainingBytes <= aBytes) {
    mRemainingBytes += std::floor(std::log(1.0 - DRandom()) / mLog1minusP);
    nSamples++;
  }
  mRemainingBytes -= aBytes;
  return nSamples;
}

NS_IMPL_ISUPPORTS(MemoryProfiler, nsIMemoryProfiler)

PRLock* MemoryProfiler::sLock;
uint32_t MemoryProfiler::sProfileRuntimeCount;
NativeProfilerImpl* MemoryProfiler::sNativeProfiler;
JSRuntimeProfilerMap* MemoryProfiler::sJSRuntimeProfilerMap;
TimeStamp MemoryProfiler::sStartTime;

void
MemoryProfiler::InitOnce()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool initialized = false;

  if (!initialized) {
    InitializeMallocHook();
    sLock = PR_NewLock();
    sProfileRuntimeCount = 0;
    sJSRuntimeProfilerMap = new JSRuntimeProfilerMap();
    std::srand(PR_Now());
    bool ignored;
    sStartTime = TimeStamp::ProcessCreation(ignored);
    initialized = true;
  }
}

NS_IMETHODIMP
MemoryProfiler::StartProfiler()
{
  InitOnce();
  JSRuntime* runtime = XPCJSRuntime::Get()->Runtime();
  AutoMPLock lock(sLock);
  if (!(*sJSRuntimeProfilerMap)[runtime].mEnabled) {
    (*sJSRuntimeProfilerMap)[runtime].mEnabled = true;
    if (sProfileRuntimeCount == 0) {
      js::EnableRuntimeProfilingStack(runtime, true);
      if (!sNativeProfiler) {
        sNativeProfiler = new NativeProfilerImpl();
      }
      MemProfiler::SetNativeProfiler(sNativeProfiler);
    }
    GCHeapProfilerImpl* gp = new GCHeapProfilerImpl();
    (*sJSRuntimeProfilerMap)[runtime].mProfiler = gp;
    MemProfiler::GetMemProfiler(runtime)->start(gp);
    if (sProfileRuntimeCount == 0) {
      EnableMallocHook(sNativeProfiler);
    }
    sProfileRuntimeCount++;
  }
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::StopProfiler()
{
  InitOnce();
  JSRuntime* runtime = XPCJSRuntime::Get()->Runtime();
  AutoMPLock lock(sLock);
  if ((*sJSRuntimeProfilerMap)[runtime].mEnabled) {
    MemProfiler::GetMemProfiler(runtime)->stop();
    if (--sProfileRuntimeCount == 0) {
      DisableMallocHook();
      MemProfiler::SetNativeProfiler(nullptr);
      js::EnableRuntimeProfilingStack(runtime, false);
    }
    (*sJSRuntimeProfilerMap)[runtime].mEnabled = false;
  }
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::ResetProfiler()
{
  InitOnce();
  JSRuntime* runtime = XPCJSRuntime::Get()->Runtime();
  AutoMPLock lock(sLock);
  if (!(*sJSRuntimeProfilerMap)[runtime].mEnabled) {
    delete (*sJSRuntimeProfilerMap)[runtime].mProfiler;
    (*sJSRuntimeProfilerMap)[runtime].mProfiler = nullptr;
  }
  if (sProfileRuntimeCount == 0) {
    delete sNativeProfiler;
    sNativeProfiler = nullptr;
  }
  return NS_OK;
}

struct MergedTraces
{
  u_vector<u_string> mNames;
  u_vector<TrieNode> mTraces;
  u_vector<AllocEvent> mEvents;
};

// Merge events and corresponding traces and names.
static MergedTraces
MergeResults(u_vector<u_string> names0, u_vector<TrieNode> traces0, u_vector<AllocEvent> events0,
             u_vector<u_string> names1, u_vector<TrieNode> traces1, u_vector<AllocEvent> events1)
{
  NodeIndexMap<u_string> names;
  NodeIndexMap<TrieNode> traces;
  u_vector<AllocEvent> events;

  u_vector<size_t> names1Tonames0;
  u_vector<size_t> traces1Totraces0(1, 0);

  // Merge names.
  for (auto& i: names0) {
    names.Insert(i);
  }
  for (auto& i: names1) {
    names1Tonames0.push_back(names.Insert(i));
  }

  // Merge traces. Note that traces1[i].parentIdx < i for all i > 0.
  for (auto& i: traces0) {
    traces.Insert(i);
  }
  for (size_t i = 1; i < traces1.size(); i++) {
    TrieNode node = traces1[i];
    node.parentIdx = traces1Totraces0[node.parentIdx];
    node.nameIdx = names1Tonames0[node.nameIdx];
    traces1Totraces0.push_back(traces.Insert(node));
  }

  // Update events1
  for (auto& i: events1) {
    i.mTraceIdx = traces1Totraces0[i.mTraceIdx];
  }

  // Merge the events according to timestamps.
  auto p0 = events0.begin();
  auto p1 = events1.begin();

  while (p0 != events0.end() && p1 != events1.end()) {
    if (p0->mTimestamp < p1->mTimestamp) {
      events.push_back(*p0++);
    } else {
      events.push_back(*p1++);
    }
  }

  while (p0 != events0.end()) {
    events.push_back(*p0++);
  }

  while (p1 != events1.end()) {
    events.push_back(*p1++);
  }

  return MergedTraces{names.Serialize(), traces.Serialize(), Move(events)};
}

NS_IMETHODIMP
MemoryProfiler::GetResults(JSContext* cx, JS::MutableHandle<JS::Value> aResult)
{
  InitOnce();
  JSRuntime* runtime = XPCJSRuntime::Get()->Runtime();
  AutoMPLock lock(sLock);
  // Getting results when the profiler is running is not allowed.
  if (sProfileRuntimeCount > 0) {
    return NS_OK;
  }
  // Return immediately when native profiler does not exist.
  if (!sNativeProfiler) {
    return NS_OK;
  }
  // Return immediately when there's no result in current runtime.
  if (!(*sJSRuntimeProfilerMap)[runtime].mProfiler) {
    return NS_OK;
  }
  GCHeapProfilerImpl* gp = (*sJSRuntimeProfilerMap)[runtime].mProfiler;

  auto results = MergeResults(gp->GetNames(), gp->GetTraces(), gp->GetEvents(),
                              sNativeProfiler->GetNames(),
                              sNativeProfiler->GetTraces(),
                              sNativeProfiler->GetEvents());
  u_vector<u_string> names = Move(results.mNames);
  u_vector<TrieNode> traces = Move(results.mTraces);
  u_vector<AllocEvent> events = Move(results.mEvents);

  JS::RootedObject jsnames(cx, JS_NewArrayObject(cx, names.size()));
  JS::RootedObject jstraces(cx, JS_NewArrayObject(cx, traces.size()));
  JS::RootedObject jsevents(cx, JS_NewArrayObject(cx, events.size()));

  for (size_t i = 0; i < names.size(); i++) {
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, names[i].c_str()));
    JS_SetElement(cx, jsnames, i, name);
  }

  for (size_t i = 0; i < traces.size(); i++) {
    JS::RootedObject tn(cx, JS_NewPlainObject(cx));
    JS::RootedValue nameIdx(cx, JS_NumberValue(traces[i].nameIdx));
    JS::RootedValue parentIdx(cx, JS_NumberValue(traces[i].parentIdx));
    JS_SetProperty(cx, tn, "nameIdx", nameIdx);
    JS_SetProperty(cx, tn, "parentIdx", parentIdx);
    JS_SetElement(cx, jstraces, i, tn);
  }

  int i = 0;
  for (auto ent: events) {
    if (ent.mSize == 0) {
      continue;
    }
    MOZ_ASSERT(!sStartTime.IsNull());
    double time = (sStartTime - ent.mTimestamp).ToMilliseconds();
    JS::RootedObject tn(cx, JS_NewPlainObject(cx));
    JS::RootedValue size(cx, JS_NumberValue(ent.mSize));
    JS::RootedValue traceIdx(cx, JS_NumberValue(ent.mTraceIdx));
    JS::RootedValue timestamp(cx, JS_NumberValue(time));
    JS_SetProperty(cx, tn, "size", size);
    JS_SetProperty(cx, tn, "traceIdx", traceIdx);
    JS_SetProperty(cx, tn, "timestamp", timestamp);
    JS_SetElement(cx, jsevents, i++, tn);
  }
  JS_SetArrayLength(cx, jsevents, i);

  JS::RootedObject result(cx, JS_NewPlainObject(cx));
  JS::RootedValue objnames(cx, ObjectOrNullValue(jsnames));
  JS_SetProperty(cx, result, "names", objnames);
  JS::RootedValue objtraces(cx, ObjectOrNullValue(jstraces));
  JS_SetProperty(cx, result, "traces", objtraces);
  JS::RootedValue objevents(cx, ObjectOrNullValue(jsevents));
  JS_SetProperty(cx, result, "events", objevents);
  aResult.setObject(*result);
  return NS_OK;
}

} // namespace mozilla
