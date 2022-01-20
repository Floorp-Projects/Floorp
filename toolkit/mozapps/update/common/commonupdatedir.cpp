/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * This file does not use many of the features Firefox provides such as
 * nsAString and nsIFile because code in this file will be included not only
 * in Firefox, but also in the Mozilla Maintenance Service, the Mozilla
 * Maintenance Service installer, and TestAUSHelper.
 */

#include <cinttypes>
#include <cwchar>
#include <string>
#include "city.h"
#include "commonupdatedir.h"
#include "updatedefines.h"

#ifdef XP_WIN
#  include <accctrl.h>
#  include <aclapi.h>
#  include <cstdarg>
#  include <errno.h>
#  include <objbase.h>
#  include <shellapi.h>
#  include <shlobj.h>
#  include <strsafe.h>
#  include <winerror.h>
#  include "nsWindowsHelpers.h"
#  include "updateutils_win.h"
#endif

#ifdef XP_WIN
// This is the name of the old update directory
// (i.e. C:\ProgramData\<OLD_ROOT_UPDATE_DIR_NAME>)
#  define OLD_ROOT_UPDATE_DIR_NAME "Mozilla"
// This is the name of the current update directory
// (i.e. C:\ProgramData\<ROOT_UPDATE_DIR_NAME>)
// It is really important that we properly set the permissions on this
// directory at creation time. Thus, it is really important that this code be
// the creator of this directory. We had many problems with the old update
// directory having been previously created by old versions of Firefox. To avoid
// this problem in the future, we are including a UUID in the root update
// directory name to attempt to ensure that it will be created by this code and
// won't already exist with the wrong permissions.
#  define ROOT_UPDATE_DIR_NAME "Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38"
// This describes the directory between the "Mozilla" directory and the install
// path hash (i.e. C:\ProgramData\Mozilla\<UPDATE_PATH_MID_DIR_NAME>\<hash>)
#  define UPDATE_PATH_MID_DIR_NAME "updates"

enum class WhichUpdateDir {
  CurrentUpdateDir,
  UnmigratedUpdateDir,
};

/**
 * This is a very simple string class.
 *
 * This class has some substantial limitations for the sake of simplicity. It
 * has no support whatsoever for modifying a string that already has data. There
 * is, therefore, no append function and no support for automatically resizing
 * strings.
 *
 * Error handling is also done in a slightly unusual manner. If there is ever
 * a failure allocating or assigning to a string, it will do the simplest
 * possible recovery: truncate itself to 0-length.
 * This coupled with the fact that the length is cached means that an effective
 * method of error checking is to attempt assignment and then check the length
 * of the result.
 */
class SimpleAutoString {
 private:
  size_t mLength;
  // Unique pointer frees the buffer when the class is deleted or goes out of
  // scope.
  mozilla::UniquePtr<wchar_t[]> mString;

  /**
   * Allocates enough space to store a string of the specified length.
   */
  bool AllocLen(size_t len) {
    mString = mozilla::MakeUnique<wchar_t[]>(len + 1);
    return mString.get() != nullptr;
  }

  /**
   * Allocates a buffer of the size given.
   */
  bool AllocSize(size_t size) {
    mString = mozilla::MakeUnique<wchar_t[]>(size);
    return mString.get() != nullptr;
  }

 public:
  SimpleAutoString() : mLength(0) {}

  /*
   * Allocates enough space for a string of the given length and formats it as
   * an empty string.
   */
  bool AllocEmpty(size_t len) {
    bool success = AllocLen(len);
    Truncate();
    return success;
  }

  /**
   * These functions can potentially return null if no buffer has yet been
   * allocated. After changing a string retrieved with MutableString, the Check
   * method should be called to synchronize other members (ex: mLength) with the
   * new buffer.
   */
  wchar_t* MutableString() { return mString.get(); }
  const wchar_t* String() const { return mString.get(); }

  size_t Length() const { return mLength; }

  /**
   * This method should be called after manually changing the string's buffer
   * via MutableString to synchronize other members (ex: mLength) with the
   * new buffer.
   * Returns true if the string is now in a valid state.
   */
  bool Check() {
    mLength = wcslen(mString.get());
    return true;
  }

