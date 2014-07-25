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
#include "prprf.h"
#include "shared-libraries.h"

#include "js/OldDebugAPI.h"

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Move.h"
#include "mozilla/Scoped.h"

#include "google_breakpad/processor/call_stack.h"
#include "google_breakpad/processor/basic_source_line_resolver.h"
#include "google_breakpad/processor/stack_frame_cpu.h"
#include "processor/basic_code_module.h"
#include "processor/basic_code_modules.h"

#if defined(MOZ_THREADSTACKHELPER_X86)
#include "processor/stackwalker_x86.h"
#elif defined(MOZ_THREADSTACKHELPER_X64)
#include "processor/stackwalker_amd64.h"
#elif defined(MOZ_THREADSTACKHELPER_ARM)
#include "processor/stackwalker_arm.h"
#endif

#include <string.h>
#include <vector>

#ifdef XP_LINUX
#include <unistd.h>
#include <sys/syscall.h>
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

#if defined(MOZ_THREADSTACKHELPER_X86) || \
    defined(MOZ_THREADSTACKHELPER_X64) || \
    defined(MOZ_THREADSTACKHELPER_ARM)
// On these architectures, the stack grows downwards (toward lower addresses).
#define MOZ_THREADSTACKHELPER_STACK_GROWS_DOWN
#else
#error "Unsupported architecture"
#endif

namespace mozilla {

void
ThreadStackHelper::Startup()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (!sInitialized) {
    // TODO: centralize signal number allocation
    sFillStackSignum = SIGRTMIN + 4;
    if (sFillStackSignum > SIGRTMAX) {
      // Leave uninitialized
      MOZ_ASSERT(false);
      return;
    }
    struct sigaction sigact = {};
    sigact.sa_sigaction = FillStackHandler;
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = SA_SIGINFO | SA_RESTART;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
  }
  sInitialized++;
#endif
}

void
ThreadStackHelper::Shutdown()
{
#if defined(XP_LINUX)
  MOZ_ASSERT(NS_IsMainThread());
  if (sInitialized == 1) {
    struct sigaction sigact = {};
    sigact.sa_handler = SIG_DFL;
    MOZ_ALWAYS_TRUE(!::sigaction(sFillStackSignum, &sigact, nullptr));
  }
  sInitialized--;
#endif
}

ThreadStackHelper::ThreadStackHelper()
  : mStackToFill(nullptr)
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  , mPseudoStack(mozilla_get_pseudo_stack())
#endif
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  , mContextToFill(nullptr)
#endif
  , mMaxStackSize(Stack::sMaxInlineStorage)
  , mMaxBufferSize(0)
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_init(&mSem, 0, 0));
  mThreadID = ::syscall(SYS_gettid);
#elif defined(XP_WIN)
  mInitialized = !!::DuplicateHandle(
    ::GetCurrentProcess(), ::GetCurrentThread(),
    ::GetCurrentProcess(), &mThreadID,
    THREAD_SUSPEND_RESUME, FALSE, 0);
  MOZ_ASSERT(mInitialized);
#elif defined(XP_MACOSX)
  mThreadID = mach_thread_self();
#endif

#ifdef MOZ_THREADSTACKHELPER_NATIVE
  GetThreadStackBase();
#endif
}

ThreadStackHelper::~ThreadStackHelper()
{
#if defined(XP_LINUX)
  MOZ_ALWAYS_TRUE(!::sem_destroy(&mSem));
#elif defined(XP_WIN)
  if (mInitialized) {
    MOZ_ALWAYS_TRUE(!!::CloseHandle(mThreadID));
  }
#endif
}

#ifdef MOZ_THREADSTACKHELPER_NATIVE
void ThreadStackHelper::GetThreadStackBase()
{
  mThreadStackBase = 0;

  // TODO get stack base for each platform
}
#endif // MOZ_THREADSTACKHELPER_NATIVE

namespace {
template <typename T>
class ScopedSetPtr
{
private:
  T*& mPtr;
public:
  ScopedSetPtr(T*& p, T* val) : mPtr(p) { mPtr = val; }
  ~ScopedSetPtr() { mPtr = nullptr; }
};
}

