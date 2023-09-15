/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#ifndef mozilla_WinHeaderOnlyUtils_h
#define mozilla_WinHeaderOnlyUtils_h

#include <windows.h>
#include <winerror.h>
#include <winnt.h>
#include <winternl.h>
#include <objbase.h>
#include <shlwapi.h>
#undef ParseURL
#include <stdlib.h>
#include <tuple>

#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/DynamicallyLinkedFunctionPtr.h"
#include "mozilla/Maybe.h"
#include "mozilla/ResultVariant.h"
#include "mozilla/UniquePtr.h"
#include "nsWindowsHelpers.h"

#if defined(MOZILLA_INTERNAL_API)
#  include "nsIFile.h"
#  include "nsString.h"
#endif  // defined(MOZILLA_INTERNAL_API)

/**
 * This header is intended for self-contained, header-only, utility code for
 * Win32. It may be used outside of xul.dll, in places such as firefox.exe or
 * mozglue.dll. If your code creates dependencies on Mozilla libraries, you
 * should put it elsewhere.
 */

#if !defined(STATUS_SUCCESS)
#  define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif  // !defined(STATUS_SUCCESS)

// Our data indicates a few users of Win7 x86 hit failure to load urlmon.dll
// for unknown reasons.  Since we don't always require urlmon.dll on Win7,
// we delay-load it, which causes a crash if loading urlmon.dll fails.  This
// macro is to safely load and call urlmon's API graciously without crash.
#if defined(_X86_)
#  define SAFECALL_URLMON_FUNC(FuncName, ...)                                  \
    do {                                                                       \
      static const mozilla::StaticDynamicallyLinkedFunctionPtr<                \
          decltype(&::FuncName)>                                               \
          func(L"urlmon.dll", #FuncName);                                      \
      hr =                                                                     \
          func ? func(__VA_ARGS__) : HRESULT_FROM_WIN32(ERROR_PROC_NOT_FOUND); \
    } while (0)
#else
#  define SAFECALL_URLMON_FUNC(FuncName, ...) hr = ::FuncName(__VA_ARGS__)
#endif

namespace mozilla {

class WindowsError final {
 private:
  // HRESULT and NTSTATUS are both typedefs of LONG, so we cannot use
  // overloading to properly differentiate between the two. Instead we'll use
  // static functions to convert the various error types to HRESULTs before
  // instantiating.
  explicit constexpr WindowsError(HRESULT aHResult) : mHResult(aHResult) {}

 public:
  using UniqueString = UniquePtr<WCHAR[], LocalFreeDeleter>;

  static constexpr WindowsError FromNtStatus(NTSTATUS aNtStatus) {
    if (aNtStatus == STATUS_SUCCESS) {
      // Special case: we don't want to set FACILITY_NT_BIT
      // (HRESULT_FROM_NT does not handle this case, unlike HRESULT_FROM_WIN32)
      return WindowsError(S_OK);
    }

    return WindowsError(HRESULT_FROM_NT(aNtStatus));
  }

  static constexpr WindowsError FromHResult(HRESULT aHResult) {
    return WindowsError(aHResult);
  }

  static constexpr WindowsError FromWin32Error(DWORD aWin32Err) {
    return WindowsError(HRESULT_FROM_WIN32(aWin32Err));
  }

  static WindowsError FromLastError() {
    return FromWin32Error(::GetLastError());
  }

  static WindowsError CreateSuccess() { return WindowsError(S_OK); }

  static WindowsError CreateGeneric() {
    return FromWin32Error(ERROR_UNIDENTIFIED_ERROR);
  }

  bool IsSuccess() const { return SUCCEEDED(mHResult); }

  bool IsFailure() const { return FAILED(mHResult); }

  bool IsAvailableAsWin32Error() const {
    return IsAvailableAsNtStatus() ||
           HRESULT_FACILITY(mHResult) == FACILITY_WIN32;
  }

  bool IsAvailableAsNtStatus() const {
    return mHResult == S_OK || (mHResult & FACILITY_NT_BIT);
  }

  bool IsAvailableAsHResult() const { return true; }

  UniqueString AsString() const {
    LPWSTR rawMsgBuf = nullptr;
    constexpr DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS;
    DWORD result =
        ::FormatMessageW(flags, nullptr, mHResult, 0,
                         reinterpret_cast<LPWSTR>(&rawMsgBuf), 0, nullptr);
    if (!result) {
      return nullptr;
    }

    return UniqueString(rawMsgBuf);
  }

  HRESULT AsHResult() const { return mHResult; }

  // Not all HRESULTs are convertible to Win32 Errors, so we use Maybe
  Maybe<DWORD> AsWin32Error() const {
    if (mHResult == S_OK) {
      return Some(static_cast<DWORD>(ERROR_SUCCESS));
    }

    if (HRESULT_FACILITY(mHResult) == FACILITY_WIN32) {
      // This is the inverse of HRESULT_FROM_WIN32
      return Some(static_cast<DWORD>(HRESULT_CODE(mHResult)));
    }

    // The NTSTATUS facility is a special case and thus does not utilize the
    // HRESULT_FACILITY and HRESULT_CODE macros.
    if (mHResult & FACILITY_NT_BIT) {
      return Some(NtStatusToWin32Error(
          static_cast<NTSTATUS>(mHResult & ~FACILITY_NT_BIT)));
    }

    return Nothing();
  }

  // Not all HRESULTs are convertible to NTSTATUS, so we use Maybe
  Maybe<NTSTATUS> AsNtStatus() const {
    if (mHResult == S_OK) {
      return Some(STATUS_SUCCESS);
    }

    // The NTSTATUS facility is a special case and thus does not utilize the
    // HRESULT_FACILITY and HRESULT_CODE macros.
    if (mHResult & FACILITY_NT_BIT) {
      return Some(static_cast<NTSTATUS>(mHResult & ~FACILITY_NT_BIT));
    }

    return Nothing();
  }

  constexpr bool operator==(const WindowsError& aOther) const {
    return mHResult == aOther.mHResult;
  }

  constexpr bool operator!=(const WindowsError& aOther) const {
    return mHResult != aOther.mHResult;
  }

  static DWORD NtStatusToWin32Error(NTSTATUS aNtStatus) {
    static const StaticDynamicallyLinkedFunctionPtr<
        decltype(&RtlNtStatusToDosError)>
        pRtlNtStatusToDosError(L"ntdll.dll", "RtlNtStatusToDosError");

    MOZ_ASSERT(!!pRtlNtStatusToDosError);
    if (!pRtlNtStatusToDosError) {
      return ERROR_UNIDENTIFIED_ERROR;
    }

    return pRtlNtStatusToDosError(aNtStatus);
  }

 private:
  // We store the error code as an HRESULT because they can encode both Win32
  // error codes and NTSTATUS codes.
  HRESULT mHResult;
};

namespace detail {
template <>
struct UnusedZero<WindowsError> {
  using StorageType = WindowsError;

  static constexpr bool value = true;
  static constexpr StorageType nullValue = WindowsError::FromHResult(S_OK);

  static constexpr void AssertValid(StorageType aValue) {}
  static constexpr const WindowsError& Inspect(const StorageType& aValue) {
    return aValue;
  }
  static constexpr WindowsError Unwrap(StorageType aValue) { return aValue; }
  static constexpr StorageType Store(WindowsError aValue) { return aValue; }
};
}  // namespace detail

enum DetourResultCode : uint32_t {
  RESULT_OK = 0,
  INTERCEPTOR_MOD_NULL,
  INTERCEPTOR_MOD_INACCESSIBLE,
  INTERCEPTOR_PROC_NULL,
  INTERCEPTOR_PROC_INACCESSIBLE,
  DETOUR_PATCHER_RESERVE_FOR_MODULE_PE_ERROR,
  DETOUR_PATCHER_RESERVE_FOR_MODULE_TEXT_ERROR,
  DETOUR_PATCHER_RESERVE_FOR_MODULE_RESERVE_ERROR,
  DETOUR_PATCHER_DO_RESERVE_ERROR,
  DETOUR_PATCHER_NEXT_TRAMPOLINE_ERROR,
  DETOUR_PATCHER_INVALID_TRAMPOLINE,
  DETOUR_PATCHER_WRITE_POINTER_ERROR,
  DETOUR_PATCHER_CREATE_TRAMPOLINE_ERROR,
  FUNCHOOKCROSSPROCESS_COPYSTUB_ERROR,
  MMPOLICY_RESERVE_INVALIDARG,
  MMPOLICY_RESERVE_ZERO_RESERVATIONSIZE,
  MMPOLICY_RESERVE_CREATEFILEMAPPING,
  MMPOLICY_RESERVE_MAPVIEWOFFILE,
  MMPOLICY_RESERVE_NOBOUND_RESERVE_ERROR,
  MMPOLICY_RESERVE_FINDREGION_INVALIDLEN,
  MMPOLICY_RESERVE_FINDREGION_INVALIDRANGE,
  MMPOLICY_RESERVE_FINDREGION_VIRTUALQUERY_ERROR,
  MMPOLICY_RESERVE_FINDREGION_NO_FREE_REGION,
  MMPOLICY_RESERVE_FINAL_RESERVE_ERROR,
};

#if defined(NIGHTLY_BUILD)
struct DetourError {
  // We have a 16-bytes buffer, but only minimum bytes to detour per
  // architecture are copied.  See CreateTrampoline in PatcherDetour.h.
  DetourResultCode mErrorCode;
  uint8_t mOrigBytes[16];
  explicit DetourError(DetourResultCode aError)
      : mErrorCode(aError), mOrigBytes{} {}
  DetourError(DetourResultCode aError, DWORD aWin32Error)
      : mErrorCode(aError), mOrigBytes{} {
    static_assert(sizeof(mOrigBytes) >= sizeof(aWin32Error),
                  "Can't fit a DWORD in mOrigBytes");
    *reinterpret_cast<DWORD*>(mOrigBytes) = aWin32Error;
  }
  operator WindowsError() const {
    return WindowsError::FromHResult(mErrorCode);
  }
};
#endif  // defined(NIGHTLY_BUILD)

template <typename T>
using WindowsErrorResult = Result<T, WindowsError>;

struct LauncherError {
  LauncherError(const char* aFile, int aLine, WindowsError aWin32Error)
      : mFile(aFile), mLine(aLine), mError(aWin32Error) {}

#if defined(NIGHTLY_BUILD)
  LauncherError(const char* aFile, int aLine,
                const Maybe<DetourError>& aDetourError)
      : mFile(aFile),
        mLine(aLine),
        mError(aDetourError.isSome() ? aDetourError.value()
                                     : WindowsError::CreateGeneric()),
        mDetourError(aDetourError) {}
#endif  // defined(NIGHTLY_BUILD)

  const char* mFile;
  int mLine;
  WindowsError mError;
#if defined(NIGHTLY_BUILD)
  Maybe<DetourError> mDetourError;
#endif  // defined(NIGHTLY_BUILD)

  bool operator==(const LauncherError& aOther) const {
    return mError == aOther.mError;
  }

  bool operator!=(const LauncherError& aOther) const {
    return mError != aOther.mError;
  }

  bool operator==(const WindowsError& aOther) const { return mError == aOther; }

  bool operator!=(const WindowsError& aOther) const { return mError != aOther; }
};

#if defined(MOZ_USE_LAUNCHER_ERROR)

template <typename T>
using LauncherResult = Result<T, LauncherError>;

template <typename T>
using LauncherResultWithLineInfo = LauncherResult<T>;

using WindowsErrorType = LauncherError;

#else

template <typename T>
using LauncherResult = WindowsErrorResult<T>;

template <typename T>
using LauncherResultWithLineInfo = Result<T, LauncherError>;

using WindowsErrorType = WindowsError;

#endif  // defined(MOZ_USE_LAUNCHER_ERROR)

using LauncherVoidResult = LauncherResult<Ok>;

using LauncherVoidResultWithLineInfo = LauncherResultWithLineInfo<Ok>;

#if defined(MOZ_USE_LAUNCHER_ERROR)

#  define LAUNCHER_ERROR_GENERIC()           \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::CreateGeneric()))

#  if defined(NIGHTLY_BUILD)
#    define LAUNCHER_ERROR_FROM_DETOUR_ERROR(err) \
      ::mozilla::Err(::mozilla::LauncherError(__FILE__, __LINE__, err))
#  else
#    define LAUNCHER_ERROR_FROM_DETOUR_ERROR(err) LAUNCHER_ERROR_GENERIC()
#  endif  // defined(NIGHTLY_BUILD)

#  define LAUNCHER_ERROR_FROM_WIN32(err)     \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromWin32Error(err)))

#  define LAUNCHER_ERROR_FROM_LAST()         \
    ::mozilla::Err(::mozilla::LauncherError( \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromLastError()))

#  define LAUNCHER_ERROR_FROM_NTSTATUS(ntstatus) \
    ::mozilla::Err(::mozilla::LauncherError(     \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromNtStatus(ntstatus)))

#  define LAUNCHER_ERROR_FROM_HRESULT(hresult) \
    ::mozilla::Err(::mozilla::LauncherError(   \
        __FILE__, __LINE__, ::mozilla::WindowsError::FromHResult(hresult)))

// This macro wraps the supplied WindowsError with a LauncherError
#  define LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(err) \
    ::mozilla::Err(::mozilla::LauncherError(__FILE__, __LINE__, err))

#else

#  define LAUNCHER_ERROR_GENERIC() \
    ::mozilla::Err(::mozilla::WindowsError::CreateGeneric())

#  define LAUNCHER_ERROR_FROM_DETOUR_ERROR(err) LAUNCHER_ERROR_GENERIC()

#  define LAUNCHER_ERROR_FROM_WIN32(err) \
    ::mozilla::Err(::mozilla::WindowsError::FromWin32Error(err))

#  define LAUNCHER_ERROR_FROM_LAST() \
    ::mozilla::Err(::mozilla::WindowsError::FromLastError())

#  define LAUNCHER_ERROR_FROM_NTSTATUS(ntstatus) \
    ::mozilla::Err(::mozilla::WindowsError::FromNtStatus(ntstatus))

#  define LAUNCHER_ERROR_FROM_HRESULT(hresult) \
    ::mozilla::Err(::mozilla::WindowsError::FromHResult(hresult))

#  define LAUNCHER_ERROR_FROM_MOZ_WINDOWS_ERROR(err) ::mozilla::Err(err)

#endif  // defined(MOZ_USE_LAUNCHER_ERROR)

// How long to wait for a created process to become available for input,
// to prevent that process's windows being forced to the background.
// This is used across update, restart, and the launcher.
const DWORD kWaitForInputIdleTimeoutMS = 10 * 1000;

/**
 * Wait for a child GUI process to become "idle." Idle means that the process
 * has created its message queue and has begun waiting for user input.
 *
 * Note that this must only be used when the child process is going to display
 * GUI! Otherwise you're going to be waiting for a very long time ;-)
 *
 * @return true if we successfully waited for input idle;
 *         false if we timed out or failed to wait.
 */
inline bool WaitForInputIdle(HANDLE aProcess,
                             DWORD aTimeoutMs = kWaitForInputIdleTimeoutMS) {
  const DWORD kSleepTimeMs = 10;
  const DWORD waitStart = aTimeoutMs == INFINITE ? 0 : ::GetTickCount();
  DWORD elapsed = 0;

  while (true) {
    if (aTimeoutMs != INFINITE) {
      elapsed = ::GetTickCount() - waitStart;
    }

    if (elapsed >= aTimeoutMs) {
      return false;
    }

    // ::WaitForInputIdle() doesn't always set the last-error code on failure
    ::SetLastError(ERROR_SUCCESS);

    DWORD waitResult = ::WaitForInputIdle(aProcess, aTimeoutMs - elapsed);
    if (!waitResult) {
      return true;
    }

    if (waitResult == WAIT_FAILED &&
        ::GetLastError() == ERROR_NOT_GUI_PROCESS) {
      ::Sleep(kSleepTimeMs);
      continue;
    }

    return false;
  }
}

enum class PathType {
  eNtPath,
  eDosPath,
};

class FileUniqueId final {
 public:
  explicit FileUniqueId(const wchar_t* aPath, PathType aPathType)
      : mId(FILE_ID_INFO()) {
    if (!aPath) {
      mId = LAUNCHER_ERROR_FROM_HRESULT(E_INVALIDARG);
      return;
    }

    nsAutoHandle file;

    switch (aPathType) {
      default:
        mId = LAUNCHER_ERROR_FROM_HRESULT(E_INVALIDARG);
        MOZ_ASSERT_UNREACHABLE("Unhandled PathType");
        return;

      case PathType::eNtPath: {
        UNICODE_STRING unicodeString;
        ::RtlInitUnicodeString(&unicodeString, aPath);
        OBJECT_ATTRIBUTES objectAttributes;
        InitializeObjectAttributes(&objectAttributes, &unicodeString,
                                   OBJ_CASE_INSENSITIVE, nullptr, nullptr);
        IO_STATUS_BLOCK ioStatus = {};
        HANDLE ntHandle;
        NTSTATUS status = ::NtOpenFile(
            &ntHandle, SYNCHRONIZE | FILE_READ_ATTRIBUTES, &objectAttributes,
            &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT);
        // We don't need to check |ntHandle| for INVALID_HANDLE_VALUE here,
        // as that value is set by the Win32 layer.
        if (!NT_SUCCESS(status)) {
          mId = LAUNCHER_ERROR_FROM_NTSTATUS(status);
          return;
        }

        file.own(ntHandle);
        break;
      }

      case PathType::eDosPath: {
        file.own(::CreateFileW(
            aPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr));
        if (file == INVALID_HANDLE_VALUE) {
          mId = LAUNCHER_ERROR_FROM_LAST();
          return;
        }

        break;
      }
    }

    GetId(file);
  }

  explicit FileUniqueId(const nsAutoHandle& aFile) : mId(FILE_ID_INFO()) {
    GetId(aFile);
  }

  ~FileUniqueId() = default;

  bool IsError() const { return mId.isErr(); }

  const WindowsErrorType& GetError() const { return mId.inspectErr(); }

  FileUniqueId(FileUniqueId&& aOther) = default;
  FileUniqueId& operator=(FileUniqueId&& aOther) = delete;

  bool operator==(const FileUniqueId& aOther) const {
    return mId.isOk() && aOther.mId.isOk() &&
           !memcmp(&mId.inspect(), &aOther.mId.inspect(), sizeof(FILE_ID_INFO));
  }

  bool operator!=(const FileUniqueId& aOther) const {
    return !((*this) == aOther);
  }

 private:
  void GetId(const nsAutoHandle& aFile) {
    FILE_ID_INFO fileIdInfo = {};
    if (::GetFileInformationByHandleEx(aFile.get(), FileIdInfo, &fileIdInfo,
                                       sizeof(fileIdInfo))) {
      mId = fileIdInfo;
      return;
    }
    // Only NTFS and ReFS support FileIdInfo. So we have to fallback if
    // GetFileInformationByHandleEx failed.

    BY_HANDLE_FILE_INFORMATION info = {};
    if (!::GetFileInformationByHandle(aFile.get(), &info)) {
      mId = LAUNCHER_ERROR_FROM_LAST();
      return;
    }

    fileIdInfo.VolumeSerialNumber = info.dwVolumeSerialNumber;
    memcpy(&fileIdInfo.FileId.Identifier[0], &info.nFileIndexLow,
           sizeof(DWORD));
    memcpy(&fileIdInfo.FileId.Identifier[sizeof(DWORD)], &info.nFileIndexHigh,
           sizeof(DWORD));
    mId = fileIdInfo;
  }

 private:
  LauncherResult<FILE_ID_INFO> mId;
};

class MOZ_RAII AutoVirtualProtect final {
 public:
  AutoVirtualProtect(void* aAddress, size_t aLength, DWORD aProtFlags,
                     HANDLE aTargetProcess = ::GetCurrentProcess())
      : mAddress(aAddress),
        mLength(aLength),
        mTargetProcess(aTargetProcess),
        mPrevProt(0),
        mError(WindowsError::CreateSuccess()) {
    if (!::VirtualProtectEx(aTargetProcess, aAddress, aLength, aProtFlags,
                            &mPrevProt)) {
      mError = WindowsError::FromLastError();
    }
  }

  ~AutoVirtualProtect() {
    if (mError.IsFailure()) {
      return;
    }

    ::VirtualProtectEx(mTargetProcess, mAddress, mLength, mPrevProt,
                       &mPrevProt);
  }

  explicit operator bool() const { return mError.IsSuccess(); }

  WindowsError GetError() const { return mError; }

  DWORD PrevProt() const { return mPrevProt; }

  AutoVirtualProtect(const AutoVirtualProtect&) = delete;
  AutoVirtualProtect(AutoVirtualProtect&&) = delete;
  AutoVirtualProtect& operator=(const AutoVirtualProtect&) = delete;
  AutoVirtualProtect& operator=(AutoVirtualProtect&&) = delete;

 private:
  void* mAddress;
  size_t mLength;
  HANDLE mTargetProcess;
  DWORD mPrevProt;
  WindowsError mError;
};

inline UniquePtr<wchar_t[]> GetFullModulePath(HMODULE aModule) {
  DWORD bufLen = MAX_PATH;
  mozilla::UniquePtr<wchar_t[]> buf;
  DWORD retLen;

  while (true) {
    buf = mozilla::MakeUnique<wchar_t[]>(bufLen);
    retLen = ::GetModuleFileNameW(aModule, buf.get(), bufLen);
    if (!retLen) {
      return nullptr;
    }

    if (retLen == bufLen && ::GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
      bufLen *= 2;
      continue;
    }

    break;
  }

  // Upon success, retLen *excludes* the null character
  ++retLen;

  // Since we're likely to have a bunch of unused space in buf, let's
  // reallocate a string to the actual size of the file name.
  auto result = mozilla::MakeUnique<wchar_t[]>(retLen);
  if (wcscpy_s(result.get(), retLen, buf.get())) {
    return nullptr;
  }

  return result;
}

inline UniquePtr<wchar_t[]> GetFullBinaryPath() {
  return GetFullModulePath(nullptr);
}

// Generates the install directory without a trailing path separator.
inline bool GetInstallDirectory(UniquePtr<wchar_t[]>& installPath) {
  installPath = GetFullBinaryPath();
  // It's not safe to use PathRemoveFileSpecW with strings longer than MAX_PATH
  // (including null terminator).
  if (wcslen(installPath.get()) >= MAX_PATH) {
    return false;
  }
  ::PathRemoveFileSpecW(installPath.get());
  return true;
}

class ModuleVersion final {
 public:
  constexpr ModuleVersion() : mVersion(0ULL) {}

  explicit ModuleVersion(const VS_FIXEDFILEINFO& aFixedInfo)
      : mVersion((static_cast<uint64_t>(aFixedInfo.dwFileVersionMS) << 32) |
                 static_cast<uint64_t>(aFixedInfo.dwFileVersionLS)) {}

  explicit ModuleVersion(const uint64_t aVersion) : mVersion(aVersion) {}

  ModuleVersion(const ModuleVersion& aOther) : mVersion(aOther.mVersion) {}

  uint64_t AsInteger() const { return mVersion; }

  operator uint64_t() const { return AsInteger(); }

  std::tuple<uint16_t, uint16_t, uint16_t, uint16_t> AsTuple() const {
    uint16_t major = static_cast<uint16_t>((mVersion >> 48) & 0xFFFFU);
    uint16_t minor = static_cast<uint16_t>((mVersion >> 32) & 0xFFFFU);
    uint16_t patch = static_cast<uint16_t>((mVersion >> 16) & 0xFFFFU);
    uint16_t build = static_cast<uint16_t>(mVersion & 0xFFFFU);

    return {major, minor, patch, build};
  }

  explicit operator bool() const { return !!mVersion; }

  bool operator<(const ModuleVersion& aOther) const {
    return mVersion < aOther.mVersion;
  }

  bool operator<(const uint64_t& aOther) const { return mVersion < aOther; }

  ModuleVersion& operator=(const uint64_t aIntVersion) {
    mVersion = aIntVersion;
    return *this;
  }

 private:
  uint64_t mVersion;
};

inline LauncherResult<ModuleVersion> GetModuleVersion(
    const wchar_t* aModuleFullPath) {
  DWORD verInfoLen = ::GetFileVersionInfoSizeW(aModuleFullPath, nullptr);
  if (!verInfoLen) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  auto verInfoBuf = MakeUnique<BYTE[]>(verInfoLen);
  if (!::GetFileVersionInfoW(aModuleFullPath, 0, verInfoLen,
                             verInfoBuf.get())) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  UINT fixedInfoLen;
  VS_FIXEDFILEINFO* fixedInfo = nullptr;
  if (!::VerQueryValueW(verInfoBuf.get(), L"\\",
                        reinterpret_cast<LPVOID*>(&fixedInfo), &fixedInfoLen)) {
    // VerQueryValue may fail if the resource does not exist. This is not an
    // error; we'll return 0 in this case.
    return ModuleVersion(0ULL);
  }

  return ModuleVersion(*fixedInfo);
}

inline LauncherResult<ModuleVersion> GetModuleVersion(HMODULE aModule) {
  UniquePtr<wchar_t[]> fullPath(GetFullModulePath(aModule));
  if (!fullPath) {
    return LAUNCHER_ERROR_GENERIC();
  }

  return GetModuleVersion(fullPath.get());
}

#if defined(MOZILLA_INTERNAL_API)
inline LauncherResult<ModuleVersion> GetModuleVersion(nsIFile* aFile) {
  if (!aFile) {
    return LAUNCHER_ERROR_FROM_HRESULT(E_INVALIDARG);
  }

  nsAutoString fullPath;
  nsresult rv = aFile->GetPath(fullPath);
  if (NS_FAILED(rv)) {
    return LAUNCHER_ERROR_GENERIC();
  }

  return GetModuleVersion(fullPath.get());
}
#endif  // defined(MOZILLA_INTERNAL_API)

struct CoTaskMemFreeDeleter {
  void operator()(void* aPtr) { ::CoTaskMemFree(aPtr); }
};

inline LauncherResult<TOKEN_ELEVATION_TYPE> GetElevationType(
    const nsAutoHandle& aToken) {
  DWORD retLen;
  TOKEN_ELEVATION_TYPE elevationType;
  if (!::GetTokenInformation(aToken.get(), TokenElevationType, &elevationType,
                             sizeof(elevationType), &retLen)) {
    return LAUNCHER_ERROR_FROM_LAST();
  }

  return elevationType;
}

inline bool HasPackageIdentity() {
  HMODULE kernel32Dll = ::GetModuleHandleW(L"kernel32");
  if (!kernel32Dll) {
    return false;
  }

  typedef LONG(WINAPI * GetCurrentPackageIdProc)(UINT32*, BYTE*);
  GetCurrentPackageIdProc pGetCurrentPackageId =
      (GetCurrentPackageIdProc)::GetProcAddress(kernel32Dll,
                                                "GetCurrentPackageId");

  // If there was any package identity to retrieve, we get
  // ERROR_INSUFFICIENT_BUFFER. If there had been no package identity it
  // would instead return APPMODEL_ERROR_NO_PACKAGE.
  UINT32 packageNameSize = 0;
  return pGetCurrentPackageId &&
         (pGetCurrentPackageId(&packageNameSize, nullptr) ==
          ERROR_INSUFFICIENT_BUFFER);
}

inline UniquePtr<wchar_t[]> GetPackageFamilyName() {
  HMODULE kernel32Dll = ::GetModuleHandleW(L"kernel32");
  if (!kernel32Dll) {
    return nullptr;
  }

  typedef LONG(WINAPI * GetCurrentPackageFamilyNameProc)(UINT32*, PWSTR);
  GetCurrentPackageFamilyNameProc pGetCurrentPackageFamilyName =
      (GetCurrentPackageFamilyNameProc)::GetProcAddress(
          kernel32Dll, "GetCurrentPackageFamilyName");
  if (!pGetCurrentPackageFamilyName) {
    return nullptr;
  }

  UINT32 packageNameSize = 0;
  if (pGetCurrentPackageFamilyName(&packageNameSize, nullptr) !=
      ERROR_INSUFFICIENT_BUFFER) {
    return nullptr;
  }

  UniquePtr<wchar_t[]> packageIdentity = MakeUnique<wchar_t[]>(packageNameSize);
  if (pGetCurrentPackageFamilyName(&packageNameSize, packageIdentity.get()) !=
      ERROR_SUCCESS) {
    return nullptr;
  }

  return packageIdentity;
}

// This implementation is equivalent to PathGetDriveNumber[AW].
// We define our own version because using PathGetDriveNumber
// delay-loads shlwapi.dll, which may fail when the process is
// sandboxed.
template <typename T>
int MozPathGetDriveNumber(const T* aPath) {
  const auto ToDriveNumber = [](const T* aPath) -> int {
    if (*aPath == '\0' || *(aPath + 1) != ':') {
      return -1;
    }

    T c = *aPath;
    return (c >= 'A' && c <= 'Z')   ? c - 'A'
           : (c >= 'a' && c <= 'z') ? c - 'a'
                                    : -1;
  };

  if (!aPath) {
    return -1;
  }

  if (*aPath == '\\' && *(aPath + 1) == '\\' && *(aPath + 2) == '?' &&
      *(aPath + 3) == '\\') {
    return ToDriveNumber(aPath + 4);
  }

  return ToDriveNumber(aPath);
}

}  // namespace mozilla

#endif  // mozilla_WinHeaderOnlyUtils_h