  void SwapBufferWith(mozilla::UniquePtr<wchar_t[]>& other) {
    mString.swap(other);
    if (mString) {
      mLength = wcslen(mString.get());
    } else {
      mLength = 0;
    }
  }

  void Swap(SimpleAutoString& other) {
    mString.swap(other.mString);
    size_t newLength = other.mLength;
    other.mLength = mLength;
    mLength = newLength;
  }

  /**
   * Truncates the string to the length specified. This must not be greater than
   * or equal to the size of the string's buffer.
   */
  void Truncate(size_t len = 0) {
    if (len > mLength) {
      return;
    }
    mLength = len;
    if (mString) {
      mString.get()[len] = L'\0';
    }
  }

  /**
   * Assigns a string and ensures that the resulting string is valid and has its
   * length set properly.
   *
   * Note that although other similar functions in this class take length, this
   * function takes buffer size instead because it is intended to be follow the
   * input convention of sprintf.
   *
   * Returns the new length, which will be 0 if there was any failure.
   *
   * This function does no allocation or reallocation. If the buffer is not
   * large enough to hold the new string, the call will fail.
   */
  size_t AssignSprintf(size_t bufferSize, const wchar_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    size_t returnValue = AssignVsprintf(bufferSize, format, ap);
    va_end(ap);
    return returnValue;
  }
  /**
   * Same as the above, but takes a va_list like vsprintf does.
   */
  size_t AssignVsprintf(size_t bufferSize, const wchar_t* format, va_list ap) {
    if (!mString) {
      Truncate();
      return 0;
    }

    int charsWritten = vswprintf(mString.get(), bufferSize, format, ap);
    if (charsWritten < 0 || static_cast<size_t>(charsWritten) >= bufferSize) {
      // charsWritten does not include the null terminator. If charsWritten is
      // equal to the buffer size, we do not have a null terminator nor do we
      // have room for one.
      Truncate();
      return 0;
    }
    mString.get()[charsWritten] = L'\0';

    mLength = charsWritten;
    return mLength;
  }

  /**
   * Allocates enough space for the string and assigns a value to it with
   * sprintf. Takes, as an argument, the maximum length that the string is
   * expected to use (which, after adding 1 for the null byte, is the amount of
   * space that will be allocated).
   *
   * Returns the new length, which will be 0 on any failure.
   */
  size_t AllocAndAssignSprintf(size_t maxLength, const wchar_t* format, ...) {
    if (!AllocLen(maxLength)) {
      Truncate();
      return 0;
    }
    va_list ap;
    va_start(ap, format);
    size_t charsWritten = AssignVsprintf(maxLength + 1, format, ap);
    va_end(ap);
    return charsWritten;
  }

  /*
   * Allocates enough for the formatted text desired. Returns maximum storable
   * length of a string in the allocated buffer on success, or 0 on failure.
   */
  size_t AllocFromScprintf(const wchar_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    size_t returnValue = AllocFromVscprintf(format, ap);
    va_end(ap);
    return returnValue;
  }
  /**
   * Same as the above, but takes a va_list like vscprintf does.
   */
  size_t AllocFromVscprintf(const wchar_t* format, va_list ap) {
    int len = _vscwprintf(format, ap);
    if (len < 0) {
      Truncate();
      return 0;
    }
    if (!AllocEmpty(len)) {
      // AllocEmpty will Truncate, no need to call it here.
      return 0;
    }
    return len;
  }

  /**
   * Automatically determines how much space is necessary, allocates that much
   * for the string, and assigns the data using swprintf. Returns the resulting
   * length of the string, which will be 0 if the function fails.
   */
  size_t AutoAllocAndAssignSprintf(const wchar_t* format, ...) {
    va_list ap;
    va_start(ap, format);
    size_t len = AllocFromVscprintf(format, ap);
    va_end(ap);
    if (len == 0) {
      // AllocFromVscprintf will Truncate, no need to call it here.
      return 0;
    }

    va_start(ap, format);
    size_t charsWritten = AssignVsprintf(len + 1, format, ap);
    va_end(ap);

    if (len != charsWritten) {
      Truncate();
      return 0;
    }
    return charsWritten;
  }