void
ThreadStackHelper::GetStack(Stack& aStack)
{
  // Always run PrepareStackBuffer first to clear aStack
  if (!PrepareStackBuffer(aStack)) {
    // Skip and return empty aStack
    return;
  }

  ScopedSetPtr<Stack> stackPtr(mStackToFill, &aStack);

#if defined(XP_LINUX)
  if (!sInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  siginfo_t uinfo = {};
  uinfo.si_signo = sFillStackSignum;
  uinfo.si_code = SI_QUEUE;
  uinfo.si_pid = getpid();
  uinfo.si_uid = getuid();
  uinfo.si_value.sival_ptr = this;
  if (::syscall(SYS_rt_tgsigqueueinfo, uinfo.si_pid,
                mThreadID, sFillStackSignum, &uinfo)) {
    // rt_tgsigqueueinfo was added in Linux 2.6.31.
    // Could have failed because the syscall did not exist.
    return;
  }
  MOZ_ALWAYS_TRUE(!::sem_wait(&mSem));

#elif defined(XP_WIN)
  if (!mInitialized) {
    MOZ_ASSERT(false);
    return;
  }
  if (::SuspendThread(mThreadID) == DWORD(-1)) {
    MOZ_ASSERT(false);
    return;
  }

  FillStackBuffer();
  FillThreadContext();

  MOZ_ALWAYS_TRUE(::ResumeThread(mThreadID) != DWORD(-1));

#elif defined(XP_MACOSX)
  if (::thread_suspend(mThreadID) != KERN_SUCCESS) {
    MOZ_ASSERT(false);
    return;
  }

  FillStackBuffer();
  FillThreadContext();

  MOZ_ALWAYS_TRUE(::thread_resume(mThreadID) == KERN_SUCCESS);

#endif
}

#ifdef MOZ_THREADSTACKHELPER_NATIVE
class ThreadStackHelper::CodeModulesProvider
  : public google_breakpad::CodeModules {
private:
  typedef google_breakpad::CodeModule CodeModule;
  typedef google_breakpad::BasicCodeModule BasicCodeModule;

  const SharedLibraryInfo mLibs;
  mutable ScopedDeletePtr<BasicCodeModule> mModule;

public:
  CodeModulesProvider() : mLibs(SharedLibraryInfo::GetInfoForSelf()) {}
  virtual ~CodeModulesProvider() {}

  virtual unsigned int module_count() const {
    return mLibs.GetSize();
  }

  virtual const CodeModule* GetModuleForAddress(uint64_t address) const {
    MOZ_CRASH("Not implemented");
  }

  virtual const CodeModule* GetMainModule() const {
    return nullptr;
  }

  virtual const CodeModule* GetModuleAtSequence(unsigned int sequence) const {
    MOZ_CRASH("Not implemented");
  }

  virtual const CodeModule* GetModuleAtIndex(unsigned int index) const {
    const SharedLibrary& lib = mLibs.GetEntry(index);
    mModule = new BasicCodeModule(lib.GetStart(), lib.GetEnd() - lib.GetStart(),
                                  lib.GetName(), lib.GetBreakpadId(),
                                  lib.GetName(), lib.GetBreakpadId(), "");
    // Keep mModule valid until the next GetModuleAtIndex call.
    return mModule;
  }

  virtual const CodeModules* Copy() const {
    MOZ_CRASH("Not implemented");
  }
};

class ThreadStackHelper::ThreadContext
  : public google_breakpad::MemoryRegion {
public:
#if defined(MOZ_THREADSTACKHELPER_X86)
  typedef MDRawContextX86 Context;
#elif defined(MOZ_THREADSTACKHELPER_X64)
  typedef MDRawContextAMD64 Context;
#elif defined(MOZ_THREADSTACKHELPER_ARM)
  typedef MDRawContextARM Context;
#endif
  // Limit copied stack to 4kB
  static const size_t kMaxStackSize = 0x1000;
  // Limit unwound stack to 32 frames
  static const unsigned int kMaxStackFrames = 32;
  // Whether this structure contains valid data
  bool mValid;
  // Processor context
  Context mContext;
  // Stack area
  ScopedDeleteArray<uint8_t> mStack;
  // Start of stack area
  uintptr_t mStackBase;
  // Size of stack area
  size_t mStackSize;
  // End of stack area
  const void* mStackEnd;

  ThreadContext() : mValid(false)
                  , mStackBase(0)
                  , mStackSize(0)
                  , mStackEnd(nullptr) {}
  virtual ~ThreadContext() {}

  virtual uint64_t GetBase() const {
    return uint64_t(mStackBase);
  }
  virtual uint32_t GetSize() const {
    return mStackSize;
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint8_t*  value) const {
    return GetMemoryAtAddressInternal(address, value);
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint16_t*  value) const {
    return GetMemoryAtAddressInternal(address, value);
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint32_t*  value) const {
    return GetMemoryAtAddressInternal(address, value);
  }
  virtual bool GetMemoryAtAddress(uint64_t address, uint64_t*  value) const {
    return GetMemoryAtAddressInternal(address, value);
  }

private:
  template <typename T>
  bool GetMemoryAtAddressInternal(uint64_t address, T* value) const {
    const intptr_t offset = intptr_t(address) - intptr_t(GetBase());
    if (offset < 0 || uintptr_t(offset) > (GetSize() - sizeof(T))) {
      return false;
    }
    *value = *reinterpret_cast<const T*>(&mStack[offset]);
    return true;
  }
};
#endif // MOZ_THREADSTACKHELPER_NATIVE

void
ThreadStackHelper::GetNativeStack(Stack& aStack)
{
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  ThreadContext context;
  context.mStack = new uint8_t[ThreadContext::kMaxStackSize];

  ScopedSetPtr<ThreadContext> contextPtr(mContextToFill, &context);

  // Get pseudostack first and fill the thread context.
  GetStack(aStack);
  NS_ENSURE_TRUE_VOID(context.mValid);

  CodeModulesProvider modulesProvider;
  google_breakpad::BasicCodeModules modules(&modulesProvider);
  google_breakpad::BasicSourceLineResolver resolver;
  google_breakpad::StackFrameSymbolizer symbolizer(nullptr, &resolver);

#if defined(MOZ_THREADSTACKHELPER_X86)
  google_breakpad::StackwalkerX86 stackWalker(
    nullptr, &context.mContext, &context, &modules, &symbolizer);
#elif defined(MOZ_THREADSTACKHELPER_X64)
  google_breakpad::StackwalkerAMD64 stackWalker(
    nullptr, &context.mContext, &context, &modules, &symbolizer);
#elif defined(MOZ_THREADSTACKHELPER_ARM)
  google_breakpad::StackwalkerARM stackWalker(
    nullptr, &context.mContext, -1, &context, &modules, &symbolizer);
#else
  #error "Unsupported architecture"
#endif

  google_breakpad::CallStack callStack;
  std::vector<const google_breakpad::CodeModule*> modules_without_symbols;

  google_breakpad::Stackwalker::set_max_frames(ThreadContext::kMaxStackFrames);
  google_breakpad::Stackwalker::
    set_max_frames_scanned(ThreadContext::kMaxStackFrames);

  NS_ENSURE_TRUE_VOID(stackWalker.Walk(&callStack, &modules_without_symbols));

  const std::vector<google_breakpad::StackFrame*>& frames(*callStack.frames());
  for (intptr_t i = frames.size() - 1; i >= 0; i--) {
    const google_breakpad::StackFrame& frame = *frames[i];
    if (!frame.module) {
      continue;
    }
    const string& module = frame.module->code_file();
#if defined(XP_LINUX) || defined(XP_MACOSX)
    const char PATH_SEP = '/';
#elif defined(XP_WIN)
    const char PATH_SEP = '\\';
#endif
    const char* const module_basename = strrchr(module.c_str(), PATH_SEP);
    const char* const module_name = module_basename ?
                                    module_basename + 1 : module.c_str();

    char buffer[0x100];
    size_t len = 0;
    if (!frame.function_name.empty()) {
      len = PR_snprintf(buffer, sizeof(buffer), "%s:%s",
                        module_name, frame.function_name.c_str());
    } else {
      len = PR_snprintf(buffer, sizeof(buffer), "%s:0x%p",
                        module_name, (intptr_t)
                        (frame.instruction - frame.module->base_address()));
    }
    if (len) {
      aStack.AppendViaBuffer(buffer, len);
    }
  }
#endif // MOZ_THREADSTACKHELPER_NATIVE
}

#ifdef XP_LINUX

int ThreadStackHelper::sInitialized;
int ThreadStackHelper::sFillStackSignum;

void
ThreadStackHelper::FillStackHandler(int aSignal, siginfo_t* aInfo,
                                    void* aContext)
{
  ThreadStackHelper* const helper =
    reinterpret_cast<ThreadStackHelper*>(aInfo->si_value.sival_ptr);
  helper->FillStackBuffer();
  helper->FillThreadContext(aContext);
  ::sem_post(&helper->mSem);
}

#endif // XP_LINUX

bool
ThreadStackHelper::PrepareStackBuffer(Stack& aStack)
{
  // Return false to skip getting the stack and return an empty stack
  aStack.clear();
#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  /* Normally, provided the profiler is enabled, it would be an error if we
     don't have a pseudostack here (the thread probably forgot to call
     profiler_register_thread). However, on B2G, profiling secondary threads
     may be disabled despite profiler being enabled. This is by-design and
     is not an error. */
#ifdef MOZ_WIDGET_GONK
  if (!mPseudoStack) {
    return false;
  }
#endif
  MOZ_ASSERT(mPseudoStack);
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

#ifdef MOZ_THREADSTACKHELPER_PSEUDO

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

} // namespace

