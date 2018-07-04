/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NS_WINDOWS_DLL_INTERCEPTOR_H_
#define NS_WINDOWS_DLL_INTERCEPTOR_H_

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ArrayUtils.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Move.h"
#include "mozilla/Tuple.h"
#include "mozilla/TypeTraits.h"
#include "mozilla/Types.h"
#include "mozilla/UniquePtr.h"
#include "mozilla/Vector.h"
#include "nsWindowsHelpers.h"

#include <wchar.h>
#include <windows.h>
#include <winternl.h>

#include "mozilla/interceptor/MMPolicies.h"
#include "mozilla/interceptor/PatcherDetour.h"
#include "mozilla/interceptor/PatcherNopSpace.h"
#include "mozilla/interceptor/VMSharingPolicies.h"

/*
 * Simple function interception.
 *
 * We have two separate mechanisms for intercepting a function: We can use the
 * built-in nop space, if it exists, or we can create a detour.
 *
 * Using the built-in nop space works as follows: On x86-32, DLL functions
 * begin with a two-byte nop (mov edi, edi) and are preceeded by five bytes of
 * NOP instructions.
 *
 * When we detect a function with this prelude, we do the following:
 *
 * 1. Write a long jump to our interceptor function into the five bytes of NOPs
 *    before the function.
 *
 * 2. Write a short jump -5 into the two-byte nop at the beginning of the function.
 *
 * This mechanism is nice because it's thread-safe.  It's even safe to do if
 * another thread is currently running the function we're modifying!
 *
 * When the WindowsDllNopSpacePatcher is destroyed, we overwrite the short jump
 * but not the long jump, so re-intercepting the same function won't work,
 * because its prelude won't match.
 *
 *
 * Unfortunately nop space patching doesn't work on functions which don't have
 * this magic prelude (and in particular, x86-64 never has the prelude).  So
 * when we can't use the built-in nop space, we fall back to using a detour,
 * which works as follows:
 *
 * 1. Save first N bytes of OrigFunction to trampoline, where N is a
 *    number of bytes >= 5 that are instruction aligned.
 *
 * 2. Replace first 5 bytes of OrigFunction with a jump to the Hook
 *    function.
 *
 * 3. After N bytes of the trampoline, add a jump to OrigFunction+N to
 *    continue original program flow.
 *
 * 4. Hook function needs to call the trampoline during its execution,
 *    to invoke the original function (so address of trampoline is
 *    returned).
 *
 * When the WindowsDllDetourPatcher object is destructed, OrigFunction is
 * patched again to jump directly to the trampoline instead of going through
 * the hook function. As such, re-intercepting the same function won't work, as
 * jump instructions are not supported.
 *
 * Note that this is not thread-safe.  Sad day.
 *
 */

namespace mozilla {
namespace interceptor {

template <typename T>
struct OriginalFunctionPtrTraits;

template <typename R, typename... Args>
struct OriginalFunctionPtrTraits<R (*)(Args...)>
{
  using ReturnType = R;
};

#if defined(_M_IX86)
template <typename R, typename... Args>
struct OriginalFunctionPtrTraits<R (__stdcall*)(Args...)>
{
  using ReturnType = R;
};

template <typename R, typename... Args>
struct OriginalFunctionPtrTraits<R (__fastcall*)(Args...)>
{
  using ReturnType = R;
};
#endif // defined(_M_IX86)

template <typename InterceptorT, typename FuncPtrT>
class FuncHook final
{
public:
  using ThisType = FuncHook<InterceptorT, FuncPtrT>;
  using ReturnType = typename OriginalFunctionPtrTraits<FuncPtrT>::ReturnType;

  constexpr FuncHook()
    : mOrigFunc(nullptr)
    , mInitOnce(INIT_ONCE_STATIC_INIT)
  {
  }

  ~FuncHook() = default;

