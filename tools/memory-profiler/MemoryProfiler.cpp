/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MemoryProfiler.h"

#include <cmath>
#include <cstdlib>

#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Move.h"
#include "mozilla/TimeStamp.h"
#include "mozilla/UniquePtr.h"

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

namespace mozilla {

#define MEMORY_PROFILER_SAMPLE_SIZE 65536
#define BACKTRACE_BUFFER_SIZE 16384

ProfilerImpl::ProfilerImpl()
  : mSampleSize(MEMORY_PROFILER_SAMPLE_SIZE)
{
  mLog1minusP = std::log(1.0 - 1.0 / mSampleSize);
  mRemainingBytes = std::floor(std::log(1.0 - DRandom()) / mLog1minusP);
}

nsTArray<nsCString>
ProfilerImpl::GetStacktrace()
{
  nsTArray<nsCString> trace;
  auto output = MakeUnique<char[]>(BACKTRACE_BUFFER_SIZE);

  profiler_get_backtrace_noalloc(output.get(), BACKTRACE_BUFFER_SIZE);
  for (const char* p = output.get(); *p; p += strlen(p) + 1) {
    trace.AppendElement()->Assign(p);
  }

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
uint32_t MemoryProfiler::sProfileContextCount;
StaticAutoPtr<NativeProfilerImpl> MemoryProfiler::sNativeProfiler;
StaticAutoPtr<JSContextProfilerMap> MemoryProfiler::sJSContextProfilerMap;
TimeStamp MemoryProfiler::sStartTime;

void
MemoryProfiler::InitOnce()
{
  MOZ_ASSERT(NS_IsMainThread());

  static bool initialized = false;

  if (!initialized) {
    MallocHook::Initialize();
    sLock = PR_NewLock();
    sProfileContextCount = 0;
    sJSContextProfilerMap = new JSContextProfilerMap();
    ClearOnShutdown(&sJSContextProfilerMap);
    ClearOnShutdown(&sNativeProfiler);
    std::srand(PR_Now());
    sStartTime = TimeStamp::ProcessCreation();
    initialized = true;
  }
}

NS_IMETHODIMP
MemoryProfiler::StartProfiler()
{
  InitOnce();
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(sLock);
  JSContext* context = XPCJSContext::Get()->Context();
  ProfilerForJSContext profiler;
  if (!sJSContextProfilerMap->Get(context, &profiler) ||
      !profiler.mEnabled) {
    if (sProfileContextCount == 0) {
      js::EnableContextProfilingStack(context, true);
      if (!sNativeProfiler) {
        sNativeProfiler = new NativeProfilerImpl();
      }
      MemProfiler::SetNativeProfiler(sNativeProfiler);
    }
    GCHeapProfilerImpl* gp = new GCHeapProfilerImpl();
    profiler.mEnabled = true;
    profiler.mProfiler = gp;
    sJSContextProfilerMap->Put(context, profiler);
    MemProfiler::GetMemProfiler(context)->start(gp);
    if (sProfileContextCount == 0) {
      MallocHook::Enable(sNativeProfiler);
    }
    sProfileContextCount++;
  }
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::StopProfiler()
{
  InitOnce();
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(sLock);
  JSContext* context = XPCJSContext::Get()->Context();
  ProfilerForJSContext profiler;
  if (sJSContextProfilerMap->Get(context, &profiler) &&
      profiler.mEnabled) {
    MemProfiler::GetMemProfiler(context)->stop();
    if (--sProfileContextCount == 0) {
      MallocHook::Disable();
      MemProfiler::SetNativeProfiler(nullptr);
      js::EnableContextProfilingStack(context, false);
    }
    profiler.mEnabled = false;
    sJSContextProfilerMap->Put(context, profiler);
  }
  return NS_OK;
}

NS_IMETHODIMP
MemoryProfiler::ResetProfiler()
{
  InitOnce();
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(sLock);
  JSContext* context = XPCJSContext::Get()->Context();
  ProfilerForJSContext profiler;
  if (!sJSContextProfilerMap->Get(context, &profiler) ||
      !profiler.mEnabled) {
    delete profiler.mProfiler;
    profiler.mProfiler = nullptr;
    sJSContextProfilerMap->Put(context, profiler);
  }
  if (sProfileContextCount == 0) {
    sNativeProfiler = nullptr;
  }
  return NS_OK;
}

struct MergedTraces
{
  nsTArray<nsCString> mNames;
  nsTArray<TrieNode> mTraces;
  nsTArray<AllocEvent> mEvents;
};

// Merge events and corresponding traces and names.
static MergedTraces
MergeResults(const nsTArray<nsCString>& names0,
             const nsTArray<TrieNode>& traces0,
             const nsTArray<AllocEvent>& events0,
             const nsTArray<nsCString>& names1,
             const nsTArray<TrieNode>& traces1,
             const nsTArray<AllocEvent>& events1)
{
  NodeIndexMap<nsCStringHashKey, nsCString> names;
  NodeIndexMap<nsGenericHashKey<TrieNode>, TrieNode> traces;
  nsTArray<AllocEvent> events;

  nsTArray<size_t> names1Tonames0(names1.Length());
  nsTArray<size_t> traces1Totraces0(traces1.Length());

  // Merge names.
  for (auto& i: names0) {
    names.Insert(i);
  }
  for (auto& i: names1) {
    names1Tonames0.AppendElement(names.Insert(i));
  }

  // Merge traces. Note that traces1[i].parentIdx < i for all i > 0.
  for (auto& i: traces0) {
    traces.Insert(i);
  }
  traces1Totraces0.AppendElement(0);
  for (size_t i = 1; i < traces1.Length(); i++) {
    TrieNode node = traces1[i];
    node.parentIdx = traces1Totraces0[node.parentIdx];
    node.nameIdx = names1Tonames0[node.nameIdx];
    traces1Totraces0.AppendElement(traces.Insert(node));
  }

  // Merge the events according to timestamps.
  auto p0 = events0.begin();
  auto p1 = events1.begin();

  while (p0 != events0.end() && p1 != events1.end()) {
    if (p0->mTimestamp < p1->mTimestamp) {
      events.AppendElement(*p0++);
    } else {
      events.AppendElement(*p1++);
      events.LastElement().mTraceIdx =
        traces1Totraces0[events.LastElement().mTraceIdx];
    }
  }

  while (p0 != events0.end()) {
    events.AppendElement(*p0++);
  }

  while (p1 != events1.end()) {
    events.AppendElement(*p1++);
    events.LastElement().mTraceIdx =
      traces1Totraces0[events.LastElement().mTraceIdx];
  }

  return MergedTraces{names.Serialize(), traces.Serialize(), Move(events)};
}

NS_IMETHODIMP
MemoryProfiler::GetResults(JSContext* cx, JS::MutableHandle<JS::Value> aResult)
{
  InitOnce();
  AutoUseUncensoredAllocator ua;
  AutoMPLock lock(sLock);
  JSContext* context = XPCJSContext::Get()->Context();
  // Getting results when the profiler is running is not allowed.
  if (sProfileContextCount > 0) {
    return NS_OK;
  }
  // Return immediately when native profiler does not exist.
  if (!sNativeProfiler) {
    return NS_OK;
  }
  // Return immediately when there's no result in current context.
  ProfilerForJSContext profiler;
  if (!sJSContextProfilerMap->Get(context, &profiler) ||
      !profiler.mProfiler) {
    return NS_OK;
  }
  GCHeapProfilerImpl* gp = profiler.mProfiler;

  auto results = MergeResults(gp->GetNames(), gp->GetTraces(), gp->GetEvents(),
                              sNativeProfiler->GetNames(),
                              sNativeProfiler->GetTraces(),
                              sNativeProfiler->GetEvents());
  const nsTArray<nsCString>& names = results.mNames;
  const nsTArray<TrieNode>& traces = results.mTraces;
  const nsTArray<AllocEvent>& events = results.mEvents;

  JS::RootedObject jsnames(cx, JS_NewArrayObject(cx, names.Length()));
  JS::RootedObject jstraces(cx, JS_NewArrayObject(cx, traces.Length()));
  JS::RootedObject jsevents(cx, JS_NewArrayObject(cx, events.Length()));

  for (size_t i = 0; i < names.Length(); i++) {
    JS::RootedString name(cx, JS_NewStringCopyZ(cx, names[i].get()));
    JS_SetElement(cx, jsnames, i, name);
  }

  for (size_t i = 0; i < traces.Length(); i++) {
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
    double time = (ent.mTimestamp - sStartTime).ToMilliseconds();
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