const char*
ThreadStackHelper::AppendJSEntry(const volatile StackEntry* aEntry,
                                 intptr_t& aAvailableBufferSize,
                                 const char* aPrevLabel)
{
  // May be called from another thread or inside a signal handler.
  // We assume querying the script is safe but we must not manupulate it.
  // Also we must not allocate any memory from heap.
  MOZ_ASSERT(aEntry->isJs());
  MOZ_ASSERT(aEntry->script());

  const char* label;
  if (IsChromeJSScript(aEntry->script())) {
    const char* const filename = JS_GetScriptFilename(aEntry->script());
    unsigned lineno = JS_PCToLineNumber(nullptr, aEntry->script(),
                                        aEntry->pc());
    MOZ_ASSERT(filename);

    char buffer[64]; // Enough to fit longest js file name from the tree
    const char* const basename = strrchr(filename, '/');
    size_t len = PR_snprintf(buffer, sizeof(buffer), "%s:%u",
                             basename ? basename + 1 : filename, lineno);
    if (len < sizeof(buffer)) {
      if (mStackToFill->IsSameAsEntry(aPrevLabel, buffer)) {
        return aPrevLabel;
      }

      // Keep track of the required buffer size
      aAvailableBufferSize -= (len + 1);
      if (aAvailableBufferSize >= 0) {
        // Buffer is big enough.
        return mStackToFill->InfallibleAppendViaBuffer(buffer, len);
      }
      // Buffer is not big enough; fall through to using static label below.
    }
    // snprintf failed or buffer is not big enough.
    label = "(chrome script)";
  } else {
    label = "(content script)";
  }

  if (mStackToFill->IsSameAsEntry(aPrevLabel, label)) {
    return aPrevLabel;
  }
  mStackToFill->infallibleAppend(label);
  return label;
}

