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
// This is the name of the directory to be put in the application data directory
// if no vendor or application name is specified.
// (i.e. C:\ProgramData\<FALLBACK_VENDOR_NAME>)
#  define FALLBACK_VENDOR_NAME "Mozilla"
// This describes the directory between the "Mozilla" directory and the install
// path hash (i.e. C:\ProgramData\Mozilla\<UPDATE_PATH_MID_DIR_NAME>\<hash>)
#  define UPDATE_PATH_MID_DIR_NAME "updates"
// This describes the directory between the update directory and the patch
// directory.
// (i.e. C:\ProgramData\Mozilla\updates\<hash>\<UPDATE_SUBDIRECTORY>\0)
#  define UPDATE_SUBDIRECTORY "updates"
// This defines the leaf update directory, where the MAR file is downloaded to
// (i.e. C:\ProgramData\Mozilla\updates\<hash>\updates\<PATCH_DIRECTORY>)
#  define PATCH_DIRECTORY "0"
// This defines the prefix of files created to lock a directory
#  define LOCK_FILE_PREFIX "mozlock."

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

static HRESULT GetFilename(SimpleAutoString& path, SimpleAutoString& filename);

enum class Tristate { False, True, Unknown };

enum class Lockstate { Locked, Unlocked };

/**
 * This class will look up and store some data about the file or directory at
 * the path given.
 * The path can additionally be locked. For files, this is done by holding a
 * handle to that file. For directories, this is done by holding a handle to a
 * file within the directory.
 */
class FileOrDirectory {
 private:
  Tristate mIsHardLink;
  DWORD mAttributes;
  nsAutoHandle mLockHandle;
  // This stores the name of the lock file. We need to keep track of this for
  // directories, which are locked via a randomly named lock file inside. But
  // we do not store a value here for files, as they do not have a separate lock
  // file.
  SimpleAutoString mDirLockFilename;

  /**
   * Locks the path. For directories, this is done by opening a file in the
   * directory and storing its handle in mLockHandle. For files, we just open
   * the file itself and store the handle.
   * Returns true on success and false on failure.
   *
   * Calling this function will result in mAttributes being updated.
   *
   * This function is private to prevent callers from locking the directory
   * after its attributes have been read. Part of the purpose of locking a
   * directory is to ensure that its attributes are what we think they are and
   * that they don't change while we hold the lock. If we get the lock after
   * attributes are looked up, we can no longer provide that guarantee.
   * If you think you want to call Lock(), you probably actually want to call
   * Reset().
   */
  bool Lock(const wchar_t* path) {
    mAttributes = GetFileAttributesW(path);
    Tristate isDir = IsDirectory();
    if (isDir == Tristate::Unknown) {
      return false;
    }

    if (isDir == Tristate::True) {
      SimpleAutoString lockPath;
      if (!lockPath.AllocEmpty(MAX_PATH)) {
        return false;
      }
      BOOL success = GetUUIDTempFilePath(path, NS_T(LOCK_FILE_PREFIX),
                                         lockPath.MutableString());
      if (!success || !lockPath.Check()) {
        return false;
      }

      HRESULT hrv = GetFilename(lockPath, mDirLockFilename);
      if (FAILED(hrv) || mDirLockFilename.Length() == 0) {
        return false;
      }

      mLockHandle.own(CreateFileW(lockPath.String(), 0, 0, nullptr, OPEN_ALWAYS,
                                  FILE_FLAG_DELETE_ON_CLOSE, nullptr));
    } else {  // If path is not a directory
      // The usual reason for us to lock a file is to read and change the
      // permissions so, unlike the directory lock file, make sure we request
      // the access necessary to read and write permissions.
      mLockHandle.own(CreateFileW(path, WRITE_DAC | READ_CONTROL, 0, nullptr,
                                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                  nullptr));
    }
    if (!IsLocked()) {
      return false;
    }
    mAttributes = GetFileAttributesW(path);
    // Directories and files are locked in different ways. If we think that we
    // just locked one but we actually locked the other, our lock will be
    // ineffective and we should not tell callers that this is locked.
    // (This should fail earlier, since files cannot have children and
    // directories cannot be opened with FILE_ATTRIBUTE_NORMAL. But just to be
    // safe...)
    if (isDir != IsDirectory()) {
      Unlock();
      return false;
    }
    return true;
  }

  /**
   * Helper function to normalize the access mask by converting generic access
   * flags to specific ones to make it easier to check if permissions match.
   */
  void NormalizeAccessMask(ACCESS_MASK& mask) {
    if ((mask & GENERIC_ALL) == GENERIC_ALL) {
      mask &= ~GENERIC_ALL;
      mask |= FILE_ALL_ACCESS;
    }
    if ((mask & GENERIC_READ) == GENERIC_READ) {
      mask &= ~GENERIC_READ;
      mask |= FILE_GENERIC_READ;
    }
    if ((mask & GENERIC_WRITE) == GENERIC_WRITE) {
      mask &= ~GENERIC_WRITE;
      mask |= FILE_GENERIC_WRITE;
    }
    if ((mask & GENERIC_EXECUTE) == GENERIC_EXECUTE) {
      mask &= ~GENERIC_EXECUTE;
      mask |= FILE_GENERIC_EXECUTE;
    }
  }

 public:
  FileOrDirectory()
      : mIsHardLink(Tristate::Unknown),
        mAttributes(INVALID_FILE_ATTRIBUTES),
        mLockHandle(INVALID_HANDLE_VALUE) {}

  /**
   * If shouldLock is Locked:Locked, the file or directory will be locked.
   * Note that locking is fallible and success should be checked via the
   * IsLocked method.
   */
  FileOrDirectory(const SimpleAutoString& path, Lockstate shouldLock)
      : FileOrDirectory() {
    Reset(path, shouldLock);
  }