  bool Set(InterceptorT& aInterceptor, const char* aName,
           FuncPtrT aHookDest)
  {
    LPVOID addHookOk;
    InitOnceContext ctx(this, &aInterceptor, aName, aHookDest, false);

    return ::InitOnceExecuteOnce(&mInitOnce, &InitOnceCallback, &ctx,
                                 &addHookOk) && addHookOk;
  }

  bool SetDetour(InterceptorT& aInterceptor, const char* aName,
                 FuncPtrT aHookDest)
  {
    LPVOID addHookOk;
    InitOnceContext ctx(this, &aInterceptor, aName, aHookDest, true);

    return ::InitOnceExecuteOnce(&mInitOnce, &InitOnceCallback, &ctx,
                                 &addHookOk) && addHookOk;
  }

  explicit operator bool() const
  {
    return !!mOrigFunc;
  }

  template <typename... ArgsType>
  ReturnType operator()(ArgsType... aArgs) const
  {
    return mOrigFunc(std::forward<ArgsType>(aArgs)...);
  }

  FuncPtrT GetStub() const
  {
    return mOrigFunc;
  }

  // One-time init stuff cannot be moved or copied
  FuncHook(const FuncHook&) = delete;
  FuncHook(FuncHook&&) = delete;
  FuncHook& operator=(const FuncHook&) = delete;
  FuncHook& operator=(FuncHook&& aOther) = delete;

private:
  struct MOZ_RAII InitOnceContext final
  {
    InitOnceContext(ThisType* aHook, InterceptorT* aInterceptor,
                    const char* aName, void* aHookDest, bool aForceDetour)
      : mHook(aHook)
      , mInterceptor(aInterceptor)
      , mName(aName)
      , mHookDest(aHookDest)
      , mForceDetour(aForceDetour)
    {
    }

    ThisType*     mHook;
    InterceptorT* mInterceptor;
    const char*   mName;
    void*         mHookDest;
    bool          mForceDetour;
  };

private:
  bool Apply(InterceptorT* aInterceptor, const char* aName, void* aHookDest)
  {
    return aInterceptor->AddHook(aName, reinterpret_cast<intptr_t>(aHookDest),
                                 reinterpret_cast<void**>(&mOrigFunc));
  }

  bool ApplyDetour(InterceptorT* aInterceptor, const char* aName,
                   void* aHookDest)
  {
    return aInterceptor->AddDetour(aName, reinterpret_cast<intptr_t>(aHookDest),
                                   reinterpret_cast<void**>(&mOrigFunc));
  }

  static BOOL CALLBACK
  InitOnceCallback(PINIT_ONCE aInitOnce, PVOID aParam, PVOID* aOutContext)
  {
    MOZ_ASSERT(aOutContext);

    bool result;
    auto ctx = reinterpret_cast<InitOnceContext*>(aParam);
    if (ctx->mForceDetour) {
      result = ctx->mHook->ApplyDetour(ctx->mInterceptor, ctx->mName,
                                       ctx->mHookDest);
    } else {
      result = ctx->mHook->Apply(ctx->mInterceptor, ctx->mName, ctx->mHookDest);
    }

    *aOutContext = result ? reinterpret_cast<PVOID>(1U << INIT_ONCE_CTX_RESERVED_BITS) : nullptr;
    return TRUE;
  }

private:
  FuncPtrT  mOrigFunc;
  INIT_ONCE mInitOnce;
};

template <typename InterceptorT, typename FuncPtrT>
class MOZ_ONLY_USED_TO_AVOID_STATIC_CONSTRUCTORS FuncHookCrossProcess final
{
public:
  using ThisType = FuncHookCrossProcess<InterceptorT, FuncPtrT>;
  using ReturnType = typename OriginalFunctionPtrTraits<FuncPtrT>::ReturnType;

  FuncHookCrossProcess() = default;
  ~FuncHookCrossProcess() = default;