  /**
   * The following CopyFrom functions take various types of strings, allocate
   * enough space to hold them, and then copy them into that space.
   *
   * They return an HRESULT that should be interpreted with the SUCCEEDED or
   * FAILED macro.
   */
  HRESULT CopyFrom(const wchar_t* src) {
    mLength = wcslen(src);
    if (!AllocLen(mLength)) {
      Truncate();
      return E_OUTOFMEMORY;
    }
    HRESULT hrv = StringCchCopyW(mString.get(), mLength + 1, src);
    if (FAILED(hrv)) {
      Truncate();
    }
    return hrv;
  }
  HRESULT CopyFrom(const SimpleAutoString& src) {
    if (!src.mString) {
      Truncate();
      return S_OK;
    }
    mLength = src.mLength;
    if (!AllocLen(mLength)) {
      Truncate();
      return E_OUTOFMEMORY;
    }
    HRESULT hrv = StringCchCopyW(mString.get(), mLength + 1, src.mString.get());
    if (FAILED(hrv)) {
      Truncate();
    }
    return hrv;
  }
  HRESULT CopyFrom(const char* src) {
    int bufferSize =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1, nullptr, 0);
    if (bufferSize == 0) {
      Truncate();
      return HRESULT_FROM_WIN32(GetLastError());
    }
    if (!AllocSize(bufferSize)) {
      Truncate();
      return E_OUTOFMEMORY;
    }
    int charsWritten = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src,
                                           -1, mString.get(), bufferSize);
    if (charsWritten == 0) {
      Truncate();
      return HRESULT_FROM_WIN32(GetLastError());
    } else if (charsWritten != bufferSize) {
      Truncate();
      return E_FAIL;
    }
    mLength = charsWritten - 1;
    return S_OK;
  }

  bool StartsWith(const SimpleAutoString& prefix) const {
    if (!mString) {
      return (prefix.mLength == 0);
    }
    if (!prefix.mString) {
      return true;
    }
    if (prefix.mLength > mLength) {
      return false;
    }
    return (wcsncmp(mString.get(), prefix.mString.get(), prefix.mLength) == 0);
  }
};

// Deleter for use with UniquePtr
struct CoTaskMemFreeDeleter {
  void operator()(void* aPtr) { ::CoTaskMemFree(aPtr); }
};

/**
 * A lot of data goes into constructing an ACL and security attributes, and the
 * Windows documentation does not make it very clear what can be safely freed
 * after these objects are constructed. This struct holds all of the
 * construction data in one place so that it can be passed around and freed
 * properly.
 */
struct AutoPerms {
  SID_IDENTIFIER_AUTHORITY sidIdentifierAuthority;
  UniqueSidPtr usersSID;
  UniqueSidPtr adminsSID;
  UniqueSidPtr systemSID;
  EXPLICIT_ACCESS_W ea[3];
  mozilla::UniquePtr<ACL, LocalFreeDeleter> acl;
  mozilla::UniquePtr<uint8_t[]> securityDescriptorBuffer;
  PSECURITY_DESCRIPTOR securityDescriptor;
  SECURITY_ATTRIBUTES securityAttributes;
};

static bool GetCachedHash(const char16_t* installPath, HKEY rootKey,
                          const SimpleAutoString& regPath,
                          mozilla::UniquePtr<NS_tchar[]>& result);
static HRESULT GetUpdateDirectory(const wchar_t* installPath,
                                  WhichUpdateDir whichDir,
                                  mozilla::UniquePtr<wchar_t[]>& result);
static HRESULT GeneratePermissions(AutoPerms& result);
static HRESULT MakeDir(const SimpleAutoString& path, const AutoPerms& perms);
#endif  // XP_WIN

/**
 * Returns a hash of the install path, suitable for uniquely identifying the
 * particular Firefox installation that is running.
 *
 * This function includes a compatibility mode that should NOT be used except by
 * GetUserUpdateDirectory. Previous implementations of this function could
 * return a value inconsistent with what our installer would generate. When the
 * update directory was migrated, this function was re-implemented to return
 * values consistent with those generated by the installer. The compatibility
 * mode is retained only so that we can properly get the old update directory
 * when migrating it.
 *
 * @param   installPath
 *          The null-terminated path to the installation directory (i.e. the
 *          directory that contains the binary). Must not be null. The path must
 *          not include a trailing slash.
 * @param   result
 *          The out parameter that will be set to contain the resulting hash.
 *          The value is wrapped in a UniquePtr to make cleanup easier on the
 *          caller.
 * @return  true if successful and false otherwise.
 */
