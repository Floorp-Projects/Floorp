/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ThreadStackHelper.h"
#include "MainThreadUtils.h"
#include "nsJSPrincipals.h"
#include "nsScriptSecurityManager.h"
#include "jsfriendapi.h"
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
#include "js/ProfilingStack.h"
#endif

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Scoped.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/MemoryChecking.h"
#include "mozilla/Sprintf.h"
#include "nsThread.h"
#include "mozilla/HangStack.h"

#ifdef __GNUC__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wshadow"
#endif

#if defined(MOZ_VALGRIND)
# include <valgrind/valgrind.h>
#endif

#include <string.h>
#include <vector>
#include <cstdlib>

#ifdef XP_LINUX
#include <ucontext.h>
#include <unistd.h>
#include <sys/syscall.h>
#endif

#ifdef __GNUC__
# pragma GCC diagnostic pop // -Wshadow
#endif

#if defined(XP_LINUX) || defined(XP_MACOSX)
#include <pthread.h>
#endif

#ifdef ANDROID
#ifndef SYS_gettid
#define SYS_gettid __NR_gettid
#endif
#if defined(__arm__) && !defined(__NR_rt_tgsigqueueinfo)
// Some NDKs don't define this constant even though the kernel supports it.
#define __NR_rt_tgsigqueueinfo (__NR_SYSCALL_BASE+363)
#endif
#ifndef SYS_rt_tgsigqueueinfo
#define SYS_rt_tgsigqueueinfo __NR_rt_tgsigqueueinfo
#endif
#endif

