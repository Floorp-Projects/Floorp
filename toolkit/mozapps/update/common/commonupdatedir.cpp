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
#include <accctrl.h>
#include <aclapi.h>
#include <cstdarg>
#include <errno.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <winerror.h>
#include "nsWindowsHelpers.h"
#include "win_dirent.h"
#endif

#ifdef XP_WIN
// This is the name of the directory to be put in the application data directory
// if no vendor or application name is specified.
// (i.e. C:\ProgramData\<FALLBACK_VENDOR_NAME>)
#define FALLBACK_VENDOR_NAME "Mozilla"
// This describes the directory between the "Mozilla" directory and the install
// path hash (i.e. C:\ProgramData\Mozilla\<UPDATE_PATH_MID_DIR_NAME>\<hash>)
#define UPDATE_PATH_MID_DIR_NAME "updates"
// This describes the directory between the update directory and the patch
// directory.
// (i.e. C:\ProgramData\Mozilla\updates\<hash>\<UPDATE_SUBDIRECTORY>\0)
#define UPDATE_SUBDIRECTORY "updates"
// This defines the leaf update directory, where the MAR file is downloaded to
// (i.e. C:\ProgramData\Mozilla\updates\<hash>\updates\<PATCH_DIRECTORY>)
#define PATCH_DIRECTORY "0"

enum class WhichUpdateDir {
  CommonAppData,
  UserAppData,
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
class SimpleAutoString
{
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
  SimpleAutoString()
  : mLength(0)
  {
  }

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
   * allocated.
   */
  wchar_t* MutableString() {
    return mString.get();
  }
  const wchar_t* String() const {
    return mString.get();
  }

  size_t Length() const {
    return mLength;
  }

  void SwapBufferWith(mozilla::UniquePtr<wchar_t[]>& other) {
    mString.swap(other);
    if (mString) {
      mLength = wcslen(mString.get());
    } else {
      mLength = 0;
    }
  }