  /**
   * Initializes the FileOrDirectory to the file with the path given.
   *
   * If shouldLock is Locked:Locked, the file or directory will be locked.
   * Note that locking is fallible and success should be checked via the
   * IsLocked method.
   */
  void Reset(const SimpleAutoString& path, Lockstate shouldLock) {
    Unlock();
    mDirLockFilename.Truncate();
    if (shouldLock == Lockstate::Locked) {
      // This will also update mAttributes.
      Lock(path.String());
    } else {
      mAttributes = GetFileAttributesW(path.String());
    }

    mIsHardLink = Tristate::Unknown;
    nsAutoHandle autoHandle;
    HANDLE handle;
    if (IsLocked() && IsDirectory() == Tristate::False) {
      // If the path is a file and we locked it, we already have a handle to it.
      // No need to open it again.
      handle = mLockHandle.get();
    } else {
      handle = CreateFileW(path.String(), 0, FILE_SHARE_READ, nullptr,
                           OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      // Make sure this handle gets freed automatically.
      autoHandle.own(handle);
    }

    Tristate isLink = Tristate::Unknown;
    if (handle != INVALID_HANDLE_VALUE) {
      BY_HANDLE_FILE_INFORMATION info;
      BOOL success = GetFileInformationByHandle(handle, &info);
      if (success) {
        if (info.nNumberOfLinks > 1) {
          isLink = Tristate::True;
        } else {
          isLink = Tristate::False;
        }
      }
    }

    mIsHardLink = Tristate::Unknown;
    Tristate isSymLink = IsSymLink();
    if (isLink == Tristate::False || isSymLink == Tristate::True) {
      mIsHardLink = Tristate::False;
    } else if (isLink == Tristate::True && isSymLink == Tristate::False) {
      mIsHardLink = Tristate::True;
    }
  }

  void Unlock() { mLockHandle.own(INVALID_HANDLE_VALUE); }

  bool IsLocked() const { return mLockHandle.get() != INVALID_HANDLE_VALUE; }

  Tristate IsSymLink() const {
    if (mAttributes == INVALID_FILE_ATTRIBUTES) {
      return Tristate::Unknown;
    }
    if (mAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
      return Tristate::True;
    }
    return Tristate::False;
  }

  Tristate IsHardLink() const { return mIsHardLink; }

  Tristate IsLink() const {
    Tristate isSymLink = IsSymLink();
    if (mIsHardLink == Tristate::True || isSymLink == Tristate::True) {
      return Tristate::True;
    }
    if (mIsHardLink == Tristate::Unknown || isSymLink == Tristate::Unknown) {
      return Tristate::Unknown;
    }
    return Tristate::False;
  }

  Tristate IsDirectory() const {
    if (mAttributes == INVALID_FILE_ATTRIBUTES) {
      return Tristate::Unknown;
    }
    if (mAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      return Tristate::True;
    }
    return Tristate::False;
  }

  Tristate IsReadonly() const {
    if (mAttributes == INVALID_FILE_ATTRIBUTES) {
      return Tristate::Unknown;
    }
    if (mAttributes & FILE_ATTRIBUTE_READONLY) {
      return Tristate::True;
    }
    return Tristate::False;
  }

  DWORD Attributes() const { return mAttributes; }

  /**
   * Sets the permissions to those passed. For this to be done safely, the file
   * must be locked and must not be a directory or a link. If these conditions
   * are not met, the function will fail.
   * Without locking, we can't guarantee that the file is the one we think it
   * is. Someone might have replaced a component of the path with a symlink.
   * With directories, setting the permissions can have the effect of setting
   * the permissions of a malicious hardlink within.
   */
  HRESULT SetPerms(const AutoPerms& perms) {
    if (IsDirectory() != Tristate::False || !IsLocked() ||
        IsHardLink() != Tristate::False) {
      return E_FAIL;
    }

    DWORD drv = SetSecurityInfo(mLockHandle.get(), SE_FILE_OBJECT,
                                DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                perms.acl.get(), nullptr);
    return HRESULT_FROM_WIN32(drv);
  }

  /**
   * Checks the permissions of a file to make sure that they match the expected
   * permissions.
   */
  Tristate PermsOk(const SimpleAutoString& path, const AutoPerms& perms) {
    nsAutoHandle autoHandle;
    HANDLE handle;
    if (IsDirectory() == Tristate::False && IsLocked()) {
      handle = mLockHandle.get();
    } else {
      handle =
          CreateFileW(path.String(), READ_CONTROL, FILE_SHARE_READ, nullptr,
                      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
      // Make sure this handle gets freed automatically.
      autoHandle.own(handle);
    }

    PACL dacl = nullptr;
    SECURITY_DESCRIPTOR* securityDescriptor = nullptr;
    DWORD drv = GetSecurityInfo(
        handle, SE_FILE_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr,
        &dacl, nullptr,
        reinterpret_cast<PSECURITY_DESCRIPTOR*>(&securityDescriptor));
    // Store the security descriptor in a UniquePtr so that it automatically
    // gets freed properly. We don't need to worry about dacl, since it will
    // point within the security descriptor.
    mozilla::UniquePtr<SECURITY_DESCRIPTOR, LocalFreeDeleter>
        autoSecurityDescriptor(securityDescriptor);
    if (drv == ERROR_ACCESS_DENIED) {
      // If access was denied reading the permissions, it seems pretty safe to
      // say that the permissions are wrong.
      return Tristate::False;
    }
    if (drv != ERROR_SUCCESS || dacl == nullptr) {
      return Tristate::Unknown;
    }

    size_t eaLen = sizeof(perms.ea) / sizeof(perms.ea[0]);
    for (size_t eaIndex = 0; eaIndex < eaLen; ++eaIndex) {
      PTRUSTEE_W trustee = const_cast<PTRUSTEE_W>(&perms.ea[eaIndex].Trustee);
      ACCESS_MASK expectedMask = perms.ea[eaIndex].grfAccessPermissions;
      ACCESS_MASK actualMask;
      drv = GetEffectiveRightsFromAclW(dacl, trustee, &actualMask);
      if (drv == ERROR_ACCESS_DENIED) {
        return Tristate::False;
      }
      if (drv != ERROR_SUCCESS) {
        return Tristate::Unknown;
      }
      NormalizeAccessMask(expectedMask);
      NormalizeAccessMask(actualMask);
      if ((actualMask & expectedMask) != expectedMask) {
        return Tristate::False;
      }
    }

    return Tristate::True;
  }

  /**
   * Valid only if IsDirectory() == True.
   * Checks to see if the string given matches the filename of the lock file.
   */
  bool LockFilenameMatches(const wchar_t* filename) {
    if (mDirLockFilename.Length() == 0) {
      return false;
    }
    return wcscmp(filename, mDirLockFilename.String()) == 0;
  }
};

static bool GetCachedHash(const char16_t* installPath, HKEY rootKey,
                          const SimpleAutoString& regPath,
                          mozilla::UniquePtr<NS_tchar[]>& result);
static HRESULT GetUpdateDirectory(const wchar_t* installPath,
                                  const char* vendor, const char* appName,
                                  WhichUpdateDir whichDir,
                                  SetPermissionsOf permsToSet,
                                  mozilla::UniquePtr<wchar_t[]>& result);
static HRESULT EnsureUpdateDirectoryPermissions(
    const SimpleAutoString& basePath, const SimpleAutoString& updatePath,
    bool fullUpdatePath, SetPermissionsOf permsToSet);
static HRESULT GeneratePermissions(AutoPerms& result);
static HRESULT MakeDir(const SimpleAutoString& path, const AutoPerms& perms);
static HRESULT RemoveRecursive(const SimpleAutoString& path,
                               FileOrDirectory& file);
static HRESULT MoveConflicting(const SimpleAutoString& path,
                               FileOrDirectory& file,
                               SimpleAutoString* outPath);
static HRESULT EnsureCorrectPermissions(SimpleAutoString& path,
                                        FileOrDirectory& file,
                                        const SimpleAutoString& leafUpdateDir,
                                        const AutoPerms& perms,
                                        SetPermissionsOf permsToSet);
static HRESULT FixDirectoryPermissions(const SimpleAutoString& path,
                                       FileOrDirectory& directory,
                                       const AutoPerms& perms,
                                       bool& permissionsFixed);
static HRESULT MoveFileOrDir(const SimpleAutoString& moveFrom,
                             const SimpleAutoString& moveTo,
                             const AutoPerms& perms);
static HRESULT SplitPath(const SimpleAutoString& path,
                         SimpleAutoString& parentPath,
                         SimpleAutoString& filename);
static bool PathConflictsWithLeaf(const SimpleAutoString& path,
                                  const SimpleAutoString& leafPath);
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
 * @param   vendor
 *          A pointer to a null-terminated string containing the vendor name, or
 *          null. This is only used to look up a registry key on Windows. On
 *          other platforms, the value has no effect. If null is passed on
 *          Windows, "Mozilla" will be used.
 * @param   result
 *          The out parameter that will be set to contain the resulting hash.
 *          The value is wrapped in a UniquePtr to make cleanup easier on the
 *          caller.
 * @param   useCompatibilityMode
 *          Enables compatibility mode. Defaults to false.
 * @return  NS_OK, if successful.
 */
nsresult GetInstallHash(const char16_t* installPath, const char* vendor,
                        mozilla::UniquePtr<NS_tchar[]>& result,
                        bool useCompatibilityMode /* = false */) {
  MOZ_ASSERT(installPath != nullptr,
             "Install path must not be null in GetInstallHash");

  // Unable to get the cached hash, so compute it.
  size_t pathSize =
      std::char_traits<char16_t>::length(installPath) * sizeof(*installPath);
  uint64_t hash =
      CityHash64(reinterpret_cast<const char*>(installPath), pathSize);

  size_t hashStrSize = sizeof(hash) * 2 + 1;  // 2 hex digits per byte + null
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
    charsWritten =
        NS_tsnprintf(result.get(), hashStrSize, NS_T("%") NS_T(PRIX64), hash);
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
 * @param   installPath
 *          The null-terminated path to the installation directory (i.e. the
 *          directory that contains the binary). The path must not include a
 *          trailing slash. If null is passed for this value, the entire update
 *          directory path cannot be retrieved, so the function will return the
 *          update directory without the installation-specific leaf directory.
 *          This feature exists for when the caller wants to use this function
 *          to set directory permissions and does not need the full update
 *          directory path.
 * @param   vendor
 *          A pointer to a null-terminated string containing the vendor name.
 *          Will default to "Mozilla" if null is passed.
 * @param   appName
 *          A pointer to a null-terminated string containing the application
 *          name, or null.
 * @param   permsToSet
 *          Determines how aggressive to be when setting permissions.
 *          This is the behavior by value:
 *          BaseDirIfNotExists - Sets the permissions on the base
 *                               directory, but only if it does not
 *                               already exist.
 *          AllFilesAndDirs - Recurses through the base directory,
 *                            setting the permissions on all files
 *                            and directories contained. Symlinks
 *                            are removed. Files with names
 *                            conflicting with the creation of the
 *                            update directory are moved or removed.
 *          FilesAndDirsWithBadPerms - Same as AllFilesAndDirs, but does not
 *                                     attempt to fix permissions if they
 *                                     cannot be determined.
 * @param   result
 *          The out parameter that will be set to contain the resulting path.
 *          The value is wrapped in a UniquePtr to make cleanup easier on the
 *          caller.
 *
 * @return  An HRESULT that should be tested with SUCCEEDED or FAILED.
 */
HRESULT
GetCommonUpdateDirectory(const wchar_t* installPath,
                         SetPermissionsOf permsToSet,
                         mozilla::UniquePtr<wchar_t[]>& result) {
  return GetUpdateDirectory(installPath, nullptr, nullptr,
                            WhichUpdateDir::CommonAppData, permsToSet, result);
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
GetUserUpdateDirectory(const wchar_t* installPath, const char* vendor,
                       const char* appName,
                       mozilla::UniquePtr<wchar_t[]>& result) {
  return GetUpdateDirectory(
      installPath, vendor, appName, WhichUpdateDir::UserAppData,
      SetPermissionsOf::BaseDirIfNotExists,  // Arbitrary value
      result);
}

/**
 * This is a much more limited version of the GetCommonUpdateDirectory that can
 * be called from Rust.
 * The result parameter must be a valid pointer to a buffer of length
 * MAX_PATH + 1
 */
extern "C" HRESULT get_common_update_directory(const wchar_t* installPath,
                                               wchar_t* result) {
  mozilla::UniquePtr<wchar_t[]> uniqueResult;
  HRESULT hr = GetCommonUpdateDirectory(
      installPath, SetPermissionsOf::BaseDirIfNotExists, uniqueResult);
  if (FAILED(hr)) {
    return hr;
  }
  return StringCchCopyW(result, MAX_PATH + 1, uniqueResult.get());
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
static HRESULT GetUpdateDirectory(const wchar_t* installPath,
                                  const char* vendor, const char* appName,
                                  WhichUpdateDir whichDir,
                                  SetPermissionsOf permsToSet,
                                  mozilla::UniquePtr<wchar_t[]>& result) {
  PWSTR baseDirParentPath;
  REFKNOWNFOLDERID folderID = (whichDir == WhichUpdateDir::CommonAppData)
                                  ? FOLDERID_ProgramData
                                  : FOLDERID_LocalAppData;
  HRESULT hrv = SHGetKnownFolderPath(folderID, KF_FLAG_CREATE, nullptr,
                                     &baseDirParentPath);
  // Free baseDirParentPath when it goes out of scope.
  mozilla::UniquePtr<wchar_t, CoTaskMemFreeDeleter> baseDirParentPathUnique(
      baseDirParentPath);
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
  size_t basePathLen =
      wcslen(baseDirParentPath) + 1 /* path separator */ + baseDir.Length();
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

    // The Windows installer caches this hash value in the registry
    bool gotHash = false;
    SimpleAutoString regPath;
    regPath.AutoAllocAndAssignSprintf(L"SOFTWARE\\%S\\%S\\TaskBarIDs",
                                      vendor ? vendor : "Mozilla",
                                      MOZ_APP_BASENAME);
    if (regPath.Length() != 0) {
      gotHash = GetCachedHash(reinterpret_cast<const char16_t*>(installPath),
                              HKEY_LOCAL_MACHINE, regPath, hash);
      if (!gotHash) {
        gotHash = GetCachedHash(reinterpret_cast<const char16_t*>(installPath),
                                HKEY_CURRENT_USER, regPath, hash);
      }
    }
    nsresult rv = NS_OK;
    if (!gotHash) {
      bool useCompatibilityMode = (whichDir == WhichUpdateDir::UserAppData);
      rv = GetInstallHash(reinterpret_cast<const char16_t*>(installPath),
                          vendor, hash, useCompatibilityMode);
    }
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
      hrv = EnsureUpdateDirectoryPermissions(basePath, updatePath, true,
                                             permsToSet);
    } else {
      hrv = EnsureUpdateDirectoryPermissions(basePath, basePath, false,
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
 * If the basePath does not exist, it is created with the expected permissions.
 *
 * It used to be that if basePath exists and SetPermissionsOf::AllFilesAndDirs
 * was passed in, this function would aggressively set the permissions of
 * the directory and everything in it. But that caused a problem: There does not
 * seem to be a good way to ensure that, when setting permissions on a
 * directory, a malicious process does not sneak a hard link into that directory
 * (causing it to inherit the permissions set on the directory).
 *
 * To address that issue, this function now takes a different approach.
 * To prevent abuse, permissions of directories will not be changed.
 * Instead, directories with bad permissions are deleted and re-created with the
 * correct permissions.
 *
 * @param   basePath
 *          The top directory within the application data directory.
 *          Typically "C:\ProgramData\Mozilla".
 * @param   updatePath
 *          The update directory to be checked for conflicts. If files
 *          conflicting with this directory structure exist, they may be moved
 *          or deleted depending on the value of permsToSet.
 * @param   fullUpdatePath
 *          Set to true if updatePath is the full update path. If set to false,
 *          it means that we don't have the installation-specific path
 *          component.
 * @param   permsToSet
 *          See the documentation for GetCommonUpdateDirectory for the
 *          descriptions of the effects of each SetPermissionsOf value.
 */
static HRESULT EnsureUpdateDirectoryPermissions(
    const SimpleAutoString& basePath, const SimpleAutoString& updatePath,
    bool fullUpdatePath, SetPermissionsOf permsToSet) {
  HRESULT returnValue = S_OK;  // Stores the value that will eventually be
                               // returned. If errors occur, this is set to the
                               // first error encountered.

  Lockstate shouldLock = permsToSet == SetPermissionsOf::BaseDirIfNotExists
                             ? Lockstate::Unlocked
                             : Lockstate::Locked;
  FileOrDirectory baseDir(basePath, shouldLock);
  // validBaseDir will be true if the basePath exists, and is a non-symlinked
  // directory.
  bool validBaseDir = baseDir.IsDirectory() == Tristate::True &&
                      baseDir.IsLink() == Tristate::False;

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

  if (permsToSet == SetPermissionsOf::BaseDirIfNotExists) {
    // We know that the base directory is invalid, because otherwise we would
    // have exited already.
    // Ignore errors here. It could be that the directory doesn't exist at all.
    // And ultimately, we are only interested in whether or not we successfully
    // create the new directory.
    MoveConflicting(basePath, baseDir, nullptr);

    hrv = MakeDir(basePath, perms);
    returnValue = FAILED(returnValue) ? returnValue : hrv;
    return returnValue;
  }

  // We need to pass a mutable basePath to EnsureCorrectPermissions, so copy it.
  SimpleAutoString mutBasePath;
  hrv = mutBasePath.CopyFrom(basePath);
  if (FAILED(hrv) || mutBasePath.Length() == 0) {
    returnValue = FAILED(returnValue) ? returnValue : hrv;
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
                        wcslen(patchDirectoryName) + 2; /* 2 path separators */
    leafDirPath.AllocAndAssignSprintf(
        leafDirLen, L"%s\\%s\\%s", updatePath.String(), updateSubdirectoryName,
        patchDirectoryName);
    if (leafDirPath.Length() == leafDirLen) {
      hrv = EnsureCorrectPermissions(mutBasePath, baseDir, leafDirPath, perms,
                                     permsToSet);
    } else {
      // If we cannot generate the leaf path, just do the best we can by using
      // the updatePath.
      returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
      hrv = EnsureCorrectPermissions(mutBasePath, baseDir, updatePath, perms,
                                     permsToSet);
    }
  } else {
    hrv = EnsureCorrectPermissions(mutBasePath, baseDir, updatePath, perms,
                                   permsToSet);
  }
  returnValue = FAILED(returnValue) ? returnValue : hrv;

  // EnsureCorrectPermissions does its best to remove links and conflicting
  // files but, in doing so, it may leave us without a base update directory.
  // Rather than checking whether it exists first, just try to create it. If
  // successful, the directory now exists with the right permissions and no
  // contents, which this function considers a success. If unsuccessful,
  // most likely the directory just already exists. But we need to verify that
  // before we can return success.
  BOOL success = CreateDirectoryW(
      basePath.String(),
      const_cast<LPSECURITY_ATTRIBUTES>(&perms.securityAttributes));
  if (success) {
    return S_OK;
  }
  if (SUCCEEDED(returnValue)) {
    baseDir.Reset(basePath, Lockstate::Unlocked);
    if (baseDir.IsDirectory() != Tristate::True ||
        baseDir.IsLink() != Tristate::False ||
        baseDir.PermsOk(basePath, perms) != Tristate::True) {
      return E_FAIL;
    }
  }

  return returnValue;
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
 * exists, this function will return success as long as it is a non-link
 * directory.
 */
static HRESULT MakeDir(const SimpleAutoString& path, const AutoPerms& perms) {
  BOOL success = CreateDirectoryW(
      path.String(),
      const_cast<LPSECURITY_ATTRIBUTES>(&perms.securityAttributes));
  if (success) {
    return S_OK;
  }
  DWORD error = GetLastError();
  if (error != ERROR_ALREADY_EXISTS) {
    return HRESULT_FROM_WIN32(error);
  }
  FileOrDirectory dir(path, Lockstate::Unlocked);
  if (dir.IsDirectory() == Tristate::True && dir.IsLink() == Tristate::False) {
    return S_OK;
  }
  return HRESULT_FROM_WIN32(error);
}

/**
 * Attempts to move the file or directory to the Windows Recycle Bin.
 * If removal fails with an ERROR_FILE_NOT_FOUND, the file must not exist, so
 * this will return success in that case.
 *
 * The file will be unlocked in order to remove it.
 *
 * Whether this function succeeds or fails, the file parameter should no longer
 * be considered accurate. If it succeeds, it will be inaccurate because the
 * file no longer exists. If it fails, it may be inaccurate due to this function
 * potentially setting file attributes.
 */
static HRESULT RemoveRecursive(const SimpleAutoString& path,
                               FileOrDirectory& file) {
  file.Unlock();
  if (file.IsReadonly() != Tristate::False) {
    // Ignore errors setting attributes. We only care if it was successfully
    // deleted.
    DWORD attributes = file.Attributes();
    if (attributes == INVALID_FILE_ATTRIBUTES) {
      SetFileAttributesW(path.String(), FILE_ATTRIBUTE_NORMAL);
    } else {
      SetFileAttributesW(path.String(), attributes & ~FILE_ATTRIBUTE_READONLY);
    }
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
  if (rv == 0 || rv == ERROR_FILE_NOT_FOUND) {
    return S_OK;
  }

  // Some files such as hard links can't be deleted properly with
  // SHFileOperation, so additionally try DeleteFile.
  BOOL success = DeleteFileW(path.String());
  return success ? S_OK : HRESULT_FROM_WIN32(GetLastError());
}

/**
 * Attempts to move the file or directory to a path that will not conflict with
 * our directory structure. If this fails, the path will instead be deleted.
 *
 * If an attempt results in the error ERROR_FILE_NOT_FOUND, this function
 * considers the file to no longer be a conflict and returns success.
 *
 * The file will be unlocked in order to move it. Strictly speaking, it may be
 * possible to move non-directories without unlocking them, but this function
 * will unconditionally unlock the file.
 *
 * If a non-null pointer is passed for outPath, the path that the file was moved
 * to will be stored there. If the file was removed, an empty string will be
 * stored. Note that if outPath is set to an empty string, it may not have a
 * buffer allocated, so outPath.Length() should be checked before using
 * outPath.String().
 * It is ok for outPath to point to the path parameter.
 * This function guarantees that if failure is returned, outPath will not be
 * modified.
 */
static HRESULT MoveConflicting(const SimpleAutoString& path,
                               FileOrDirectory& file,
                               SimpleAutoString* outPath) {
  file.Unlock();
  // Try to move the file to a backup location
  SimpleAutoString newPath;
  unsigned int maxTries = 3;
  const wchar_t newPathFormat[] = L"%s.bak%u";
  size_t newPathMaxLength =
      newPath.AllocFromScprintf(newPathFormat, path.String(), maxTries);
  if (newPathMaxLength > 0) {
    for (unsigned int suffix = 0; suffix <= maxTries; ++suffix) {
      newPath.AssignSprintf(newPathMaxLength + 1, newPathFormat, path.String(),
                            suffix);
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
        if (outPath) {
          outPath->Swap(newPath);
        }
        return S_OK;
      }
      DWORD drv = GetLastError();
      if (drv == ERROR_FILE_NOT_FOUND) {
        if (outPath) {
          outPath->Truncate();
        }
        return S_OK;
      }
      // If the move failed because newPath already exists, loop to try a new
      // suffix. If the move failed for any other reason, a new suffix will
      // probably not help.
      // Sometimes, however, if we cannot read the existing file due to lack of
      // permissions, we may get an "Access Denied" error. So retry in that case
      // too.
      if (drv != ERROR_ALREADY_EXISTS && drv != ERROR_ACCESS_DENIED) {
        break;
      }
    }
  }

  // Moving failed. Try to delete.
  HRESULT hrv = RemoveRecursive(path, file);
  if (SUCCEEDED(hrv)) {
    if (outPath) {
      outPath->Truncate();
    }
  }
  return hrv;
}

/**
 * This function will ensure that the specified path and all contained files and
 * subdirectories have the correct permissions.
 * Files will have their permissions set to match those specified.
 * Unfortunately, setting the permissions on directories is prone to abuse,
 * since it can potentially result in a hard link within the directory
 * inheriting those permissions. To get around this issue, directories will not
 * have their permissions changed. Instead, the directory will be moved
 * elsewhere so that it can be recreated with the correct permissions and its
 * contents moved back in.
 *
 * Symlinks and hard links are removed from the checked directories.
 *
 * This function also ensures that nothing is in the way of leafUpdateDir.
 * Non-directory files that conflict with this are moved or deleted.
 *
 * This function's second argument must receive a locked FileOrDirectory to
 * ensure that it is not tampered with while fixing the permissions of the
 * file/directory and any contents.
 *
 * If we cannot successfully determine if the path is a file or directory, we
 * simply attempt to delete it.
 *
 * Note that the path parameter is not constant. Its contents may be changed by
 * this function.
 */
static HRESULT EnsureCorrectPermissions(SimpleAutoString& path,
                                        FileOrDirectory& file,
                                        const SimpleAutoString& leafUpdateDir,
                                        const AutoPerms& perms,
                                        SetPermissionsOf permsToSet) {
  HRESULT returnValue = S_OK;  // Stores the value that will eventually be
                               // returned. If errors occur, this is set to the
                               // first error encountered.
  HRESULT hrv;
  bool conflictsWithLeaf = PathConflictsWithLeaf(path, leafUpdateDir);
  if (file.IsDirectory() != Tristate::True ||
      file.IsLink() != Tristate::False) {
    // We want to keep track of the result of trying to set the permissions
    // separately from returnValue. If we later remove the file, we should not
    // report an error to set permissions.
    // SetPerms will automatically abort and return failure if it is unsafe to
    // set the permissions on the file (for example, if it is a hard link).
    HRESULT permSetResult = file.SetPerms(perms);

    bool removed = false;
    if (file.IsLink() != Tristate::False) {
      hrv = RemoveRecursive(path, file);
      returnValue = FAILED(returnValue) ? returnValue : hrv;
      if (SUCCEEDED(hrv)) {
        removed = true;
      }
    }

    if (FAILED(permSetResult) && !removed) {
      returnValue = FAILED(returnValue) ? returnValue : permSetResult;
    }

    if (conflictsWithLeaf && !removed) {
      hrv = MoveConflicting(path, file, nullptr);
      returnValue = FAILED(returnValue) ? returnValue : hrv;
    }
    return returnValue;
  }

  // If the permissions cannot be read, only try to fix them if the most
  // aggressive permission-setting option was passed. If Firefox is experiencing
  // problems updating, it makes sense to try to force the permissions back to
  // being correct. But there are other times when this is run more proactively,
  // and we don't really want to move everything around unnecessarily in those
  // cases.
  Tristate permissionsOk = file.PermsOk(path, perms);
  if (permissionsOk == Tristate::False ||
      (permissionsOk == Tristate::Unknown &&
       permsToSet == SetPermissionsOf::AllFilesAndDirs)) {
    bool permissionsFixed;
    hrv = FixDirectoryPermissions(path, file, perms, permissionsFixed);
    returnValue = FAILED(returnValue) ? returnValue : hrv;
    // We only need to move conflicting directories if they have bad permissions
    // that we are unable to fix. If its permissions are correct, it isn't
    // conflicting with the leaf path, it is a component of the leaf path.
    if (!permissionsFixed && conflictsWithLeaf) {
      // No need to check for error here. returnValue is already a failure code
      // because FixDirectoryPermissions failed. MoveConflicting will ensure
      // that path is correct (or empty, on deletion) whether it succeeds or
      // fails.
      MoveConflicting(path, file, &path);
      if (path.Length() == 0) {
        // Path has been deleted.
        return returnValue;
      }
    }
    if (!file.IsLocked()) {
      // FixDirectoryPermissions or MoveConflicting may have left the directory
      // unlocked, but we still want to recurse into it, so re-lock it.
      file.Reset(path, Lockstate::Locked);
    }
  } else if (permissionsOk != Tristate::True) {
    // If we are skipping permission setting, we want to report failure since
    // this function did not do its job.
    returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
  }

  // We MUST not recurse into unlocked directories or links.
  if (!file.IsLocked() || file.IsLink() != Tristate::False ||
      file.IsDirectory() != Tristate::True) {
    returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
    return returnValue;
  }

  // Recurse into the directory.
  DIR directoryHandle(path.String());
  errno = 0;
  for (dirent* entry = readdir(&directoryHandle); entry;
       entry = readdir(&directoryHandle)) {
    if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0 ||
        file.LockFilenameMatches(entry->d_name)) {
      continue;
    }

    SimpleAutoString childBuffer;
    if (!childBuffer.AllocEmpty(MAX_PATH)) {
      // Just return on this failure rather than continuing. It is unlikely that
      // this error will go away for the next path we try.
      return FAILED(returnValue) ? returnValue : E_OUTOFMEMORY;
    }

    childBuffer.AssignSprintf(MAX_PATH + 1, L"%s\\%s", path.String(),
                              entry->d_name);
    if (childBuffer.Length() == 0) {
      returnValue = FAILED(returnValue)
                        ? returnValue
                        : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
      continue;
    }

    FileOrDirectory child(childBuffer, Lockstate::Locked);
    hrv = EnsureCorrectPermissions(childBuffer, child, leafUpdateDir, perms,
                                   permsToSet);
    returnValue = FAILED(returnValue) ? returnValue : hrv;

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
 * This function fixes directory permissions without setting them directly.
 * The reasoning behind this is that if someone puts a hardlink in the
 * directory before we set the permissions, the permissions of the linked file
 * will be changed too. To prevent this, we will instead move the directory,
 * recreate it with the correct permissions, and move the contents back in.
 *
 * The new directory will be locked with the directory parameter so that the
 * caller can safely use the new directory. If the function fails, the directory
 * parameter may be left locked or unlocked. However, the function will never
 * leave the directory parameter locking something invalid. In other words, if
 * the directory parameter is locked after this function exits, it is safe to
 * assume that it is a locked non-link directory at the same location as the
 * original path.
 *
 * The permissionsFixed outparam serves as sort of a supplement to the return
 * value. The return value will be an error code if any part of this function
 * fails. But the function can fail at some parts while still completing its
 * main goal of fixing the directory permissions. To distinguish between these,
 * this value will be set to true if the directory permissions were successfully
 * fixed.
 */
static HRESULT FixDirectoryPermissions(const SimpleAutoString& path,
                                       FileOrDirectory& directory,
                                       const AutoPerms& perms,
                                       bool& permissionsFixed) {
  permissionsFixed = false;

  SimpleAutoString parent;
  SimpleAutoString dirName;
  HRESULT hrv = SplitPath(path, parent, dirName);
  if (FAILED(hrv)) {
    return E_FAIL;
  }

  SimpleAutoString tempPath;
  if (!tempPath.AllocEmpty(MAX_PATH)) {
    return E_FAIL;
  }
  BOOL success = GetUUIDTempFilePath(parent.String(), dirName.String(),
                                     tempPath.MutableString());
  if (!success || !tempPath.Check() || tempPath.Length() == 0) {
    return E_FAIL;
  }

  directory.Unlock();
  success = MoveFileW(path.String(), tempPath.String());
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }

  success = CreateDirectoryW(path.String(), const_cast<LPSECURITY_ATTRIBUTES>(
                                                &perms.securityAttributes));
  if (!success) {
    return E_FAIL;
  }
  directory.Reset(path, Lockstate::Locked);
  if (!directory.IsLocked() || directory.IsLink() != Tristate::False ||
      directory.IsDirectory() != Tristate::True ||
      directory.PermsOk(path, perms) != Tristate::True) {
    // Don't leave an invalid file locked when we return.
    directory.Unlock();
    return E_FAIL;
  }
  permissionsFixed = true;

  FileOrDirectory tempDir(tempPath, Lockstate::Locked);
  if (!tempDir.IsLocked() || tempDir.IsLink() != Tristate::False ||
      tempDir.IsDirectory() != Tristate::True) {
    return E_FAIL;
  }

  SimpleAutoString moveFrom;
  SimpleAutoString moveTo;
  if (!moveFrom.AllocEmpty(MAX_PATH) || !moveTo.AllocEmpty(MAX_PATH)) {
    return E_OUTOFMEMORY;
  }

  // If we fail to move one file, we still want to try for the others. This will
  // store the first error we encounter so it can be returned.
  HRESULT returnValue = S_OK;

  // Copy the contents of tempDir back to the original directory.
  DIR directoryHandle(tempPath.String());
  errno = 0;
  for (dirent* entry = readdir(&directoryHandle); entry;
       entry = readdir(&directoryHandle)) {
    if (wcscmp(entry->d_name, L".") == 0 || wcscmp(entry->d_name, L"..") == 0 ||
        tempDir.LockFilenameMatches(entry->d_name)) {
      continue;
    }

    moveFrom.AssignSprintf(MAX_PATH + 1, L"%s\\%s", tempPath.String(),
                           entry->d_name);
    if (moveFrom.Length() == 0) {
      returnValue = FAILED(returnValue)
                        ? returnValue
                        : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
      continue;
    }

    moveTo.AssignSprintf(MAX_PATH + 1, L"%s\\%s", path.String(), entry->d_name);
    if (moveTo.Length() == 0) {
      returnValue = FAILED(returnValue)
                        ? returnValue
                        : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
      continue;
    }

    hrv = MoveFileOrDir(moveFrom, moveTo, perms);
    if (FAILED(hrv)) {
      returnValue = FAILED(returnValue) ? returnValue : hrv;
    }

    // Before looping, clear any errors that might have been encountered so we
    // can correctly get errors from readdir.
    errno = 0;
  }
  if (errno != 0) {
    returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
  }

  hrv = RemoveRecursive(tempPath, tempDir);
  returnValue = FAILED(returnValue) ? returnValue : hrv;

  return returnValue;
}

/**
 * This function moves a file or directory from one location to another.
 * Sometimes it cannot be moved because something (probably anti-virus) has
 * opened it. In that case, we copy the file and attempt to remove the original.
 *
 * If the file cannot be copied, this function will try to remove the original
 * anyway.
 */
static HRESULT MoveFileOrDir(const SimpleAutoString& moveFrom,
                             const SimpleAutoString& moveTo,
                             const AutoPerms& perms) {
  BOOL success = MoveFileW(moveFrom.String(), moveTo.String());
  if (success) {
    return S_OK;
  }

  FileOrDirectory fileToMove(moveFrom, Lockstate::Locked);

  // If we fail to move one file, we still want to try for the others. This will
  // store the first error we encounter so it can be returned.
  HRESULT returnValue = S_OK;

  if (fileToMove.IsDirectory() != Tristate::True) {
    fileToMove.Unlock();
    if (fileToMove.IsLink() == Tristate::False) {
      success = CopyFileW(moveFrom.String(), moveTo.String(), TRUE);
      if (!success) {
        returnValue = FAILED(returnValue) ? returnValue
                                          : HRESULT_FROM_WIN32(GetLastError());
      }
    }
    success = DeleteFileW(moveFrom.String());
    if (!success) {
      // If we failed to delete it, try having it removed at reboot.
      success =
          MoveFileExW(moveFrom.String(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
      if (!success) {
        returnValue = FAILED(returnValue) ? returnValue
                                          : HRESULT_FROM_WIN32(GetLastError());
      }
    }
    return returnValue;
  }  // Done handling files. The rest of this function is for moving a
     // directory.

  success = CreateDirectoryW(moveTo.String(), const_cast<LPSECURITY_ATTRIBUTES>(
                                                  &perms.securityAttributes));
  if (!success) {
    return HRESULT_FROM_WIN32(GetLastError());
  }
  FileOrDirectory destDir(moveTo, Lockstate::Locked);

  SimpleAutoString childPath;
  SimpleAutoString childDestPath;
  if (!childPath.AllocEmpty(MAX_PATH) || !childDestPath.AllocEmpty(MAX_PATH)) {
    return E_OUTOFMEMORY;
  }

  if (!fileToMove.IsLocked() || !destDir.IsLocked() ||
      destDir.IsDirectory() != Tristate::True ||
      destDir.IsLink() != Tristate::False) {
    returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
  } else if (fileToMove.IsLink() == Tristate::False) {
    DIR directoryHandle(moveFrom.String());
    errno = 0;
    for (dirent* entry = readdir(&directoryHandle); entry;
         entry = readdir(&directoryHandle)) {
      if (wcscmp(entry->d_name, L".") == 0 ||
          wcscmp(entry->d_name, L"..") == 0 ||
          fileToMove.LockFilenameMatches(entry->d_name)) {
        continue;
      }

      childPath.AssignSprintf(MAX_PATH + 1, L"%s\\%s", moveFrom.String(),
                              entry->d_name);
      if (childPath.Length() == 0) {
        returnValue = FAILED(returnValue)
                          ? returnValue
                          : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        continue;
      }

      childDestPath.AssignSprintf(MAX_PATH + 1, L"%s\\%s", moveTo.String(),
                                  entry->d_name);
      if (childDestPath.Length() == 0) {
        returnValue = FAILED(returnValue)
                          ? returnValue
                          : HRESULT_FROM_WIN32(ERROR_BUFFER_OVERFLOW);
        continue;
      }

      HRESULT hrv = MoveFileOrDir(childPath, childDestPath, perms);
      if (FAILED(hrv)) {
        returnValue = FAILED(returnValue) ? returnValue : hrv;
      }

      // Before looping, clear any errors that might have been encountered so we
      // can correctly get errors from readdir.
      errno = 0;
    }
    if (errno != 0) {
      returnValue = FAILED(returnValue) ? returnValue : E_FAIL;
    }
  }

  // Everything has been copied out of the directory. Now remove it.
  HRESULT hrv = RemoveRecursive(moveFrom, fileToMove);
  if (FAILED(hrv)) {
    // If we failed to remove it, try having it removed on reboot.
    success =
        MoveFileExW(moveFrom.String(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    if (!success) {
      returnValue = FAILED(returnValue) ? returnValue
                                        : HRESULT_FROM_WIN32(GetLastError());
    }
  }

  return returnValue;
}

/**
 * Splits an absolute path into its parent directory and filename.
 * For example, splits path="C:\foo\bar" into parentPath="C:\foo" and
 * filename="bar".
 */
static HRESULT SplitPath(const SimpleAutoString& path,
                         SimpleAutoString& parentPath,
                         SimpleAutoString& filename) {
  HRESULT hrv = parentPath.CopyFrom(path);
  if (FAILED(hrv) || parentPath.Length() == 0) {
    return hrv;
  }

  hrv = GetFilename(parentPath, filename);
  if (FAILED(hrv)) {
    return hrv;
  }

  size_t parentPathLen = parentPath.Length();
  if (parentPathLen < filename.Length() + 1) {
    return E_FAIL;
  }
  parentPathLen -= filename.Length() + 1;
  parentPath.Truncate(parentPathLen);
  if (parentPath.Length() == 0) {
    return E_FAIL;
  }

  return S_OK;
}

/**
 * Gets the filename of the given path. Also removes trailing path separators
 * from the input path.
 * Ex: If path="C:\foo\bar", filename="bar"
 */
static HRESULT GetFilename(SimpleAutoString& path, SimpleAutoString& filename) {
  // Remove trailing path separators.
  size_t pathLen = path.Length();
  if (pathLen == 0) {
    return E_FAIL;
  }
  wchar_t lastChar = path.String()[pathLen - 1];
  while (lastChar == '/' || lastChar == '\\') {
    --pathLen;
    path.Truncate(pathLen);
    if (pathLen == 0) {
      return E_FAIL;
    }
    lastChar = path.String()[pathLen - 1];
  }

  const wchar_t* separator1 = wcsrchr(path.String(), '/');
  const wchar_t* separator2 = wcsrchr(path.String(), '\\');
  const wchar_t* separator =
      (separator1 > separator2) ? separator1 : separator2;
  if (separator == nullptr) {
    return E_FAIL;
  }

  HRESULT hrv = filename.CopyFrom(separator + 1);
  if (FAILED(hrv) || filename.Length() == 0) {
    return E_FAIL;
  }
  return S_OK;
}

/**
 * Returns true if the path conflicts with the leaf path.
 */
static bool PathConflictsWithLeaf(const SimpleAutoString& path,
                                  const SimpleAutoString& leafPath) {
  if (!leafPath.StartsWith(path)) {
    return false;
  }
  // Make sure that the next character after the path ends is a path separator
  // or the end of the string. We don't want to say that "C:\f" conflicts with
  // "C:\foo\bar".
  wchar_t charAfterPath = leafPath.String()[path.Length()];
  return (charAfterPath == L'\\' || charAfterPath == L'\0');
}
#endif  // XP_WIN