namespace mozilla {

ThreadStackHelper::ThreadStackHelper()
  : mStackToFill(nullptr)
  , mMaxStackSize(HangStack::sMaxInlineStorage)
  , mMaxBufferSize(512)
  , mDesiredStackSize(0)
  , mDesiredBufferSize(0)
{
  mThreadId = profiler_current_thread_id();
}

bool
ThreadStackHelper::PrepareStackBuffer(HangStack& aStack)
{
  // If we need to grow because we used more than we could store last time,
  // increase our maximum sizes for this time.
  if (mDesiredBufferSize > mMaxBufferSize) {
    mMaxBufferSize = mDesiredBufferSize;
  }
  if (mDesiredStackSize > mMaxStackSize) {
    mMaxStackSize = mDesiredStackSize;
  }
  mDesiredBufferSize = 0;
  mDesiredStackSize = 0;

  // Return false to skip getting the stack and return an empty stack
  aStack.clear();
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  if (!aStack.reserve(mMaxStackSize) ||
      !aStack.reserve(aStack.capacity()) || // reserve up to the capacity
      !aStack.EnsureBufferCapacity(mMaxBufferSize)) {
    return false;
  }
  return true;
#else
  return false;
#endif
}

namespace {
template<typename T>
class ScopedSetPtr
{
private:
  T*& mPtr;
public:
  ScopedSetPtr(T*& p, T* val) : mPtr(p) { mPtr = val; }
  ~ScopedSetPtr() { mPtr = nullptr; }
};
} // namespace

void
ThreadStackHelper::GetStack(HangStack& aStack, nsACString& aRunnableName, bool aStackWalk)
{
  aRunnableName.AssignLiteral("???");

  if (!PrepareStackBuffer(aStack)) {
    return;
  }

  Array<char, nsThread::kRunnableNameBufSize> runnableName;
  runnableName[0] = '\0';

  ScopedSetPtr<HangStack> _stackGuard(mStackToFill, &aStack);
  ScopedSetPtr<Array<char, nsThread::kRunnableNameBufSize>>
    _runnableGuard(mRunnableNameBuffer, &runnableName);

  // XXX: We don't need to pass in ProfilerFeature::StackWalk to trigger
  // stackwalking, as that is instead controlled by the last argument.
  profiler_suspend_and_sample_thread(
    mThreadId, ProfilerFeature::Privacy, *this, aStackWalk);

  // Copy the name buffer allocation into the output string. We explicitly set
  // the last byte to null in case we read in some corrupted data without a null
  // terminator.
  runnableName[nsThread::kRunnableNameBufSize - 1] = '\0';
  aRunnableName.AssignASCII(runnableName.cbegin());
}

void
ThreadStackHelper::SetIsMainThread()
{
  MOZ_RELEASE_ASSERT(mRunnableNameBuffer);

  // NOTE: We cannot allocate any memory in this callback, as the target
  // thread is suspended, so we first copy it into a stack-allocated buffer,
  // and then once the target thread is resumed, we can copy it into a real
  // nsCString.
  //
  // Currently we only store the names of runnables which are running on the
  // main thread, so we only want to read sMainThreadRunnableName and copy its
  // value in the case that we are currently suspending the main thread.
  *mRunnableNameBuffer = nsThread::sMainThreadRunnableName;
}

void
ThreadStackHelper::TryAppendFrame(HangStack::Frame aFrame)
{
  MOZ_RELEASE_ASSERT(mStackToFill);

  // We deduplicate identical frames of kind CONTENT, JIT, WASM, and SUPPRESSED.
  switch (aFrame.GetKind()) {
    case HangStack::Frame::Kind::CONTENT:
    case HangStack::Frame::Kind::JIT:
    case HangStack::Frame::Kind::WASM:
    case HangStack::Frame::Kind::SUPPRESSED:
      if (!mStackToFill->empty() && mStackToFill->back().GetKind() == aFrame.GetKind()) {
        return;
      }
      break;
    default:
      break;
  }

  // Record that we _want_ to use another frame entry. If this exceeds
  // mMaxStackSize, we'll allocate more room on the next hang.
  mDesiredStackSize += 1;

  // Perform the append if we have enough space to do so.
  if (mStackToFill->canAppendWithoutRealloc(1)) {
    mStackToFill->infallibleAppend(aFrame);
  }
}

void
ThreadStackHelper::CollectNativeLeafAddr(void* aAddr)
{
  MOZ_RELEASE_ASSERT(mStackToFill);
  TryAppendFrame(HangStack::Frame(reinterpret_cast<uintptr_t>(aAddr)));
}

void
ThreadStackHelper::CollectJitReturnAddr(void* aAddr)
{
  MOZ_RELEASE_ASSERT(mStackToFill);
  TryAppendFrame(HangStack::Frame::Jit());
}

void
ThreadStackHelper::CollectWasmFrame(const char* aLabel)
{
  MOZ_RELEASE_ASSERT(mStackToFill);
  // We don't want to collect WASM frames, as they are probably for content, so
  // we just add a "(content wasm)" frame.
  TryAppendFrame(HangStack::Frame::Wasm());
}

namespace {

bool
IsChromeJSScript(JSScript* aScript)
{
  // May be called from another thread or inside a signal handler.
  // We assume querying the script is safe but we must not manipulate it.
  nsIScriptSecurityManager* const secman =
    nsScriptSecurityManager::GetScriptSecurityManager();
  NS_ENSURE_TRUE(secman, false);

  JSPrincipals* const principals = JS_GetScriptPrincipals(aScript);
  return secman->IsSystemPrincipal(nsJSPrincipals::get(principals));
}

// Get the full path after the URI scheme, if the URI matches the scheme.
// For example, GetFullPathForScheme("a://b/c/d/e", "a://") returns "b/c/d/e".
template <size_t LEN>
const char*
GetFullPathForScheme(const char* filename, const char (&scheme)[LEN]) {
  // Account for the null terminator included in LEN.
  if (!strncmp(filename, scheme, LEN - 1)) {
    return filename + LEN - 1;
  }
  return nullptr;
}

// Get the full path after a URI component, if the URI contains the component.
// For example, GetPathAfterComponent("a://b/c/d/e", "/c/") returns "d/e".
template <size_t LEN>
const char*
GetPathAfterComponent(const char* filename, const char (&component)[LEN]) {
  const char* found = nullptr;
  const char* next = strstr(filename, component);
  while (next) {
    // Move 'found' to end of the component, after the separator '/'.
    // 'LEN - 1' accounts for the null terminator included in LEN,
    found = next + LEN - 1;
    // Resume searching before the separator '/'.
    next = strstr(found - 1, component);
  }
  return found;
}

} // namespace

void
ThreadStackHelper::CollectPseudoEntry(const js::ProfileEntry& aEntry)
{
  // For non-js frames we just include the raw label.
  if (!aEntry.isJs()) {
    const char* label = aEntry.label();
    TryAppendFrame(HangStack::Frame(label));
    return;
  }

  if (!aEntry.script()) {
    TryAppendFrame(HangStack::Frame::Suppressed());
    return;
  }

  if (!IsChromeJSScript(aEntry.script())) {
    TryAppendFrame(HangStack::Frame::Content());
    return;
  }

  // Rather than using the profiler's dynamic string, we compute our own string.
  // This is because we want to do some size-saving strategies, and throw out
  // information which won't help us as much.
  // XXX: We currently don't collect the function name which hung.
  const char* filename = JS_GetScriptFilename(aEntry.script());
  unsigned lineno = JS_PCToLineNumber(aEntry.script(), aEntry.pc());

  // Some script names are in the form "foo -> bar -> baz".
  // Here we find the origin of these redirected scripts.
  const char* basename = GetPathAfterComponent(filename, " -> ");
  if (basename) {
    filename = basename;
  }

  // Strip chrome:// or resource:// off of the filename if present.
  basename = GetFullPathForScheme(filename, "chrome://");
  if (!basename) {
    basename = GetFullPathForScheme(filename, "resource://");
  }
  if (!basename) {
    // If we're in an add-on script, under the {profile}/extensions
    // directory, extract the path after the /extensions/ part.
    basename = GetPathAfterComponent(filename, "/extensions/");
  }
  if (!basename) {
    // Only keep the file base name for paths outside the above formats.
    basename = strrchr(filename, '/');
    basename = basename ? basename + 1 : filename;
    // Look for Windows path separator as well.
    filename = strrchr(basename, '\\');
    if (filename) {
      basename = filename + 1;
    }
  }

  mDesiredStackSize += 1;
  char buffer[128]; // Enough to fit longest js file name from the tree
  size_t len = SprintfLiteral(buffer, "%s:%u", basename, lineno);
  if (len < sizeof(buffer)) {
    mDesiredBufferSize += len + 1;
    if (mStackToFill->canAppendWithoutRealloc(1) &&
        mStackToFill->AvailableBufferSize() >= len + 1) {
      mStackToFill->InfallibleAppendViaBuffer(buffer, len);
      return;
    }
  }

  if (mStackToFill->canAppendWithoutRealloc(1)) {
    mStackToFill->infallibleAppend(HangStack::Frame("(chrome script)"));
  }
}

} // namespace mozilla