#endif // MOZ_THREADSTACKHELPER_PSEUDO

void
ThreadStackHelper::FillStackBuffer()
{
  MOZ_ASSERT(mStackToFill->empty());

#ifdef MOZ_THREADSTACKHELPER_PSEUDO
  size_t reservedSize = mStackToFill->capacity();
  size_t reservedBufferSize = mStackToFill->AvailableBufferSize();
  intptr_t availableBufferSize = intptr_t(reservedBufferSize);

  // Go from front to back
  const volatile StackEntry* entry = mPseudoStack->mStack;
  const volatile StackEntry* end = entry + mPseudoStack->stackSize();
  // Deduplicate identical, consecutive frames
  const char* prevLabel = nullptr;
  for (; reservedSize-- && entry != end; entry++) {
    /* We only accept non-copy labels, including js::RunScript,
       because we only want static labels in the hang stack. */
    if (entry->isCopyLabel()) {
      continue;
    }
    if (entry->isJs()) {
      prevLabel = AppendJSEntry(entry, availableBufferSize, prevLabel);
      continue;
    }
#ifdef MOZ_THREADSTACKHELPER_NATIVE
    if (mContextToFill) {
      mContextToFill->mStackEnd = entry->stackAddress();
    }
#endif
    const char* const label = entry->label();
    if (mStackToFill->IsSameAsEntry(prevLabel, label)) {
      continue;
    }
    mStackToFill->infallibleAppend(label);
    prevLabel = label;
  }

  // end != entry if we exited early due to not enough reserved frames.
  // Expand the number of reserved frames for next time.
  mMaxStackSize = mStackToFill->capacity() + (end - entry);

  // availableBufferSize < 0 if we needed a larger buffer than we reserved.
  // Calculate a new reserve size for next time.
  if (availableBufferSize < 0) {
    mMaxBufferSize = reservedBufferSize - availableBufferSize;
  }
#endif
}

void
ThreadStackHelper::FillThreadContext(void* aContext)
{
#ifdef MOZ_THREADSTACKHELPER_NATIVE
  if (!mContextToFill) {
    return;
  }

  // TODO fill context for each platform

#endif // MOZ_THREADSTACKHELPER_NATIVE
}

} // namespace mozilla