bool GetInstallHash(const char16_t* installPath,
                    mozilla::UniquePtr<NS_tchar[]>& result) {
  MOZ_ASSERT(installPath != nullptr,
             "Install path must not be null in GetInstallHash");

  size_t pathSize =
      std::char_traits<char16_t>::length(installPath) * sizeof(*installPath);
  uint64_t hash =
      CityHash64(reinterpret_cast<const char*>(installPath), pathSize);

  size_t hashStrSize = sizeof(hash) * 2 + 1;  // 2 hex digits per byte + null
  result = mozilla::MakeUnique<NS_tchar[]>(hashStrSize);
  int charsWritten =
      NS_tsnprintf(result.get(), hashStrSize, NS_T("%") NS_T(PRIX64), hash);
  return !(charsWritten < 1 ||
           static_cast<size_t>(charsWritten) > hashStrSize - 1);
}

#ifdef XP_WIN
/**
 * Returns true if the registry key was successfully found and read into result.
 */
static bool GetCachedHash(const char16_t* installPath, HKEY rootKey,
                          const SimpleAutoString& regPath,
                          mozilla::UniquePtr<NS_tchar[]>& result) {
  // Find the size of the string we are reading before we read it so we can
  // allocate space.
  unsigned long bufferSize;
  LSTATUS lrv = RegGetValueW(rootKey, regPath.String(),
                             reinterpret_cast<const wchar_t*>(installPath),
                             RRF_RT_REG_SZ, nullptr, nullptr, &bufferSize);
  if (lrv != ERROR_SUCCESS) {
    return false;
  }
  result = mozilla::MakeUnique<NS_tchar[]>(bufferSize);
  // Now read the actual value from the registry.
  lrv = RegGetValueW(rootKey, regPath.String(),
                     reinterpret_cast<const wchar_t*>(installPath),
                     RRF_RT_REG_SZ, nullptr, result.get(), &bufferSize);
  return (lrv == ERROR_SUCCESS);
}

/**
 * Returns the update directory path. The update directory needs to have
 * different permissions from the default, so we don't really want anyone using
 * the path without the directory already being created with the correct
 * permissions. Therefore, this function also ensures that the base directory
 * that needs permissions set already exists. If it does not exist, it is
 * created with the needed permissions.
 * The desired permissions give Full Control to SYSTEM, Administrators, and
 * Users.
 *
 * @param   installPath
 *          Must be the null-terminated path to the installation directory (i.e.
 *          the directory that contains the binary). The path must not include a
 *          trailing slash.
 * @param   result
 *          The out parameter that will be set to contain the resulting path.
 *          The value is wrapped in a UniquePtr to make cleanup easier on the
 *          caller.
 *
 * @return  An HRESULT that should be tested with SUCCEEDED or FAILED.
 */
HRESULT
GetCommonUpdateDirectory(const wchar_t* installPath,
                         mozilla::UniquePtr<wchar_t[]>& result) {
  return GetUpdateDirectory(installPath, WhichUpdateDir::CurrentUpdateDir,
                            result);
}

/**
 * This function is identical to the function above except that it gets the
 * "old" (pre-migration) update directory.
 *
 * The other difference is that this function does not create the directory.
 */
HRESULT
GetOldUpdateDirectory(const wchar_t* installPath,
                      mozilla::UniquePtr<wchar_t[]>& result) {
  return GetUpdateDirectory(installPath, WhichUpdateDir::UnmigratedUpdateDir,
                            result);
}

/**
 * This is a version of the GetCommonUpdateDirectory that can be called from
 * Rust.
 * The result parameter must be a valid pointer to a buffer of length
 * MAX_PATH + 1
 */