  bool Set(HANDLE aProcess, InterceptorT& aInterceptor, const char* aName,
           FuncPtrT aHookDest)
  {
    if (!aInterceptor.AddHook(aName, reinterpret_cast<intptr_t>(aHookDest),
                              reinterpret_cast<void**>(&mOrigFunc))) {
      return false;
    }

    return CopyStubToChildProcess(aProcess);
  }

  bool SetDetour(HANDLE aProcess, InterceptorT& aInterceptor, const char* aName,
                 FuncPtrT aHookDest)
  {
    if (!aInterceptor.AddDetour(aName, reinterpret_cast<intptr_t>(aHookDest),
                                reinterpret_cast<void**>(&mOrigFunc))) {
      return false;
    }

    return CopyStubToChildProcess(aProcess);
  }

  explicit operator bool() const
  {
    return !!mOrigFunc;
  }

  /**
   * NB: This operator is only meaningful when invoked in the target process!
   */
  template <typename... ArgsType>
  ReturnType operator()(ArgsType... aArgs) const
  {
    return mOrigFunc(std::forward<ArgsType>(aArgs)...);
  }

  FuncHookCrossProcess(const FuncHookCrossProcess&) = delete;
  FuncHookCrossProcess(FuncHookCrossProcess&&) = delete;
  FuncHookCrossProcess& operator=(const FuncHookCrossProcess&) = delete;
  FuncHookCrossProcess& operator=(FuncHookCrossProcess&& aOther) = delete;

private:
  bool CopyStubToChildProcess(HANDLE aProcess)
  {
    SIZE_T bytesWritten;
    return !!::WriteProcessMemory(aProcess, &mOrigFunc, &mOrigFunc,
                                  sizeof(mOrigFunc), &bytesWritten);
  }

private:
  FuncPtrT  mOrigFunc;
};

enum
{
  kDefaultTrampolineSize = 128
};

template <typename MMPolicyT, typename InterceptorT>
struct TypeResolver;

template <typename InterceptorT>
struct TypeResolver<mozilla::interceptor::MMPolicyInProcess, InterceptorT>
{
  template <typename FuncPtrT>
  using FuncHookType = FuncHook<InterceptorT, FuncPtrT>;
};

template <typename InterceptorT>
struct TypeResolver<mozilla::interceptor::MMPolicyOutOfProcess, InterceptorT>
{
  template <typename FuncPtrT>
  using FuncHookType = FuncHookCrossProcess<InterceptorT, FuncPtrT>;
};

template <typename VMPolicy =
            mozilla::interceptor::VMSharingPolicyShared<
              mozilla::interceptor::MMPolicyInProcess, kDefaultTrampolineSize>>
class WindowsDllInterceptor final : public TypeResolver<typename VMPolicy::MMPolicyT,
                                                        WindowsDllInterceptor<VMPolicy>>
{
  typedef WindowsDllInterceptor<VMPolicy> ThisType;

  interceptor::WindowsDllDetourPatcher<VMPolicy> mDetourPatcher;
#if defined(_M_IX86)
  interceptor::WindowsDllNopSpacePatcher<typename VMPolicy::MMPolicyT> mNopSpacePatcher;
#endif // defined(_M_IX86)

  HMODULE mModule;
  int mNHooks;

public:
  template <typename... Args>
  explicit WindowsDllInterceptor(Args... aArgs)
    : mDetourPatcher(std::forward<Args>(aArgs)...)
#if defined(_M_IX86)
    , mNopSpacePatcher(std::forward<Args>(aArgs)...)
#endif // defined(_M_IX86)
    , mModule(nullptr)
    , mNHooks(0)
  {
  }

  WindowsDllInterceptor(const WindowsDllInterceptor&) = delete;
  WindowsDllInterceptor(WindowsDllInterceptor&&) = delete;
  WindowsDllInterceptor& operator=(const WindowsDllInterceptor&) = delete;
  WindowsDllInterceptor& operator=(WindowsDllInterceptor&&) = delete;

  ~WindowsDllInterceptor()
  {
    Clear();
  }

  template <size_t N>
  void Init(const char (&aModuleName)[N], int aNumHooks = 0)
  {
    wchar_t moduleName[N];

    for (size_t i = 0; i < N; ++i) {
      MOZ_ASSERT(!(aModuleName[i] & 0x80),
                 "Use wide-character overload for non-ASCII module names");
      moduleName[i] = aModuleName[i];
    }

    Init(moduleName, aNumHooks);
  }

  void Init(const wchar_t* aModuleName, int aNumHooks = 0)
  {
    if (mModule) {
      return;
    }

    mModule = ::LoadLibraryW(aModuleName);
    mNHooks = aNumHooks;
  }

  void Clear()
  {
    if (!mModule) {
      return;
    }

#if defined(_M_IX86)
    mNopSpacePatcher.Clear();
#endif // defined(_M_IX86)
    mDetourPatcher.Clear();

    // NB: We intentionally leak mModule
  }

private:
  /**
   * Hook/detour the method aName from the DLL we set in Init so that it calls
   * aHookDest instead.  Returns the original method pointer in aOrigFunc
   * and returns true if successful.
   *
   * IMPORTANT: If you use this method, please add your case to the
   * TestDllInterceptor in order to detect future failures.  Even if this
   * succeeds now, updates to the hooked DLL could cause it to fail in
   * the future.
   */
  bool AddHook(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    // Use a nop space patch if possible, otherwise fall back to a detour.
    // This should be the preferred method for adding hooks.
    if (!mModule) {
      return false;
    }

    FARPROC proc = ::GetProcAddress(mModule, aName);
    if (!proc) {
      return false;
    }

#if defined(_M_IX86)
    if (mNopSpacePatcher.AddHook(proc, aHookDest, aOrigFunc)) {
      return true;
    }
#endif // defined(_M_IX86)

    return AddDetour(proc, aHookDest, aOrigFunc);
  }

  /**
   * Detour the method aName from the DLL we set in Init so that it calls
   * aHookDest instead.  Returns the original method pointer in aOrigFunc
   * and returns true if successful.
   *
   * IMPORTANT: If you use this method, please add your case to the
   * TestDllInterceptor in order to detect future failures.  Even if this
   * succeeds now, updates to the detoured DLL could cause it to fail in
   * the future.
   */
  bool AddDetour(const char* aName, intptr_t aHookDest, void** aOrigFunc)
  {
    // Generally, code should not call this method directly. Use AddHook unless
    // there is a specific need to avoid nop space patches.

    if (!mModule) {
      return false;
    }

    FARPROC proc = ::GetProcAddress(mModule, aName);
    if (!proc) {
      return false;
    }

    return AddDetour(proc, aHookDest, aOrigFunc);
  }

  bool AddDetour(FARPROC aProc, intptr_t aHookDest, void** aOrigFunc)
  {
    MOZ_ASSERT(mModule && aProc);

    if (!mDetourPatcher.Initialized()) {
      mDetourPatcher.Init(mNHooks);
    }

    return mDetourPatcher.AddHook(aProc, aHookDest, aOrigFunc);
  }

private:
  template <typename InterceptorT, typename FuncPtrT>
  friend class FuncHook;

  template <typename InterceptorT, typename FuncPtrT>
  friend class FuncHookCrossProcess;
};

} // namespace interceptor

using WindowsDllInterceptor = interceptor::WindowsDllInterceptor<>;

using CrossProcessDllInterceptor = interceptor::WindowsDllInterceptor<
  mozilla::interceptor::VMSharingPolicyUnique<
    mozilla::interceptor::MMPolicyOutOfProcess,
    mozilla::interceptor::kDefaultTrampolineSize>>;

} // namespace mozilla

#endif /* NS_WINDOWS_DLL_INTERCEPTOR_H_ */
