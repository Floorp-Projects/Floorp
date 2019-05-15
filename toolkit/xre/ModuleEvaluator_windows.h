/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ModuleEvaluator_windows_h
#define mozilla_ModuleEvaluator_windows_h

#include "mozilla/glue/WindowsDllServices.h"
#include "mozilla/Authenticode.h"
#include "mozilla/Maybe.h"
#include "mozilla/ModuleVersionInfo_windows.h"
#include "mozilla/TypedEnumBits.h"
#include "mozilla/Vector.h"
#include "nsString.h"

class InfallibleAllocPolicy;

namespace mozilla {

enum class ModuleTrustFlags : uint32_t {
  None = 0,
  MozillaSignature = 1,
  MicrosoftWindowsSignature = 2,
  MicrosoftVersion = 4,
  FirefoxDirectory = 8,
  FirefoxDirectoryAndVersion = 0x10,
  SystemDirectory = 0x20,
  KeyboardLayout = 0x40,
  JitPI = 0x80,
  WinSxSDirectory = 0x100,
  Xul = 0x200,
  SysWOW64Directory = 0x400,
};

MOZ_MAKE_ENUM_CLASS_BITWISE_OPERATORS(ModuleTrustFlags);

// This class is very similar to mozilla::glue::ModuleLoadEvent, except this is
// more Gecko-friendly, and has a few more fields that enable us to evaluate
// trustworthiness.
class ModuleLoadEvent {
 public:
  class ModuleInfo {
   public:
    ModuleInfo() = delete;
    ModuleInfo(const ModuleInfo&) = default;
    ModuleInfo(ModuleInfo&&) = default;
    ModuleInfo& operator=(const ModuleInfo&) = default;
    ModuleInfo& operator=(ModuleInfo&&) = default;

    explicit ModuleInfo(uintptr_t aBase);

    // Construct from the mozilla::glue version of this class.
    explicit ModuleInfo(const glue::ModuleLoadEvent::ModuleInfo&);

    bool PopulatePathInfo();
    bool PrepForTelemetry();

    uintptr_t mBase;
    nsString mLdrName;
    nsCOMPtr<nsIFile> mFile;  // Path as reported by GetModuleFileName()
    Maybe<double> mLoadDurationMS;

    // The following members are populated as we evaluate the module.
    nsString mFilePathClean;  // Path sanitized for telemetry reporting.
    ModuleTrustFlags mTrustFlags;
    nsCString mFileVersion;
  };

  ModuleLoadEvent() = default;
  ModuleLoadEvent(ModuleLoadEvent&&) = default;

  // The standard ModuleLoadEvent copy constructor "copy everything" behavior
  // can be tweaked to suit the context using this option.
  enum class CopyOption : int {
    CopyEverything,  // Default
    CopyWithoutModules,
  };

  ModuleLoadEvent(const ModuleLoadEvent& aOther,
                  CopyOption aOption = CopyOption::CopyEverything);

  ModuleLoadEvent& operator=(ModuleLoadEvent&& aOther) = default;
  ModuleLoadEvent& operator=(const ModuleLoadEvent& aOther) = delete;

  // Copy-construct from the mozilla::glue version of this class.
  explicit ModuleLoadEvent(const glue::ModuleLoadEvent& aOther);

  // Indicates whether the DLL was already loaded before we had a chance to
  // capture live info about it such as uptime / stack / thread.
  bool mIsStartup;
  DWORD mThreadID;
  nsCString mThreadName;
  uint64_t mProcessUptimeMS;
  Vector<uintptr_t, 0, InfallibleAllocPolicy> mStack;
  Vector<ModuleInfo, 0, InfallibleAllocPolicy> mModules;
};

// This class performs trustworthiness evaluation for incoming DLLs.
class ModuleEvaluator {
  Maybe<uint64_t> mExeVersion;  // Version number of the running EXE image
  nsCOMPtr<nsIFile> mExeDirectory;
  nsCOMPtr<nsIFile> mSysDirectory;
  nsCOMPtr<nsIFile> mWinSxSDirectory;
#ifdef _M_IX86
  nsCOMPtr<nsIFile> mSysWOW64Directory;
#endif  // _M_IX86
  Vector<nsString, 0, InfallibleAllocPolicy> mKeyboardLayoutDlls;

 public:
  ModuleEvaluator();

  explicit operator bool() const {
    // We exclude mSysWOW64Directory as it may not always be present
    return mExeVersion.isSome() && mExeDirectory && mSysDirectory &&
           mWinSxSDirectory;
  }

  /**
   * Evaluates the trustworthiness of the given module and fills in remaining
   * fields in ModuleInfo.
   *
   * @param  aDllInfo [in,out] Info about the DLL in question.
   *                  The following members must be valid:
   *                  mBase
   *                  mFile
   *                  This function fills in remaining fields of aDllInfo.
   * @param  aEvent   [in] Info about the relevant event that contains this DLL.
   * @param  aSvc     [in] Pointer to the service we use for Authenticode ops
   * @return Some(true) if the given module is trusted.
   *         Some(false) if the given module is untrusted.
   *         Nothing() if there was a failure.
   */
  Maybe<bool> IsModuleTrusted(ModuleLoadEvent::ModuleInfo& aDllInfo,
                              const ModuleLoadEvent& aEvent,
                              Authenticode* aSvc) const;
};

}  // namespace mozilla

#endif  // mozilla_ModuleEvaluator_windows_h