extern "C" HRESULT get_common_update_directory(const wchar_t* installPath,
                                               wchar_t* result) {
  mozilla::UniquePtr<wchar_t[]> uniqueResult;
  HRESULT hr = GetCommonUpdateDirectory(installPath, uniqueResult);
  if (FAILED(hr)) {
    return hr;
  }
  return StringCchCopyW(result, MAX_PATH + 1, uniqueResult.get());
}

/**
 * This is a helper function that does all of the work for
 * GetCommonUpdateDirectory and GetUserUpdateDirectory.
 *
 * For information on the parameters and return value, see
 * GetCommonUpdateDirectory.
 */
static HRESULT GetUpdateDirectory(const wchar_t* installPath,
                                  WhichUpdateDir whichDir,
                                  mozilla::UniquePtr<wchar_t[]>& result) {
  MOZ_ASSERT(installPath != nullptr,
             "Install path must not be null in GetUpdateDirectory");

  AutoPerms perms;
  HRESULT hrv = GeneratePermissions(perms);
  if (FAILED(hrv)) {
    return hrv;
  }

  PWSTR baseDirParentPath;
  hrv = SHGetKnownFolderPath(FOLDERID_ProgramData, KF_FLAG_CREATE, nullptr,
                             &baseDirParentPath);
  // Free baseDirParentPath when it goes out of scope.
  mozilla::UniquePtr<wchar_t, CoTaskMemFreeDeleter> baseDirParentPathUnique(
      baseDirParentPath);
  if (FAILED(hrv)) {
    return hrv;
  }

  SimpleAutoString baseDir;
  if (whichDir == WhichUpdateDir::UnmigratedUpdateDir) {
    const wchar_t baseDirLiteral[] = NS_T(OLD_ROOT_UPDATE_DIR_NAME);
    hrv = baseDir.CopyFrom(baseDirLiteral);
  } else {
    const wchar_t baseDirLiteral[] = NS_T(ROOT_UPDATE_DIR_NAME);
    hrv = baseDir.CopyFrom(baseDirLiteral);
  }
  if (FAILED(hrv)) {
    return hrv;
  }

  // Generate the base path
  // (C:\ProgramData\Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38)
  SimpleAutoString basePath;
  size_t basePathLen =
      wcslen(baseDirParentPath) + 1 /* path separator */ + baseDir.Length();
  basePath.AllocAndAssignSprintf(basePathLen, L"%s\\%s", baseDirParentPath,
                                 baseDir.String());
  if (basePath.Length() != basePathLen) {
    return E_FAIL;
  }

  if (whichDir == WhichUpdateDir::CurrentUpdateDir) {
    hrv = MakeDir(basePath, perms);
    if (FAILED(hrv)) {
      return hrv;
    }
  }

  // Generate what we are going to call the mid-path
  // (C:\ProgramData\Mozilla-1de4eec8-1241-4177-a864-e594e8d1fb38\updates)
  const wchar_t midPathDirName[] = NS_T(UPDATE_PATH_MID_DIR_NAME);
  size_t midPathLen =
      basePath.Length() + 1 /* path separator */ + wcslen(midPathDirName);
  SimpleAutoString midPath;
  midPath.AllocAndAssignSprintf(midPathLen, L"%s\\%s", basePath.String(),
                                midPathDirName);
  if (midPath.Length() != midPathLen) {
    return E_FAIL;
  }

  mozilla::UniquePtr<NS_tchar[]> hash;

  // The Windows installer caches this hash value in the registry
  bool gotHash = false;
  SimpleAutoString regPath;
  regPath.AutoAllocAndAssignSprintf(L"SOFTWARE\\Mozilla\\%S\\TaskBarIDs",
                                    MOZ_APP_BASENAME);
  if (regPath.Length() != 0) {
    gotHash = GetCachedHash(reinterpret_cast<const char16_t*>(installPath),
                            HKEY_LOCAL_MACHINE, regPath, hash);
    if (!gotHash) {
      gotHash = GetCachedHash(reinterpret_cast<const char16_t*>(installPath),
                              HKEY_CURRENT_USER, regPath, hash);
    }
  }
  // If we couldn't get it out of the registry, we'll just have to regenerate
  // it.
  if (!gotHash) {
    bool success =
        GetInstallHash(reinterpret_cast<const char16_t*>(installPath), hash);
    if (!success) {
      return E_FAIL;
    }
  }

  size_t updatePathLen =
      midPath.Length() + 1 /* path separator */ + wcslen(hash.get());
  SimpleAutoString updatePath;
  updatePath.AllocAndAssignSprintf(updatePathLen, L"%s\\%s", midPath.String(),
                                   hash.get());
  if (updatePath.Length() != updatePathLen) {
    return E_FAIL;
  }

  updatePath.SwapBufferWith(result);
  return S_OK;
}