  void Truncate() {
    mLength = 0;
    if (mString) {
      mString.get()[0] = L'\0';
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
    if(!AllocEmpty(len)) {
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
    int bufferSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, src, -1,
                                         nullptr, 0);
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
struct CoTaskMemFreeDeleter
{
  void operator()(void* aPtr)
  {
    ::CoTaskMemFree(aPtr);
  }
};

/**
 * A lot of data goes into constructing an ACL and security attributes, and the
 * Windows documentation does not make it very clear what can be safely freed
 * after these objects are constructed. This struct holds all of the
 * construction data in one place so that it can be passed around and freed
 * properly.
 */
struct AutoPerms
{
  SID_IDENTIFIER_AUTHORITY sidIdentifierAuthority;
  nsAutoSid usersSID;
  nsAutoSid adminsSID;
  nsAutoSid systemSID;
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
                                  const char* vendor,
                                  const char* appName,
                                  WhichUpdateDir whichDir,
                                  SetPermissionsOf permsToSet,
                                  mozilla::UniquePtr<wchar_t[]>& result);
static HRESULT SetUpdateDirectoryPermissions(const SimpleAutoString& basePath,
                                             const SimpleAutoString& updatePath,
                                             bool fullUpdatePath,
                                             SetPermissionsOf permsToSet);
static HRESULT GeneratePermissions(AutoPerms& result);
static HRESULT SetPathPerms(SimpleAutoString& path, const AutoPerms& perms);
static HRESULT MoveConflicting(const SimpleAutoString& path);
static HRESULT SetPermissionsOfContents(const SimpleAutoString& basePath,
                                        const SimpleAutoString& leafUpdateDir,
                                        const AutoPerms& perms);
static bool PathConflictsWithLeaf(const SimpleAutoString& path,
                                  DWORD pathAttributes,
                                  bool permsSuccessfullySet,
                                  const SimpleAutoString& leafPath);
#endif // XP_WIN

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
 * @param installPath The null-terminated path to the installation directory
 *                    (i.e. the directory that contains the binary). Must not be
 *                    null. The path must not include a trailing slash.
 * @param vendor A pointer to a null-terminated string containing the vendor
 *               name, or null. This is only used to look up a registry key on
 *               Windows. On other platforms, the value has no effect. If null
 *               is passed on Windows, "Mozilla" will be used.
 * @param result The out parameter that will be set to contain the resulting
 *               hash. The value is wrapped in a UniquePtr to make cleanup
 *               easier on the caller.
 * @param useCompatibilityMode Enables compatibiliy mode. Defaults to false.
 * @return NS_OK, if successful.
 */
nsresult
GetInstallHash(const char16_t* installPath, const char* vendor,
               mozilla::UniquePtr<NS_tchar[]>& result,
               bool useCompatibilityMode /* = false */)
{
  MOZ_ASSERT(installPath != nullptr,
             "Install path must not be null in GetInstallHash");

#ifdef XP_WIN
  // The Windows installer caches this hash value in the registry
  SimpleAutoString regPath;
  regPath.AutoAllocAndAssignSprintf(L"SOFTWARE\\%S\\%S\\TaskBarIDs",
                                    vendor ? vendor : "Mozilla",
                                    MOZ_APP_BASENAME);
  if (regPath.Length() != 0) {
    bool gotCachedHash = GetCachedHash(installPath, HKEY_LOCAL_MACHINE, regPath,
                                       result);
    if (gotCachedHash) {
      return NS_OK;
    }
    gotCachedHash = GetCachedHash(installPath, HKEY_CURRENT_USER, regPath,
                                  result);
    if (gotCachedHash) {
      return NS_OK;
    }
  }
#endif

  // Unable to get the cached hash, so compute it.
  size_t pathSize = std::char_traits<char16_t>::length(installPath) *
                    sizeof(*installPath);
  uint64_t hash = CityHash64(reinterpret_cast<const char*>(installPath),
                             pathSize);

  size_t hashStrSize = sizeof(hash) * 2 + 1; // 2 hex digits per byte + null
  result = mozilla::MakeUnique<NS_tchar[]>(hashStrSize);
  int charsWritten;
  if (useCompatibilityMode) {
    // This behavior differs slightly from the default behavior.
    // When the default output would be "1234567800000009", this instead
    // produces "123456789".
    charsWritten = NS_tsnprintf(result.get(), hashStrSize,
                                NS_T("%") NS_T(PRIX32) NS_T("%") NS_T(PRIX32),
                                static_cast<uint32_t>(hash >> 32),
                                static_cast<uint32_t>(hash));
  } else {
    charsWritten = NS_tsnprintf(result.get(), hashStrSize,
                                NS_T("%") NS_T(PRIX64), hash);
  }
  if (charsWritten < 1 || static_cast<size_t>(charsWritten) > hashStrSize - 1) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

#ifdef XP_WIN
/**
 * Returns true if the registry key was successfully found and read into result.
 */
static bool
GetCachedHash(const char16_t* installPath, HKEY rootKey,
              const SimpleAutoString& regPath,
              mozilla::UniquePtr<NS_tchar[]>& result)
{
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
  if (!result) {
    return false;
  }
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
 * vendor and appName are passed as char*, not because we want that (we don't,
 * we want wchar_t), but because of the expected origin of the data. If this
 * data is available, it is probably available via XREAppData::vendor and
 * XREAppData::name.
 *
 * @param installPath The null-terminated path to the installation directory
 *                    (i.e. the directory that contains the binary). The path
 *                    must not include a trailing slash. If null is passed for
 *                    this value, the entire update directory path cannot be
 *                    retrieved, so the function will return the update
 *                    directory without the installation-specific leaf
 *                    directory. This feature exists for when the caller wants
 *                    to use this function to set directory permissions and does
 *                    not need the full update directory path.
 * @param vendor A pointer to a null-terminated string containing the vendor
 *               name. Will default to "Mozilla" if null is passed.
 * @param appName A pointer to a null-terminated string containing the
 *                application name, or null.
 * @param permsToSet Determines how aggressive to be when setting permissions.
 *                   This is the behavior by value:
 *                     BaseDir - Sets the permissions on the base directory
 *                               (Most likely C:\ProgramData\Mozilla)
 *                     BaseDirIfNotExists - Sets the permissions on the base
 *                                          directory, but only if it does not
 *                                          already exist.
 *                     AllFilesAndDirs - Recurses through the base directory,
 *                                       setting the permissions on all files
 *                                       and directories contained. Symlinks
 *                                       are removed. Files with names
 *                                       conflicting with the creation of the
 *                                       update directory are moved or removed.
 * @param result The out parameter that will be set to contain the resulting
 *               path. The value is wrapped in a UniquePtr to make cleanup
 *               easier on the caller.
 *
 * @return An HRESULT that should be tested with SUCCEEDED or FAILED.
 */
HRESULT
GetCommonUpdateDirectory(const wchar_t* installPath,
                         SetPermissionsOf permsToSet,
                         mozilla::UniquePtr<wchar_t[]>& result)
{
  return GetUpdateDirectory(installPath,
                            nullptr,
                            nullptr,
                            WhichUpdateDir::CommonAppData,
                            permsToSet,
                            result);
}

/**
 * This function is identical to the function above except that it gets the
 * "old" (pre-migration) update directory that is located in the user's app data
 * directory, rather than the new one in the common app data directory.
 *
 * The other difference is that this function does not create or change the
 * permissions of the update directory since the default permissions on this
 * directory are acceptable as they are.
 */
HRESULT
GetUserUpdateDirectory(const wchar_t* installPath,
                       const char* vendor,
                       const char* appName,
                       mozilla::UniquePtr<wchar_t[]>& result)
{
  return GetUpdateDirectory(installPath,
                            vendor,
                            appName,
                            WhichUpdateDir::UserAppData,
                            SetPermissionsOf::BaseDir, // Arbitrary value
                            result);
}

/**
 * This is a helper function that does all of the work for
 * GetCommonUpdateDirectory and GetUserUpdateDirectory. It partially exists to
 * prevent callers of GetUserUpdateDirectory from having to pass a useless
 * SetPermissionsOf argument, which will be ignored if whichDir is UserAppData.
 *
 * For information on the parameters and return value, see
 * GetCommonUpdateDirectory.
 */
static HRESULT
GetUpdateDirectory(const wchar_t* installPath,
                   const char* vendor,
                   const char* appName,
                   WhichUpdateDir whichDir,
                   SetPermissionsOf permsToSet,
                   mozilla::UniquePtr<wchar_t[]>& result)
{
  PWSTR baseDirParentPath;
  REFKNOWNFOLDERID folderID =
    (whichDir == WhichUpdateDir::CommonAppData) ? FOLDERID_ProgramData
                                                : FOLDERID_LocalAppData;
  HRESULT hrv = SHGetKnownFolderPath(folderID, KF_FLAG_CREATE, nullptr,
                                     &baseDirParentPath);
  // Free baseDirParentPath when it goes out of scope.
  mozilla::UniquePtr<wchar_t, CoTaskMemFreeDeleter>
    baseDirParentPathUnique(baseDirParentPath);
  if (FAILED(hrv)) {
    return hrv;
  }

  SimpleAutoString baseDir;
  if (whichDir == WhichUpdateDir::UserAppData && (vendor || appName)) {
    const char* rawBaseDir = vendor ? vendor : appName;
    hrv = baseDir.CopyFrom(rawBaseDir);
  } else {
    const wchar_t baseDirLiteral[] = NS_T(FALLBACK_VENDOR_NAME);
    hrv = baseDir.CopyFrom(baseDirLiteral);
  }
  if (FAILED(hrv)) {
    return hrv;
  }

  // Generate the base path (C:\ProgramData\Mozilla)
  SimpleAutoString basePath;
  size_t basePathLen = wcslen(baseDirParentPath) + 1 /* path separator */ +
                       baseDir.Length();
  basePath.AllocAndAssignSprintf(basePathLen, L"%s\\%s", baseDirParentPath,
                                 baseDir.String());
  if (basePath.Length() != basePathLen) {
    return E_FAIL;
  }

  // Generate the update directory path. This is the value to be returned by
  // this function.
  SimpleAutoString updatePath;
  if (installPath) {
    mozilla::UniquePtr<NS_tchar[]> hash;
    bool useCompatibilityMode = (whichDir == WhichUpdateDir::UserAppData);
    nsresult rv = GetInstallHash(reinterpret_cast<const char16_t*>(installPath),
                                 vendor, hash, useCompatibilityMode);
    if (NS_SUCCEEDED(rv)) {
      const wchar_t midPathDirName[] = NS_T(UPDATE_PATH_MID_DIR_NAME);
      size_t updatePathLen = basePath.Length() + 1 /* path separator */ +
                             wcslen(midPathDirName) + 1 /* path separator */ +
                             wcslen(hash.get());
      updatePath.AllocAndAssignSprintf(updatePathLen, L"%s\\%s\\%s",
                                       basePath.String(), midPathDirName,
                                       hash.get());
      // Permissions can still be set without this string, so wait until after
      // setting permissions to return failure if the string assignment failed.
    }
  }

  if (whichDir == WhichUpdateDir::CommonAppData) {
    if (updatePath.Length() > 0) {
      hrv = SetUpdateDirectoryPermissions(basePath, updatePath, true,
                                          permsToSet);
    } else {
      hrv = SetUpdateDirectoryPermissions(basePath, basePath, false,
                                          permsToSet);
    }
    if (FAILED(hrv)) {
      return hrv;
    }
  }

  if (!installPath) {
    basePath.SwapBufferWith(result);
    return S_OK;
  }

  if (updatePath.Length() == 0) {
    return E_FAIL;
  }
  updatePath.SwapBufferWith(result);
  return S_OK;
}

/**
 * If the basePath does not exist, it is created. If it does exist, it and its
 * contents may have their permissions reset based on the value of permsToSet.
 *
 * This function tries to set as many permissions as possible, even if an error
 * is encountered along the way. Any encountered error eventually causes the
 * function to return failure, but does not stop the execution of the function.
 *
 * @param basePath The top directory within the application data directory.
 *                 Typically "C:\ProgramData\Mozilla".
 * @param updatePath The update directory to be checked for conflicts. If files
 *                   conflicting with this directory structure exist, they may
 *                   be moved or deleted depending on the value of permsToSet.
 * @param fullUpdatePath Set to true if updatePath is the full update path. If
 *                       set to false, it means that we don't have the
 *                       installation-specific path component.
 * @param permsToSet See the documentation for GetCommonUpdateDirectory for the
 *                   descriptions of the effects of each SetPermissionsOf value.
 */
static HRESULT
SetUpdateDirectoryPermissions(const SimpleAutoString& basePath,
                              const SimpleAutoString& updatePath,
                              bool fullUpdatePath,
                              SetPermissionsOf permsToSet)
{
  HRESULT returnValue = S_OK; // Stores the value that will eventually be
                              // returned. If errors occur, this is set to the
                              // first error encountered.
  DWORD attributes = GetFileAttributesW(basePath.String());
  // validBaseDir will be true if the basePath exists, and is a non-symlinked
  // directory.
  bool validBaseDir = attributes != INVALID_FILE_ATTRIBUTES &&
                      attributes & FILE_ATTRIBUTE_DIRECTORY &&
                      !(attributes & FILE_ATTRIBUTE_REPARSE_POINT);

  // The most common case when calling this function is when the caller of
  // GetCommonUpdateDirectory just wants the update directory path, and passes
  // in the least aggressive option for setting permissions.
  // The most common environment is that the update directory already exists.
  // Optimize for this case.
  if (permsToSet == SetPermissionsOf::BaseDirIfNotExists && validBaseDir) {
    return S_OK;
  }

  AutoPerms perms;
  HRESULT hrv = GeneratePermissions(perms);
  if (FAILED(hrv)) {
    // Fatal error. There is no real way to recover from this.
    return hrv;
  }

  // If the base directory looks ok, we want to ensure that the permissions on
  // it are correct (we already exited early in the case where we don't need to
  // check permissions).
  // If the base directory doesn't look right, we may as well attempt to fix the
  // permissions before we attempt to get rid of the conflicting file.
  // Either way, attempt to set the permissions here.
  // Annoyingly, setting the permissions requires a mutable string.
  SimpleAutoString mutableBasePath;
  mutableBasePath.CopyFrom(basePath);
  if (mutableBasePath.Length() == 0) {
    // If we don't have a valid base directory, we are about to try to recreate
    // it, in which case we only care about the success of creating the base
    // directory. But if we aren't doing that, we are failing here by not
    // successfully setting the directory's permissions.
    if (validBaseDir) {
      returnValue = FAILED(returnValue) ? returnValue : E_OUTOFMEMORY;
    }
  } else {
    hrv = SetPathPerms(mutableBasePath, perms);
    // Don't set returnValue for this error. The error means that that path is
    // inaccessible, but if we can successfuly move/remove it, then the function
    // will have done its job.
    if (FAILED(hrv)) {
      validBaseDir = false;
    }
  }

  if (!validBaseDir) {
    MoveConflicting(basePath);
    // Don't bother checking the error here. Just check once for whether we
    // created the directory or not.

    // Now that there is nothing in the way of our base directory, create it.
    BOOL success = CreateDirectoryW(basePath.String(),
                                    &perms.securityAttributes);
    if (success) {
      // We successfully created a directory. It is safe to assume that it is
      // empty, so there is no reason to check the permissions of its contents.
      return returnValue;
    } else {
      returnValue = FAILED(returnValue) ? returnValue
                                        : HRESULT_FROM_WIN32(GetLastError());
    }
  }

  // We have now done our best to ensure that the base directory exists and has
  // the right permissions set. We only need to continue if we are setting the
  // permissions on all contained files.
  if (permsToSet != SetPermissionsOf::AllFilesAndDirs) {
    return returnValue;
  }

  if (fullUpdatePath) {
    // When we are doing a full permissions reset, we are also ensuring that no
    // files are in the way of our required directory structure. Generate the
    // path of the furthest leaf in our directory structure so that we can check
    // for conflicting files.
    SimpleAutoString leafDirPath;
    wchar_t updateSubdirectoryName[] = NS_T(UPDATE_SUBDIRECTORY);
    wchar_t patchDirectoryName[] = NS_T(PATCH_DIRECTORY);
    size_t leafDirLen = updatePath.Length() + wcslen(updateSubdirectoryName) +
                        wcslen(patchDirectoryName) + 2; /* 2 path seperators */
    leafDirPath.AllocAndAssignSprintf(leafDirLen, L"%s\\%s\\%s",
                                      updatePath.String(),
                                      updateSubdirectoryName,
                                      patchDirectoryName);
    if (leafDirPath.Length() == leafDirLen) {
      hrv = SetPermissionsOfContents(basePath, leafDirPath, perms);
    } else {
      // If we cannot generate the leaf path, just do the best we can by using
      // the updatePath.
      returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
      hrv = SetPermissionsOfContents(basePath, updatePath, perms);
    }
  } else {
    hrv = SetPermissionsOfContents(basePath, updatePath, perms);
  }
  return FAILED(returnValue) ? returnValue : hrv;
}

/**
 * Generates the permission set that we want to be applied to the update
 * directory and its contents. Returns the permissions data via the result
 * outparam.
 */
static HRESULT
GeneratePermissions(AutoPerms& result)
{
  result.sidIdentifierAuthority = SECURITY_NT_AUTHORITY;
  ZeroMemory(&result.ea, sizeof(result.ea));

  // Make Users group SID and add it to the Explicit Access List.
  PSID usersSID = nullptr;
  BOOL success = AllocateAndInitializeSid(&result.sidIdentifierAuthority, 2,
                                          SECURITY_BUILTIN_DOMAIN_RID,
                                          DOMAIN_ALIAS_RID_USERS,
                                          0, 0, 0, 0, 0, 0, &usersSID);
  result.usersSID.own(usersSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[0].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[0].grfAccessMode = SET_ACCESS;
  result.ea[0].grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[0].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[0].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  result.ea[0].Trustee.ptstrName  = static_cast<LPWSTR>(usersSID);

  // Make Administrators group SID and add it to the Explicit Access List.
  PSID adminsSID = nullptr;
  success = AllocateAndInitializeSid(&result.sidIdentifierAuthority, 2,
                                     SECURITY_BUILTIN_DOMAIN_RID,
                                     DOMAIN_ALIAS_RID_ADMINS,
                                     0, 0, 0, 0, 0, 0, &adminsSID);
  result.adminsSID.own(adminsSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[1].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[1].grfAccessMode = SET_ACCESS;
  result.ea[1].grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[1].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[1].Trustee.TrusteeType = TRUSTEE_IS_GROUP;
  result.ea[1].Trustee.ptstrName  = static_cast<LPWSTR>(adminsSID);

  // Make SYSTEM user SID and add it to the Explicit Access List.
  PSID systemSID = nullptr;
  success = AllocateAndInitializeSid(&result.sidIdentifierAuthority, 1,
                                     SECURITY_LOCAL_SYSTEM_RID,
                                     0, 0, 0, 0, 0, 0, 0, &systemSID);
  result.systemSID.own(systemSID);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  result.ea[2].grfAccessPermissions = FILE_ALL_ACCESS;
  result.ea[2].grfAccessMode = SET_ACCESS;
  result.ea[2].grfInheritance= SUB_CONTAINERS_AND_OBJECTS_INHERIT;
  result.ea[2].Trustee.TrusteeForm = TRUSTEE_IS_SID;
  result.ea[2].Trustee.TrusteeType = TRUSTEE_IS_USER;
  result.ea[2].Trustee.ptstrName  = static_cast<LPWSTR>(systemSID);

  PACL acl = nullptr;
  DWORD drv = SetEntriesInAclW(3, result.ea, nullptr, &acl);
  // Put the ACL in a unique pointer so that LocalFree is called when it goes
  // out of scope
  result.acl.reset(acl);
  if(drv != ERROR_SUCCESS) {
    return HRESULT_FROM_WIN32(drv);
  }

  result.securityDescriptorBuffer =
    mozilla::MakeUnique<uint8_t[]>(SECURITY_DESCRIPTOR_MIN_LENGTH);
  if (!result.securityDescriptorBuffer) {
    return E_OUTOFMEMORY;
  }
  result.securityDescriptor =
    reinterpret_cast<PSECURITY_DESCRIPTOR>(result.securityDescriptorBuffer.get());
  success = InitializeSecurityDescriptor(result.securityDescriptor,
                                         SECURITY_DESCRIPTOR_REVISION);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  success = SetSecurityDescriptorDacl(result.securityDescriptor, TRUE, acl,
                                      FALSE);
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  result.securityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
  result.securityAttributes.lpSecurityDescriptor = result.securityDescriptor;
  result.securityAttributes.bInheritHandle = FALSE;
  return S_OK;
}

/**
 * Sets the permissions of the file indicated by path to the permissions passed.
 * Unfortunately this does not take a const string because SetNamedSecurityInfoW
 * doesn't take one.
 */
static HRESULT
SetPathPerms(SimpleAutoString& path, const AutoPerms& perms)
{
  DWORD drv = SetNamedSecurityInfoW(path.MutableString(), SE_FILE_OBJECT,
                                    DACL_SECURITY_INFORMATION, nullptr,
                                    nullptr, perms.acl.get(), nullptr);
  return HRESULT_FROM_WIN32(drv);
}

/**
 * Attempts to move the file or directory to the Windows Recycle Bin.
 * If removal fails with an ERROR_FILE_NOT_FOUND, the file must not exist, so
 * this will return success in that case.
 */
static HRESULT
RemoveRecursive(const SimpleAutoString& path)
{
  DWORD attributes = GetFileAttributesW(path.String());

  // Ignore errors setting attributes. We only care if it was successfully
  // deleted.
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    SetFileAttributesW(path.String(), FILE_ATTRIBUTE_NORMAL);
  } else if (attributes & FILE_ATTRIBUTE_READONLY) {
    SetFileAttributesW(path.String(), attributes & ~FILE_ATTRIBUTE_READONLY);
  }

  // The SHFILEOPSTRUCTW expects a list of paths. The list is simply one long
  // string separated by null characters. The end of the list is designated by
  // two null characters.
  SimpleAutoString pathList;
  pathList.AllocAndAssignSprintf(path.Length() + 1, L"%s\0", path.String());

  SHFILEOPSTRUCTW fileOperation;
  fileOperation.hwnd = nullptr;
  fileOperation.wFunc = FO_DELETE;
  fileOperation.pFrom = pathList.String();
  fileOperation.pTo = nullptr;
  fileOperation.fFlags = FOF_ALLOWUNDO | FOF_NO_UI;
  fileOperation.lpszProgressTitle = nullptr;

  int rv = SHFileOperationW(&fileOperation);
  if (rv != 0 && rv != ERROR_FILE_NOT_FOUND) {
    return E_FAIL;
  }

  return S_OK;
}

/**
 * Attempts to move the file or directory to a path that will not conflict with
 * our directory structure.
 *
 * If an attempt results in the error ERROR_FILE_NOT_FOUND, this function
 * considers the file to no longer be a conflict and returns success.
 */
static HRESULT
MoveConflicting(const SimpleAutoString& path)
{
  // Try to move the file to a backup location
  SimpleAutoString newPath;
  unsigned int maxTries = 9;
  const wchar_t newPathFormat[] = L"%s.bak%u";
  size_t newPathMaxLength = newPath.AllocFromScprintf(newPathFormat,
                                                      path.String(),
                                                      maxTries);
  if (newPathMaxLength > 0) {
    for (unsigned int suffix = 0; suffix <= maxTries; ++suffix) {
      newPath.AssignSprintf(newPathMaxLength + 1, newPathFormat,
                            path.String(), suffix);
      if (newPath.Length() == 0) {
        // If we failed to make this string, we probably aren't going to
        // succeed on the next one.
        break;
      }
      BOOL success;
      if (suffix < maxTries) {
        success = MoveFileW(path.String(), newPath.String());
      } else {
        // Moving a file can sometimes work when deleting a file does not. If
        // there are already the maximum number of backed up files, try
        // overwriting the last backup before we fall back to deleting the
        // original.
        success = MoveFileExW(path.String(), newPath.String(),
                              MOVEFILE_REPLACE_EXISTING);
      }
      if (success) {
        return S_OK;
      }
      DWORD drv = GetLastError();
      if (drv == ERROR_FILE_NOT_FOUND) {
        return S_OK;
      }
      // If the move failed because newPath already exists, loop to try a new
      // suffix. If the move failed for any other reason, a new suffix will
      // probably not help.
      if (drv != ERROR_ALREADY_EXISTS) {
        break;
      }
    }
  }

  // Moving failed. Try to delete.
  return RemoveRecursive(path);
}

/**
 * This function recurses through the files and directories in the path passed.
 * All files and directories have their permissions set to those described in
 * perms.
 *
 * This function attempts to set as many permissions as possible. If an error is
 * encountered, the function will return the error *after* it attempts to set
 * permissions on all remaining files.
 */
static HRESULT
SetPermissionsOfContents(const SimpleAutoString& basePath,
                         const SimpleAutoString& leafUpdateDir,
                         const AutoPerms& perms)
{
  HRESULT returnValue = S_OK; // Stores the value that will eventually be
                              // returned. If errors occur, this is set to the
                              // first error encountered.

  SimpleAutoString pathBuffer;
  if (!pathBuffer.AllocEmpty(MAX_PATH)) {
    // Fatal error. We need a buffer to put the path in.
    return FAILED(returnValue) ? returnValue : E_OUTOFMEMORY;
  }

  DIR directoryHandle(basePath.String());
  errno = 0;

  for (dirent* entry = readdir(&directoryHandle); entry;
       entry = readdir(&directoryHandle)) {
    if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0) {
      continue;
    }

    pathBuffer.AssignSprintf(MAX_PATH + 1, L"%s\\%s", basePath.String(),
                             entry->d_name);
    if (pathBuffer.Length() == 0) {
      returnValue = FAILED(returnValue) ? returnValue
                  : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
      continue;
    }

    HRESULT hrv = SetPathPerms(pathBuffer, perms);
    returnValue = FAILED(returnValue) ? returnValue : hrv;
    bool permsSuccessfullySet = SUCCEEDED(hrv);

    // Rewrite the path buffer, since we cannot guarantee that SetPathPerms did
    // not change it.
    pathBuffer.AssignSprintf(MAX_PATH + 1, L"%s\\%s", basePath.String(),
                             entry->d_name);
    if (pathBuffer.Length() == 0) {
      returnValue = FAILED(returnValue) ? returnValue
                  : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
      continue;
    }

    DWORD attributes = GetFileAttributesW(pathBuffer.String());

    if (attributes == INVALID_FILE_ATTRIBUTES ||
        attributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      // Remove symlinks.
      hrv = RemoveRecursive(pathBuffer);
      if (SUCCEEDED(hrv)) {
        // If we successfully deleted the file, move on to the next file. There
        // is nothing else to do for this one.
        continue;
      } else {
        // Do not call continue in the error case. If this file conflicts with
        // our directory structure, we want to attempt to move it (which may
        // succeed even though deletion failed).
        returnValue = FAILED(returnValue) ? returnValue : hrv;
      }
    }
    if (PathConflictsWithLeaf(pathBuffer, attributes, permsSuccessfullySet,
                              leafUpdateDir)) {
      hrv = MoveConflicting(pathBuffer);
      if (SUCCEEDED(hrv)) {
        // File is out of the way. Move on to the next one.
        continue;
      }
      returnValue = FAILED(returnValue) ? returnValue : hrv;
    }

    // Recursively set permissions for non-symlink directories
    if (attributes != INVALID_FILE_ATTRIBUTES &&
        attributes & FILE_ATTRIBUTE_DIRECTORY &&
        !(attributes & FILE_ATTRIBUTE_REPARSE_POINT)) {
      hrv = SetPermissionsOfContents(pathBuffer, leafUpdateDir, perms);
      returnValue = FAILED(returnValue) ? returnValue : hrv;
    }

    // Before looping, clear any errors that might have been encountered so we
    // can correctly get errors from readdir.
    errno = 0;
  }
  if (errno != 0) {
    returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
  }

  return returnValue;
}

/**
 * Returns true if the path conflicts with the leaf path.
 */
static bool
PathConflictsWithLeaf(const SimpleAutoString& path,
                      DWORD pathAttributes,
                      bool permsSuccessfullySet,
                      const SimpleAutoString& leafPath)
{
  // Directories only conflict with our directory structure if we don't have
  // access to them
  if (pathAttributes != INVALID_FILE_ATTRIBUTES &&
      pathAttributes & FILE_ATTRIBUTE_DIRECTORY &&
      permsSuccessfullySet) {
    return false;
  }
  if (!leafPath.StartsWith(path)) {
    return false;
  }
  // Make sure that the next character after the path ends is a path separator
  // or the end of the string. We don't want to say that "C:\f" conflicts with
  // "C:\foo\bar".
  wchar_t charAfterPath = leafPath.String()[path.Length()];
  return (charAfterPath == L'\\' || charAfterPath == L'\0');
}
#endif // XP_WIN