/**
 * Generates the permission set that we want to be applied to the update
 * directory and its contents. Returns the permissions data via the result
 * outparam.
 *
 * These are also the permissions that will be used to check that file
 * permissions are correct.
 */
static HRESULT GeneratePermissions(AutoPerms& result) {
  result.sidIdentifierAuthority = SECURITY_NT_AUTHORITY;
  ZeroMemory(&result.ea, sizeof(result.ea));

  // Make Users group SID and add it to the Explicit Access List.
  PSID usersSID = nullptr;
  BOOL success = AllocateAndInitializeSid(
      &result.sidIdentifierAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
      DOMAIN_ALIAS_RID_USERS, 0, 0, 0, 0, 0, 0, &usersSID);
  result.usersSID.reset(usersSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[0].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[0].grfAccessMode = SET_ACCESS;
  result.ea[0].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  result.ea[0].Trustee.ptstrName = static_cast<LPWSTR>(usersSID);

  // Make Administrators group SID and add it to the Explicit Access List.
  PSID adminsSID = nullptr;
  success = AllocateAndInitializeSid(
      &result.sidIdentifierAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
      DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminsSID);
  result.adminsSID.reset(adminsSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[1].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[1].grfAccessMode = SET_ACCESS;
  result.ea[1].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  result.ea[1].Trustee.ptstrName = static_cast<LPWSTR>(adminsSID);

  // Make SYSTEM user SID and add it to the Explicit Access List.
  PSID systemSID = nullptr;
  success = AllocateAndInitializeSid(&result.sidIdentifierAuthority, 1,
                                     SECURITY_LOCAL_SYSTEM_RID, 0, 0, 0, 0, 0,
                                     0, 0, &systemSID);
  result.systemSID.reset(systemSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[2].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[2].grfAccessMode = SET_ACCESS;
  result.ea[2].grfInheritance = SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[2].Trustee.TrusteeType = TRUSTEE_IS_USER;
  result.ea[2].Trustee.ptstrName = static_cast<LPWSTR>(systemSID);

  PACL acl = nullptr;
  DWORD drv = SetEntriesInAclW(3, result.ea, nullptr, &acl);
  // Put the ACL in a unique pointer so that LocalFree is called when it goes
  // out of scope
  result.acl.reset(acl);
  if (drv != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(drv);
  }

  result.securityDescriptorBuffer =
      mozilla::MakeUnique<uint8_t[]>(SECURITY_DESCRIPTOR_MIN_LENGTH);
  if (!result.securityDescriptorBuffer) {
    return E_OUTOFMEMORY;
  }
  result.securityDescriptor = reinterpret_cast<PSECURITY_DESCRIPTOR>(
      result.securityDescriptorBuffer.get());
  success = InitializeSecurityDescriptor(result.securityDescriptor,
                                         SECURITY_DESCRIPTOR_REVISION);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  success =
      SetSecurityDescriptorDacl(result.securityDescriptor, TRUE, acl, FALSE);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  result.securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  result.securityAttributes.lpSecurityDescriptor = result.securityDescriptor;
  result.securityAttributes.bInheritHandle = FALSE;
  return S_OK;
}

/**
 * Creates a directory with the permissions specified. If the directory already
 * exists, this function will return success.
 */
static HRESULT MakeDir(const SimpleAutoString& path, const AutoPerms& perms) {
  BOOL success = CreateDirectoryW(
      path.String(),
      const_cast<LPSECURITY_ATTRIBUTES>(&perms.securityAttributes));
  if (success) {
    return S_OK;
  }
  DWORD error = GetLastError();
  if (error == ERROR_ALREADY_EXISTS) {
    return S_OK;
  }
  return HRESULT_FROM_WIN32(error);
}
#endif  // XP_WIN
