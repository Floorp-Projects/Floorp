/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Implementation of nsIFile for "unixy" systems.
 */

#include "nsLocalFile.h"

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/CheckedInt.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/Sprintf.h"
#include "mozilla/FilePreferences.h"
#include "prtime.h"

#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>

#if defined(XP_MACOSX)
#  include <sys/xattr.h>
#endif

#if defined(USE_LINUX_QUOTACTL)
#  include <sys/mount.h>
#  include <sys/quota.h>
#  include <sys/sysmacros.h>
#  ifndef BLOCK_SIZE
#    define BLOCK_SIZE 1024 /* kernel block size */
#  endif
#endif

#include "nsDirectoryServiceDefs.h"
#include "nsCOMPtr.h"
#include "nsIFile.h"
#include "nsString.h"
#include "nsIDirectoryEnumerator.h"
#include "nsSimpleEnumerator.h"
#include "private/pprio.h"
#include "prlink.h"

#ifdef MOZ_WIDGET_GTK
#  include "nsIGIOService.h"
#endif

#ifdef MOZ_WIDGET_COCOA
#  include <Carbon/Carbon.h>
#  include "CocoaFileUtils.h"
#  include "prmem.h"
#  include "plbase64.h"

static nsresult MacErrorMapper(OSErr inErr);
#endif

#ifdef MOZ_WIDGET_ANDROID
#  include "mozilla/java/GeckoAppShellWrappers.h"
#  include "nsIMIMEService.h"
#  include <linux/magic.h>
#endif

#include "nsNativeCharsetUtils.h"
#include "nsTraceRefcnt.h"

/**
 *  we need these for statfs()
 */
#ifdef HAVE_SYS_STATVFS_H
#  if defined(__osf__) && defined(__DECCXX)
extern "C" int statvfs(const char*, struct statvfs*);
#  endif
#  include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#  include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_VFS_H
#  include <sys/vfs.h>
#endif

#if defined(HAVE_STATVFS64) && (!defined(LINUX) && !defined(__osf__))
#  define STATFS statvfs64
#  define F_BSIZE f_frsize
#elif defined(HAVE_STATVFS) && (!defined(LINUX) && !defined(__osf__))
#  define STATFS statvfs
#  define F_BSIZE f_frsize
#elif defined(HAVE_STATFS64)
#  define STATFS statfs64
#  define F_BSIZE f_bsize
#elif defined(HAVE_STATFS)
#  define STATFS statfs
#  define F_BSIZE f_bsize
#endif

using namespace mozilla;

#define ENSURE_STAT_CACHE()                            \
  do {                                                 \
    if (!FillStatCache()) return NSRESULT_FOR_ERRNO(); \
  } while (0)

#define CHECK_mPath()                                     \
  do {                                                    \
    if (mPath.IsEmpty()) return NS_ERROR_NOT_INITIALIZED; \
    if (!FilePreferences::IsAllowedPath(mPath))           \
      return NS_ERROR_FILE_ACCESS_DENIED;                 \
  } while (0)

static PRTime TimespecToMillis(const struct timespec& aTimeSpec) {
  return PRTime(aTimeSpec.tv_sec) * PR_MSEC_PER_SEC +
         PRTime(aTimeSpec.tv_nsec) / PR_NSEC_PER_MSEC;
}

/* directory enumerator */
class nsDirEnumeratorUnix final : public nsSimpleEnumerator,
                                  public nsIDirectoryEnumerator {
 public:
  nsDirEnumeratorUnix();

  // nsISupports interface
  NS_DECL_ISUPPORTS_INHERITED

  // nsISimpleEnumerator interface
  NS_DECL_NSISIMPLEENUMERATOR

  // nsIDirectoryEnumerator interface
  NS_DECL_NSIDIRECTORYENUMERATOR

  NS_IMETHOD Init(nsLocalFile* aParent, bool aIgnored);

  NS_FORWARD_NSISIMPLEENUMERATORBASE(nsSimpleEnumerator::)

  const nsID& DefaultInterface() override { return NS_GET_IID(nsIFile); }

 private:
  ~nsDirEnumeratorUnix() override;

 protected:
  NS_IMETHOD GetNextEntry();

  DIR* mDir;
  struct dirent* mEntry;
  nsCString mParentPath;
};

nsDirEnumeratorUnix::nsDirEnumeratorUnix() : mDir(nullptr), mEntry(nullptr) {}

nsDirEnumeratorUnix::~nsDirEnumeratorUnix() { Close(); }

NS_IMPL_ISUPPORTS_INHERITED(nsDirEnumeratorUnix, nsSimpleEnumerator,
                            nsIDirectoryEnumerator)

NS_IMETHODIMP
nsDirEnumeratorUnix::Init(nsLocalFile* aParent,
                          bool aResolveSymlinks /*ignored*/) {
  nsAutoCString dirPath;
  if (NS_FAILED(aParent->GetNativePath(dirPath)) || dirPath.IsEmpty()) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  // When enumerating the directory, the paths must have a slash at the end.
  nsAutoCString dirPathWithSlash(dirPath);
  dirPathWithSlash.Append('/');
  if (!FilePreferences::IsAllowedPath(dirPathWithSlash)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  if (NS_FAILED(aParent->GetNativePath(mParentPath))) {
    return NS_ERROR_FAILURE;
  }

  mDir = opendir(dirPath.get());
  if (!mDir) {
    return NSRESULT_FOR_ERRNO();
  }
  return GetNextEntry();
}

NS_IMETHODIMP
nsDirEnumeratorUnix::HasMoreElements(bool* aResult) {
  *aResult = mDir && mEntry;
  if (!*aResult) {
    Close();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsDirEnumeratorUnix::GetNext(nsISupports** aResult) {
  nsCOMPtr<nsIFile> file;
  nsresult rv = GetNextFile(getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!file) {
    return NS_ERROR_FAILURE;
  }
  file.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsDirEnumeratorUnix::GetNextEntry() {
  do {
    errno = 0;
    mEntry = readdir(mDir);

    // end of dir or error
    if (!mEntry) {
      return NSRESULT_FOR_ERRNO();
    }

    // keep going past "." and ".."
  } while (mEntry->d_name[0] == '.' &&
           (mEntry->d_name[1] == '\0' ||                                // .\0
            (mEntry->d_name[1] == '.' && mEntry->d_name[2] == '\0')));  // ..\0
  return NS_OK;
}

NS_IMETHODIMP
nsDirEnumeratorUnix::GetNextFile(nsIFile** aResult) {
  nsresult rv;
  if (!mDir || !mEntry) {
    *aResult = nullptr;
    return NS_OK;
  }

  nsCOMPtr<nsIFile> file = new nsLocalFile();

  if (NS_FAILED(rv = file->InitWithNativePath(mParentPath)) ||
      NS_FAILED(rv = file->AppendNative(nsDependentCString(mEntry->d_name)))) {
    return rv;
  }

  file.forget(aResult);
  return GetNextEntry();
}

NS_IMETHODIMP
nsDirEnumeratorUnix::Close() {
  if (mDir) {
    closedir(mDir);
    mDir = nullptr;
  }
  return NS_OK;
}

nsLocalFile::nsLocalFile() : mCachedStat() {}

nsLocalFile::nsLocalFile(const nsACString& aFilePath) : mCachedStat() {
  InitWithNativePath(aFilePath);
}

nsLocalFile::nsLocalFile(const nsLocalFile& aOther) : mPath(aOther.mPath) {}

#ifdef MOZ_WIDGET_COCOA
NS_IMPL_ISUPPORTS(nsLocalFile, nsILocalFileMac, nsIFile)
#else
NS_IMPL_ISUPPORTS(nsLocalFile, nsIFile)
#endif

nsresult nsLocalFile::nsLocalFileConstructor(const nsIID& aIID,
                                             void** aInstancePtr) {
  if (NS_WARN_IF(!aInstancePtr)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aInstancePtr = nullptr;

  nsCOMPtr<nsIFile> inst = new nsLocalFile();
  return inst->QueryInterface(aIID, aInstancePtr);
}

bool nsLocalFile::FillStatCache() {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    errno = EACCES;
    return false;
  }

  if (STAT(mPath.get(), &mCachedStat) == -1) {
    // try lstat it may be a symlink
    if (LSTAT(mPath.get(), &mCachedStat) == -1) {
      return false;
    }
  }
  return true;
}

NS_IMETHODIMP
nsLocalFile::Clone(nsIFile** aFile) {
  // Just copy-construct ourselves
  RefPtr<nsLocalFile> copy = new nsLocalFile(*this);
  copy.forget(aFile);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithNativePath(const nsACString& aFilePath) {
  if (!aFilePath.IsEmpty() && aFilePath.First() == '~') {
    if (aFilePath.Length() == 1 || aFilePath.CharAt(1) == '/') {
      // Home dir for the current user

      nsCOMPtr<nsIFile> homeDir;
      nsAutoCString homePath;
      if (NS_FAILED(NS_GetSpecialDirectory(NS_OS_HOME_DIR,
                                           getter_AddRefs(homeDir))) ||
          NS_FAILED(homeDir->GetNativePath(homePath))) {
        return NS_ERROR_FAILURE;
      }

      mPath = homePath;
      if (aFilePath.Length() > 2) {
        mPath.Append(Substring(aFilePath, 1));
      }
    } else {
      // Home dir for an arbitrary user e.g. `~foo/bar` -> `/home/foo/bar`
      // (`/Users/foo/bar` on Mac). The accurate way to get this directory
      // is with `getpwnam`, but we would like to avoid doing blocking
      // filesystem I/O while creating an `nsIFile`.

      mPath =
#ifdef XP_MACOSX
          "/Users/"_ns
#else
          "/home/"_ns
#endif
          + Substring(aFilePath, 1);
    }
  } else {
    if (aFilePath.IsEmpty() || aFilePath.First() != '/') {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    mPath = aFilePath;
  }

  if (!FilePreferences::IsAllowedPath(mPath)) {
    mPath.Truncate();
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  // trim off trailing slashes
  ssize_t len = mPath.Length();
  while ((len > 1) && (mPath[len - 1] == '/')) {
    --len;
  }
  mPath.SetLength(len);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CreateAllAncestors(uint32_t aPermissions) {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  // <jband> I promise to play nice
  char* buffer = mPath.BeginWriting();
  char* slashp = buffer;
  int mkdir_result = 0;
  int mkdir_errno;

#ifdef DEBUG_NSIFILE
  fprintf(stderr, "nsIFile: before: %s\n", buffer);
#endif

  while ((slashp = strchr(slashp + 1, '/'))) {
    /*
     * Sequences of '/' are equivalent to a single '/'.
     */
    if (slashp[1] == '/') {
      continue;
    }

    /*
     * If the path has a trailing slash, don't make the last component,
     * because we'll get EEXIST in Create when we try to build the final
     * component again, and it's easier to condition the logic here than
     * there.
     */
    if (slashp[1] == '\0') {
      break;
    }

    /* Temporarily NUL-terminate here */
    *slashp = '\0';
#ifdef DEBUG_NSIFILE
    fprintf(stderr, "nsIFile: mkdir(\"%s\")\n", buffer);
#endif
    mkdir_result = mkdir(buffer, aPermissions);
    if (mkdir_result == -1) {
      mkdir_errno = errno;
      /*
       * Always set |errno| to EEXIST if the dir already exists
       * (we have to do this here since the errno value is not consistent
       * in all cases - various reasons like different platform,
       * automounter-controlled dir, etc. can affect it (see bug 125489
       * for details)).
       */
      if (mkdir_errno != EEXIST && access(buffer, F_OK) == 0) {
        mkdir_errno = EEXIST;
      }
#ifdef DEBUG_NSIFILE
      fprintf(stderr, "nsIFile: errno: %d\n", mkdir_errno);
#endif
    }

    /* Put the / back */
    *slashp = '/';
  }

  /*
   * We could get EEXIST for an existing file -- not directory --
   * but that's OK: we'll get ENOTDIR when we try to make the final
   * component of the path back in Create and error out appropriately.
   */
  if (mkdir_result == -1 && mkdir_errno != EEXIST) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDesc(int32_t aFlags, int32_t aMode,
                              PRFileDesc** aResult) {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }
  *aResult = PR_Open(mPath.get(), aFlags, aMode);
  if (!*aResult) {
    return NS_ErrorAccordingToNSPR();
  }

  if (aFlags & DELETE_ON_CLOSE) {
    PR_Delete(mPath.get());
  }

#if defined(HAVE_POSIX_FADVISE)
  if (aFlags & OS_READAHEAD) {
    posix_fadvise(PR_FileDesc2NativeHandle(*aResult), 0, 0,
                  POSIX_FADV_SEQUENTIAL);
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char* aMode, FILE** aResult) {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }
  *aResult = fopen(mPath.get(), aMode);
  if (!*aResult) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

static int do_create(const char* aPath, int aFlags, mode_t aMode,
                     PRFileDesc** aResult) {
  *aResult = PR_Open(aPath, aFlags, aMode);
  return *aResult ? 0 : -1;
}

static int do_mkdir(const char* aPath, int aFlags, mode_t aMode,
                    PRFileDesc** aResult) {
  *aResult = nullptr;
  return mkdir(aPath, aMode);
}

nsresult nsLocalFile::CreateAndKeepOpen(uint32_t aType, int aFlags,
                                        uint32_t aPermissions,
                                        bool aSkipAncestors,
                                        PRFileDesc** aResult) {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  if (aType != NORMAL_FILE_TYPE && aType != DIRECTORY_TYPE) {
    return NS_ERROR_FILE_UNKNOWN_TYPE;
  }

  int (*createFunc)(const char*, int, mode_t, PRFileDesc**) =
      (aType == NORMAL_FILE_TYPE) ? do_create : do_mkdir;

  int result = createFunc(mPath.get(), aFlags, aPermissions, aResult);
  if (result == -1 && errno == ENOENT && !aSkipAncestors) {
    /*
     * If we failed because of missing ancestor components, try to create
     * them and then retry the original creation.
     *
     * Ancestor directories get the same permissions as the file we're
     * creating, with the X bit set for each of (user,group,other) with
     * an R bit in the original permissions.    If you want to do anything
     * fancy like setgid or sticky bits, do it by hand.
     */
    int dirperm = aPermissions;
    if (aPermissions & S_IRUSR) {
      dirperm |= S_IXUSR;
    }
    if (aPermissions & S_IRGRP) {
      dirperm |= S_IXGRP;
    }
    if (aPermissions & S_IROTH) {
      dirperm |= S_IXOTH;
    }

#ifdef DEBUG_NSIFILE
    fprintf(stderr, "nsIFile: perm = %o, dirperm = %o\n", aPermissions,
            dirperm);
#endif

    if (NS_FAILED(CreateAllAncestors(dirperm))) {
      return NS_ERROR_FAILURE;
    }

#ifdef DEBUG_NSIFILE
    fprintf(stderr, "nsIFile: Create(\"%s\") again\n", mPath.get());
#endif
    result = createFunc(mPath.get(), aFlags, aPermissions, aResult);
  }
  return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP
nsLocalFile::Create(uint32_t aType, uint32_t aPermissions,
                    bool aSkipAncestors) {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  PRFileDesc* junk = nullptr;
  nsresult rv = CreateAndKeepOpen(
      aType, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE | PR_EXCL, aPermissions,
      aSkipAncestors, &junk);
  if (junk) {
    PR_Close(junk);
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString& aFragment) {
  if (aFragment.IsEmpty()) {
    return NS_OK;
  }

  // only one component of path can be appended and cannot append ".."
  nsACString::const_iterator begin, end;
  if (aFragment.EqualsASCII("..") ||
      FindCharInReadable('/', aFragment.BeginReading(begin),
                         aFragment.EndReading(end))) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  return AppendRelativeNativePath(aFragment);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString& aFragment) {
  if (aFragment.IsEmpty()) {
    return NS_OK;
  }

  // No leading '/' and cannot be ".."
  if (aFragment.First() == '/' || aFragment.EqualsASCII("..")) {
    return NS_ERROR_FILE_UNRECOGNIZED_PATH;
  }

  if (aFragment.Contains('/')) {
    // can't contain .. as a path component. Ensure that the valid components
    // "foo..foo", "..foo", and "foo.." are not falsely detected,
    // but the invalid paths "../", "foo/..", "foo/../foo",
    // "../foo", etc are.
    constexpr auto doubleDot = "/.."_ns;
    nsACString::const_iterator start, end, offset;
    aFragment.BeginReading(start);
    aFragment.EndReading(end);
    offset = end;
    while (FindInReadable(doubleDot, start, offset)) {
      if (offset == end || *offset == '/') {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
      }
      start = offset;
      offset = end;
    }

    // catches the remaining cases of prefixes
    if (StringBeginsWith(aFragment, "../"_ns)) {
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
  }

  if (!mPath.EqualsLiteral("/")) {
    mPath.Append('/');
  }
  mPath.Append(aFragment);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize() {
  char resolved_path[PATH_MAX] = "";
  char* resolved_path_ptr = nullptr;

  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  resolved_path_ptr = realpath(mPath.get(), resolved_path);

  // if there is an error, the return is null.
  if (!resolved_path_ptr) {
    return NSRESULT_FOR_ERRNO();
  }

  mPath = resolved_path;
  return NS_OK;
}

void nsLocalFile::LocateNativeLeafName(nsACString::const_iterator& aBegin,
                                       nsACString::const_iterator& aEnd) {
  // XXX perhaps we should cache this??

  mPath.BeginReading(aBegin);
  mPath.EndReading(aEnd);

  nsACString::const_iterator it = aEnd;
  nsACString::const_iterator stop = aBegin;
  --stop;
  while (--it != stop) {
    if (*it == '/') {
      aBegin = ++it;
      return;
    }
  }
  // else, the entire path is the leaf name (which means this
  // isn't an absolute path... unexpected??)
}

NS_IMETHODIMP
nsLocalFile::GetNativeLeafName(nsACString& aLeafName) {
  nsACString::const_iterator begin, end;
  LocateNativeLeafName(begin, end);
  aLeafName = Substring(begin, end);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetNativeLeafName(const nsACString& aLeafName) {
  nsACString::const_iterator begin, end;
  LocateNativeLeafName(begin, end);
  mPath.Replace(begin.get() - mPath.get(), Distance(begin, end), aLeafName);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDisplayName(nsAString& aLeafName) {
  return GetLeafName(aLeafName);
}

nsCString nsLocalFile::NativePath() { return mPath; }

nsresult nsIFile::GetNativePath(nsACString& aResult) {
  aResult = NativePath();
  return NS_OK;
}

nsCString nsIFile::HumanReadablePath() {
  nsCString path;
  DebugOnly<nsresult> rv = GetNativePath(path);
  MOZ_ASSERT(NS_SUCCEEDED(rv));
  return path;
}

nsresult nsLocalFile::GetNativeTargetPathName(nsIFile* aNewParent,
                                              const nsACString& aNewName,
                                              nsACString& aResult) {
  nsresult rv;
  nsCOMPtr<nsIFile> oldParent;

  if (!aNewParent) {
    if (NS_FAILED(rv = GetParent(getter_AddRefs(oldParent)))) {
      return rv;
    }
    aNewParent = oldParent.get();
  } else {
    // check to see if our target directory exists
    bool targetExists;
    if (NS_FAILED(rv = aNewParent->Exists(&targetExists))) {
      return rv;
    }

    if (!targetExists) {
      // XXX create the new directory with some permissions
      rv = aNewParent->Create(DIRECTORY_TYPE, 0755);
      if (NS_FAILED(rv)) {
        return rv;
      }
    } else {
      // make sure that the target is actually a directory
      bool targetIsDirectory;
      if (NS_FAILED(rv = aNewParent->IsDirectory(&targetIsDirectory))) {
        return rv;
      }
      if (!targetIsDirectory) {
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;
      }
    }
  }

  nsACString::const_iterator nameBegin, nameEnd;
  if (!aNewName.IsEmpty()) {
    aNewName.BeginReading(nameBegin);
    aNewName.EndReading(nameEnd);
  } else {
    LocateNativeLeafName(nameBegin, nameEnd);
  }

  nsAutoCString dirName;
  if (NS_FAILED(rv = aNewParent->GetNativePath(dirName))) {
    return rv;
  }

  aResult = dirName + "/"_ns + Substring(nameBegin, nameEnd);
  return NS_OK;
}

nsresult nsLocalFile::CopyDirectoryTo(nsIFile* aNewParent) {
  nsresult rv;
  /*
   * dirCheck is used for various boolean test results such as from Equals,
   * Exists, isDir, etc.
   */
  bool dirCheck, isSymlink;
  uint32_t oldPerms;

  if (NS_FAILED(rv = IsDirectory(&dirCheck))) {
    return rv;
  }
  if (!dirCheck) {
    return CopyToNative(aNewParent, ""_ns);
  }

  if (NS_FAILED(rv = Equals(aNewParent, &dirCheck))) {
    return rv;
  }
  if (dirCheck) {
    // can't copy dir to itself
    return NS_ERROR_INVALID_ARG;
  }

  if (NS_FAILED(rv = aNewParent->Exists(&dirCheck))) {
    return rv;
  }
  // get the dirs old permissions
  if (NS_FAILED(rv = GetPermissions(&oldPerms))) {
    return rv;
  }
  if (!dirCheck) {
    if (NS_FAILED(rv = aNewParent->Create(DIRECTORY_TYPE, oldPerms))) {
      return rv;
    }
  } else {  // dir exists lets try to use leaf
    nsAutoCString leafName;
    if (NS_FAILED(rv = GetNativeLeafName(leafName))) {
      return rv;
    }
    if (NS_FAILED(rv = aNewParent->AppendNative(leafName))) {
      return rv;
    }
    if (NS_FAILED(rv = aNewParent->Exists(&dirCheck))) {
      return rv;
    }
    if (dirCheck) {
      return NS_ERROR_FILE_ALREADY_EXISTS;  // dest exists
    }
    if (NS_FAILED(rv = aNewParent->Create(DIRECTORY_TYPE, oldPerms))) {
      return rv;
    }
  }

  nsCOMPtr<nsIDirectoryEnumerator> dirIterator;
  if (NS_FAILED(rv = GetDirectoryEntries(getter_AddRefs(dirIterator)))) {
    return rv;
  }

  nsCOMPtr<nsIFile> entry;
  while (NS_SUCCEEDED(dirIterator->GetNextFile(getter_AddRefs(entry))) &&
         entry) {
    if (NS_FAILED(rv = entry->IsSymlink(&isSymlink))) {
      return rv;
    }
    if (NS_FAILED(rv = entry->IsDirectory(&dirCheck))) {
      return rv;
    }
    if (dirCheck && !isSymlink) {
      nsCOMPtr<nsIFile> destClone;
      rv = aNewParent->Clone(getter_AddRefs(destClone));
      if (NS_SUCCEEDED(rv)) {
        if (NS_FAILED(rv = entry->CopyToNative(destClone, ""_ns))) {
#ifdef DEBUG
          nsresult rv2;
          nsAutoCString pathName;
          if (NS_FAILED(rv2 = entry->GetNativePath(pathName))) {
            return rv2;
          }
          printf("Operation not supported: %s\n", pathName.get());
#endif
          if (rv == NS_ERROR_OUT_OF_MEMORY) {
            return rv;
          }
          continue;
        }
      }
    } else {
      if (NS_FAILED(rv = entry->CopyToNative(aNewParent, ""_ns))) {
#ifdef DEBUG
        nsresult rv2;
        nsAutoCString pathName;
        if (NS_FAILED(rv2 = entry->GetNativePath(pathName))) {
          return rv2;
        }
        printf("Operation not supported: %s\n", pathName.get());
#endif
        if (rv == NS_ERROR_OUT_OF_MEMORY) {
          return rv;
        }
        continue;
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyToNative(nsIFile* aNewParent, const nsACString& aNewName) {
  nsresult rv;
  // check to make sure that this has been initialized properly
  CHECK_mPath();

  // we copy the parent here so 'aNewParent' remains immutable
  nsCOMPtr<nsIFile> workParent;
  if (aNewParent) {
    if (NS_FAILED(rv = aNewParent->Clone(getter_AddRefs(workParent)))) {
      return rv;
    }
  } else {
    if (NS_FAILED(rv = GetParent(getter_AddRefs(workParent)))) {
      return rv;
    }
  }

  // check to see if we are a directory or if we are a file
  bool isDirectory;
  if (NS_FAILED(rv = IsDirectory(&isDirectory))) {
    return rv;
  }

  nsAutoCString newPathName;
  if (isDirectory) {
    if (!aNewName.IsEmpty()) {
      if (NS_FAILED(rv = workParent->AppendNative(aNewName))) {
        return rv;
      }
    } else {
      if (NS_FAILED(rv = GetNativeLeafName(newPathName))) {
        return rv;
      }
      if (NS_FAILED(rv = workParent->AppendNative(newPathName))) {
        return rv;
      }
    }
    if (NS_FAILED(rv = CopyDirectoryTo(workParent))) {
      return rv;
    }
  } else {
    rv = GetNativeTargetPathName(workParent, aNewName, newPathName);
    if (NS_FAILED(rv)) {
      return rv;
    }

#ifdef DEBUG_blizzard
    printf("nsLocalFile::CopyTo() %s -> %s\n", mPath.get(), newPathName.get());
#endif

    // actually create the file.
    auto* newFile = new nsLocalFile();
    nsCOMPtr<nsIFile> fileRef(newFile);  // release on exit

    rv = newFile->InitWithNativePath(newPathName);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // get the old permissions
    uint32_t myPerms = 0;
    rv = GetPermissions(&myPerms);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // Create the new file with the old file's permissions, even if write
    // permission is missing.  We can't create with write permission and
    // then change back to myPerm on all filesystems (FAT on Linux, e.g.).
    // But we can write to a read-only file on all Unix filesystems if we
    // open it successfully for writing.

    PRFileDesc* newFD;
    rv = newFile->CreateAndKeepOpen(
        NORMAL_FILE_TYPE, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE, myPerms,
        /* aSkipAncestors = */ false, &newFD);
    if (NS_FAILED(rv)) {
      return rv;
    }

    // open the old file, too
    bool specialFile;
    if (NS_FAILED(rv = IsSpecial(&specialFile))) {
      PR_Close(newFD);
      return rv;
    }
    if (specialFile) {
#ifdef DEBUG
      printf("Operation not supported: %s\n", mPath.get());
#endif
      // make sure to clean up properly
      PR_Close(newFD);
      return NS_OK;
    }

#if defined(XP_MACOSX)
    bool quarantined = true;
    (void)HasXAttr("com.apple.quarantine"_ns, &quarantined);
#endif

    PRFileDesc* oldFD;
    rv = OpenNSPRFileDesc(PR_RDONLY, myPerms, &oldFD);
    if (NS_FAILED(rv)) {
      // make sure to clean up properly
      PR_Close(newFD);
      return rv;
    }

#ifdef DEBUG_blizzard
    int32_t totalRead = 0;
    int32_t totalWritten = 0;
#endif
    char buf[BUFSIZ];
    int32_t bytesRead;

    // record PR_Write() error for better error message later.
    nsresult saved_write_error = NS_OK;
    nsresult saved_read_error = NS_OK;
    nsresult saved_read_close_error = NS_OK;
    nsresult saved_write_close_error = NS_OK;

    // DONE: Does PR_Read() return bytesRead < 0 for error?
    // Yes., The errors from PR_Read are not so common and
    // the value may not have correspondence in NS_ERROR_*, but
    // we do catch it still, immediately after while() loop.
    // We can differentiate errors pf PR_Read and PR_Write by
    // looking at saved_write_error value. If PR_Write error occurs (and not
    // PR_Read() error), save_write_error is not NS_OK.

    while ((bytesRead = PR_Read(oldFD, buf, BUFSIZ)) > 0) {
#ifdef DEBUG_blizzard
      totalRead += bytesRead;
#endif

      // PR_Write promises never to do a short write
      int32_t bytesWritten = PR_Write(newFD, buf, bytesRead);
      if (bytesWritten < 0) {
        saved_write_error = NSRESULT_FOR_ERRNO();
        bytesRead = -1;
        break;
      }
      NS_ASSERTION(bytesWritten == bytesRead, "short PR_Write?");

#ifdef DEBUG_blizzard
      totalWritten += bytesWritten;
#endif
    }

    // TODO/FIXME: If CIFS (and NFS?) may force read/write to return EINTR,
    // we are better off to prepare for retrying. But we need confirmation if
    // EINTR is returned.

    // Record error if PR_Read() failed.
    // Must be done before any other I/O which may reset errno.
    if (bytesRead < 0 && saved_write_error == NS_OK) {
      saved_read_error = NSRESULT_FOR_ERRNO();
    }

#ifdef DEBUG_blizzard
    printf("read %d bytes, wrote %d bytes\n", totalRead, totalWritten);
#endif

    // DONE: Errors of close can occur.  Read man page of
    // close(2);
    // This is likely to happen if the file system is remote file
    // system (NFS, CIFS, etc.) and network outage occurs.
    // At least, we should tell the user that filesystem/disk is
    // hosed (possibly due to network error, hard disk failure,
    // etc.) so that users can take remedial action.

    // close the files
    if (PR_Close(newFD) < 0) {
      saved_write_close_error = NSRESULT_FOR_ERRNO();
#if DEBUG
      // This error merits printing.
      fprintf(stderr, "ERROR: PR_Close(newFD) returned error. errno = %d\n",
              errno);
#endif
    }
#if defined(XP_MACOSX)
    else if (!quarantined) {
      // If the original file was not in quarantine, lift the quarantine that
      // file creation added because of LSFileQuarantineEnabled.
      (void)newFile->DelXAttr("com.apple.quarantine"_ns);
    }
#endif  // defined(XP_MACOSX)

    if (PR_Close(oldFD) < 0) {
      saved_read_close_error = NSRESULT_FOR_ERRNO();
#if DEBUG
      fprintf(stderr, "ERROR: PR_Close(oldFD) returned error. errno = %d\n",
              errno);
#endif
    }

    // Let us report the failure to write and read.
    // check for write/read error after cleaning up
    if (bytesRead < 0) {
      if (saved_write_error != NS_OK) {
        return saved_write_error;
      }
      if (saved_read_error != NS_OK) {
        return saved_read_error;
      }
#if DEBUG
      MOZ_ASSERT(0);
#endif
    }

    if (saved_write_close_error != NS_OK) {
      return saved_write_close_error;
    }
    if (saved_read_close_error != NS_OK) {
      return saved_read_close_error;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile* aNewParent,
                                        const nsACString& aNewName) {
  return CopyToNative(aNewParent, aNewName);
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile* aNewParent, const nsACString& aNewName) {
  nsresult rv;

  // check to make sure that this has been initialized properly
  CHECK_mPath();

  // check to make sure that we have a new parent
  nsAutoCString newPathName;
  rv = GetNativeTargetPathName(aNewParent, aNewName, newPathName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!FilePreferences::IsAllowedPath(newPathName)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  // try for atomic rename, falling back to copy/delete
  if (rename(mPath.get(), newPathName.get()) < 0) {
    if (errno == EXDEV) {
      rv = CopyToNative(aNewParent, aNewName);
      if (NS_SUCCEEDED(rv)) {
        rv = Remove(true);
      }
    } else {
      rv = NSRESULT_FOR_ERRNO();
    }
  }

  if (NS_SUCCEEDED(rv)) {
    // Adjust this
    mPath = newPathName;
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveToFollowingLinksNative(nsIFile* aNewParent,
                                        const nsACString& aNewName) {
  return MoveToNative(aNewParent, aNewName);
}

NS_IMETHODIMP
nsLocalFile::Remove(bool aRecursive, uint32_t* aRemoveCount) {
  CHECK_mPath();
  ENSURE_STAT_CACHE();

  bool isSymLink;

  nsresult rv = IsSymlink(&isSymLink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (isSymLink || !S_ISDIR(mCachedStat.st_mode)) {
    rv = NSRESULT_FOR_RETURN(unlink(mPath.get()));
    if (NS_SUCCEEDED(rv) && aRemoveCount) {
      *aRemoveCount += 1;
    }
    return rv;
  }

  if (aRecursive) {
    auto* dir = new nsDirEnumeratorUnix();

    RefPtr<nsSimpleEnumerator> dirRef(dir);  // release on exit

    rv = dir->Init(this, false);
    if (NS_FAILED(rv)) {
      return rv;
    }

    bool more;
    while (NS_SUCCEEDED(dir->HasMoreElements(&more)) && more) {
      nsCOMPtr<nsISupports> item;
      rv = dir->GetNext(getter_AddRefs(item));
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }

      nsCOMPtr<nsIFile> file = do_QueryInterface(item, &rv);
      if (NS_FAILED(rv)) {
        return NS_ERROR_FAILURE;
      }
      // XXX: We care the result of the removal here while
      // nsLocalFileWin does not. We should align the behavior. (bug 1779696)
      rv = file->Remove(aRecursive, aRemoveCount);

#ifdef ANDROID
      // See bug 580434 - Bionic gives us just deleted files
      if (rv == NS_ERROR_FILE_NOT_FOUND) {
        continue;
      }
#endif
      if (NS_FAILED(rv)) {
        return rv;
      }
    }
  }

  rv = NSRESULT_FOR_RETURN(rmdir(mPath.get()));
  if (NS_SUCCEEDED(rv) && aRemoveCount) {
    *aRemoveCount += 1;
  }
  return rv;
}

nsresult nsLocalFile::GetTimeImpl(PRTime* aTime,
                                  nsLocalFile::TimeField aTimeField,
                                  bool aFollowLinks) {
  CHECK_mPath();
  if (NS_WARN_IF(!aTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  using StatFn = int (*)(const char*, struct STAT*);
  StatFn statFn = aFollowLinks ? &STAT : &LSTAT;

  struct STAT fileStats {};
  if (statFn(mPath.get(), &fileStats) < 0) {
    return NSRESULT_FOR_ERRNO();
  }

  struct timespec* timespec;
  switch (aTimeField) {
    case TimeField::AccessedTime:
#if (defined(__APPLE__) && defined(__MACH__))
      timespec = &fileStats.st_atimespec;
#else
      timespec = &fileStats.st_atim;
#endif
      break;

    case TimeField::ModifiedTime:
#if (defined(__APPLE__) && defined(__MACH__))
      timespec = &fileStats.st_mtimespec;
#else
      timespec = &fileStats.st_mtim;
#endif
      break;

    default:
      MOZ_CRASH("Unknown TimeField");
  }

  *aTime = TimespecToMillis(*timespec);

  return NS_OK;
}

nsresult nsLocalFile::SetTimeImpl(PRTime aTime,
                                  nsLocalFile::TimeField aTimeField,
                                  bool aFollowLinks) {
  CHECK_mPath();

  using UtimesFn = int (*)(const char*, const timeval*);
  UtimesFn utimesFn = &utimes;

#if HAVE_LUTIMES
  if (!aFollowLinks) {
    utimesFn = &lutimes;
  }
#endif

  ENSURE_STAT_CACHE();

  if (aTime == 0) {
    aTime = PR_Now();
  }

  // We only want to write to a single field (accessed time or modified time),
  // but utimes() doesn't let you omit one. If you do, it will set that field to
  // the current time, which is not what we want.
  //
  // So what we do is write to both fields, but copy one of the fields from our
  // cached stat structure.
  //
  // If we are writing to the accessed time field, then we want to copy the
  // modified time and vice versa.

  timeval times[2];

  const size_t writeIndex = aTimeField == TimeField::AccessedTime ? 0 : 1;
  const size_t copyIndex = aTimeField == TimeField::AccessedTime ? 1 : 0;

#if (defined(__APPLE__) && defined(__MACH__))
  auto* copyFrom = aTimeField == TimeField::AccessedTime
                       ? &mCachedStat.st_mtimespec
                       : &mCachedStat.st_atimespec;
#else
  auto* copyFrom = aTimeField == TimeField::AccessedTime ? &mCachedStat.st_mtim
                                                         : &mCachedStat.st_atim;
#endif

  times[copyIndex].tv_sec = copyFrom->tv_sec;
  times[copyIndex].tv_usec = copyFrom->tv_nsec / 1000;

  times[writeIndex].tv_sec = aTime / PR_MSEC_PER_SEC;
  times[writeIndex].tv_usec = (aTime % PR_MSEC_PER_SEC) * PR_USEC_PER_MSEC;

  int result = utimesFn(mPath.get(), times);
  return NSRESULT_FOR_RETURN(result);
}

NS_IMETHODIMP
nsLocalFile::GetLastAccessedTime(PRTime* aLastAccessedTime) {
  return GetTimeImpl(aLastAccessedTime, TimeField::AccessedTime,
                     /* follow links? */ true);
}

NS_IMETHODIMP
nsLocalFile::SetLastAccessedTime(PRTime aLastAccessedTime) {
  return SetTimeImpl(aLastAccessedTime, TimeField::AccessedTime,
                     /* follow links? */ true);
}

NS_IMETHODIMP
nsLocalFile::GetLastAccessedTimeOfLink(PRTime* aLastAccessedTime) {
  return GetTimeImpl(aLastAccessedTime, TimeField::AccessedTime,
                     /* follow links? */ false);
}

NS_IMETHODIMP
nsLocalFile::SetLastAccessedTimeOfLink(PRTime aLastAccessedTime) {
  return SetTimeImpl(aLastAccessedTime, TimeField::AccessedTime,
                     /* follow links? */ false);
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTime(PRTime* aLastModTime) {
  return GetTimeImpl(aLastModTime, TimeField::ModifiedTime,
                     /* follow links? */ true);
}

NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRTime aLastModTime) {
  return SetTimeImpl(aLastModTime, TimeField::ModifiedTime,
                     /* follow links ? */ true);
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTimeOfLink(PRTime* aLastModTimeOfLink) {
  return GetTimeImpl(aLastModTimeOfLink, TimeField::ModifiedTime,
                     /* follow link? */ false);
}

NS_IMETHODIMP
nsLocalFile::SetLastModifiedTimeOfLink(PRTime aLastModTimeOfLink) {
  return SetTimeImpl(aLastModTimeOfLink, TimeField::ModifiedTime,
                     /* follow links? */ false);
}

NS_IMETHODIMP
nsLocalFile::GetCreationTime(PRTime* aCreationTime) {
  return GetCreationTimeImpl(aCreationTime, false);
}

NS_IMETHODIMP
nsLocalFile::GetCreationTimeOfLink(PRTime* aCreationTimeOfLink) {
  return GetCreationTimeImpl(aCreationTimeOfLink, /* aFollowLinks = */ true);
}

nsresult nsLocalFile::GetCreationTimeImpl(PRTime* aCreationTime,
                                          bool aFollowLinks) {
  CHECK_mPath();
  if (NS_WARN_IF(!aCreationTime)) {
    return NS_ERROR_INVALID_ARG;
  }

#if defined(_DARWIN_FEATURE_64_BIT_INODE)
  using StatFn = int (*)(const char*, struct STAT*);
  StatFn statFn = aFollowLinks ? &STAT : &LSTAT;

  struct STAT fileStats {};
  if (statFn(mPath.get(), &fileStats) < 0) {
    return NSRESULT_FOR_ERRNO();
  }

  *aCreationTime = TimespecToMillis(fileStats.st_birthtimespec);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

/*
 * Only send back permissions bits: maybe we want to send back the whole
 * mode_t to permit checks against other file types?
 */

#define NORMALIZE_PERMS(mode) ((mode) & (S_IRWXU | S_IRWXG | S_IRWXO))

NS_IMETHODIMP
nsLocalFile::GetPermissions(uint32_t* aPermissions) {
  if (NS_WARN_IF(!aPermissions)) {
    return NS_ERROR_INVALID_ARG;
  }
  ENSURE_STAT_CACHE();
  *aPermissions = NORMALIZE_PERMS(mCachedStat.st_mode);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPermissionsOfLink(uint32_t* aPermissionsOfLink) {
  CHECK_mPath();
  if (NS_WARN_IF(!aPermissionsOfLink)) {
    return NS_ERROR_INVALID_ARG;
  }

  struct STAT sbuf;
  if (LSTAT(mPath.get(), &sbuf) == -1) {
    return NSRESULT_FOR_ERRNO();
  }
  *aPermissionsOfLink = NORMALIZE_PERMS(sbuf.st_mode);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPermissions(uint32_t aPermissions) {
  CHECK_mPath();

  /*
   * Race condition here: we should use fchmod instead, there's no way to
   * guarantee the name still refers to the same file.
   */
  if (chmod(mPath.get(), aPermissions) >= 0) {
    return NS_OK;
  }
#if defined(ANDROID) && defined(STATFS)
  // For the time being, this is restricted for use by Android, but we
  // will figure out what to do for all platforms in bug 638503
  struct STATFS sfs;
  if (STATFS(mPath.get(), &sfs) < 0) {
    return NSRESULT_FOR_ERRNO();
  }

  // if this is a FAT file system we can't set file permissions
  if (sfs.f_type == MSDOS_SUPER_MAGIC) {
    return NS_OK;
  }
#endif
  return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::SetPermissionsOfLink(uint32_t aPermissions) {
  // There isn't a consistent mechanism for doing this on UNIX platforms. We
  // might want to carefully implement this in the future though.
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsLocalFile::GetFileSize(int64_t* aFileSize) {
  if (NS_WARN_IF(!aFileSize)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aFileSize = 0;
  ENSURE_STAT_CACHE();

  if (!S_ISDIR(mCachedStat.st_mode)) {
    *aFileSize = (int64_t)mCachedStat.st_size;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(int64_t aFileSize) {
  CHECK_mPath();

#if defined(ANDROID)
  /* no truncate on bionic */
  int fd = open(mPath.get(), O_WRONLY);
  if (fd == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  int ret = ftruncate(fd, (off_t)aFileSize);
  close(fd);

  if (ret == -1) {
    return NSRESULT_FOR_ERRNO();
  }
#elif defined(HAVE_TRUNCATE64)
  if (truncate64(mPath.get(), (off64_t)aFileSize) == -1) {
    return NSRESULT_FOR_ERRNO();
  }
#else
  off_t size = (off_t)aFileSize;
  if (truncate(mPath.get(), size) == -1) {
    return NSRESULT_FOR_ERRNO();
  }
#endif
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(int64_t* aFileSize) {
  CHECK_mPath();
  if (NS_WARN_IF(!aFileSize)) {
    return NS_ERROR_INVALID_ARG;
  }

  struct STAT sbuf;
  if (LSTAT(mPath.get(), &sbuf) == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  *aFileSize = (int64_t)sbuf.st_size;
  return NS_OK;
}

#if defined(USE_LINUX_QUOTACTL)
/*
 * Searches /proc/self/mountinfo for given device (Major:Minor),
 * returns exported name from /dev
 *
 * Fails when /proc/self/mountinfo or diven device don't exist.
 */
static bool GetDeviceName(unsigned int aDeviceMajor, unsigned int aDeviceMinor,
                          nsACString& aDeviceName) {
  bool ret = false;

  const int kMountInfoLineLength = 200;
  const int kMountInfoDevPosition = 6;

  char mountinfoLine[kMountInfoLineLength];
  char deviceNum[kMountInfoLineLength];

  SprintfLiteral(deviceNum, "%u:%u", aDeviceMajor, aDeviceMinor);

  FILE* f = fopen("/proc/self/mountinfo", "rt");
  if (!f) {
    return ret;
  }

  // Expects /proc/self/mountinfo in format:
  // 'ID ID major:minor root mountpoint flags - type devicename flags'
  while (fgets(mountinfoLine, kMountInfoLineLength, f)) {
    char* p_dev = strstr(mountinfoLine, deviceNum);

    for (int i = 0; i < kMountInfoDevPosition && p_dev; ++i) {
      p_dev = strchr(p_dev, ' ');
      if (p_dev) {
        p_dev++;
      }
    }

    if (p_dev) {
      char* p_dev_end = strchr(p_dev, ' ');
      if (p_dev_end) {
        *p_dev_end = '\0';
        aDeviceName.Assign(p_dev);
        ret = true;
        break;
      }
    }
  }

  fclose(f);
  return ret;
}
#endif

#if defined(USE_LINUX_QUOTACTL)
template <typename StatInfoFunc, typename QuotaInfoFunc>
nsresult nsLocalFile::GetDiskInfo(StatInfoFunc&& aStatInfoFunc,
                                  QuotaInfoFunc&& aQuotaInfoFunc,
                                  int64_t* aResult)
#else
template <typename StatInfoFunc>
nsresult nsLocalFile::GetDiskInfo(StatInfoFunc&& aStatInfoFunc,
                                  int64_t* aResult)
#endif
{
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  // These systems have the operations necessary to check disk space.

#ifdef STATFS

  // check to make sure that mPath is properly initialized
  CHECK_mPath();

  struct STATFS fs_buf;

  /*
   * Members of the STATFS struct that you should know about:
   * F_BSIZE = block size on disk.
   * f_bavail = number of free blocks available to a non-superuser.
   * f_bfree = number of total free blocks in file system.
   * f_blocks = number of total used or free blocks in file system.
   */

  if (STATFS(mPath.get(), &fs_buf) < 0) {
    // The call to STATFS failed.
#  ifdef DEBUG
    printf("ERROR: GetDiskInfo: STATFS call FAILED. \n");
#  endif
    return NS_ERROR_FAILURE;
  }

  CheckedInt64 statfsResult = std::forward<StatInfoFunc>(aStatInfoFunc)(fs_buf);
  if (!statfsResult.isValid()) {
    return NS_ERROR_CANNOT_CONVERT_DATA;
  }

  // Assign statfsResult to *aResult in case one of the quota calls fails.
  *aResult = statfsResult.value();

#  if defined(USE_LINUX_QUOTACTL)

  if (!FillStatCache()) {
    // Returns info from statfs
    return NS_OK;
  }

  nsAutoCString deviceName;
  if (!GetDeviceName(major(mCachedStat.st_dev), minor(mCachedStat.st_dev),
                     deviceName)) {
    // Returns info from statfs
    return NS_OK;
  }

  struct dqblk dq;
  if (!quotactl(QCMD(Q_GETQUOTA, USRQUOTA), deviceName.get(), getuid(),
                (caddr_t)&dq)
#    ifdef QIF_BLIMITS
      && dq.dqb_valid & QIF_BLIMITS
#    endif
      && dq.dqb_bhardlimit) {
    CheckedInt64 quotaResult = std::forward<QuotaInfoFunc>(aQuotaInfoFunc)(dq);
    if (!quotaResult.isValid()) {
      // Returns info from statfs
      return NS_OK;
    }

    if (quotaResult.value() < *aResult) {
      *aResult = quotaResult.value();
    }
  }
#  endif  // defined(USE_LINUX_QUOTACTL)

#  ifdef DEBUG_DISK_SPACE
  printf("DiskInfo: %lu bytes\n", *aResult);
#  endif

  return NS_OK;

#else  // STATFS
  /*
   * This platform doesn't have statfs or statvfs.  I'm sure that there's
   * a way to check for free disk space and disk capacity on platforms that
   * don't have statfs (I'm SURE they have df, for example).
   *
   * Until we figure out how to do that, lets be honest and say that this
   * command isn't implemented properly for these platforms yet.
   */
#  ifdef DEBUG
  printf("ERROR: GetDiskInfo: Not implemented for plaforms without statfs.\n");
#  endif
  return NS_ERROR_NOT_IMPLEMENTED;

#endif  // STATFS
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(int64_t* aDiskSpaceAvailable) {
  return GetDiskInfo(
      [](const struct STATFS& aStatInfo) {
        return aStatInfo.f_bavail * static_cast<uint64_t>(aStatInfo.F_BSIZE);
      },
#if defined(USE_LINUX_QUOTACTL)
      [](const struct dqblk& aQuotaInfo) -> uint64_t {
        // dqb_bhardlimit is count of BLOCK_SIZE blocks, dqb_curspace is bytes
        const uint64_t hardlimit = aQuotaInfo.dqb_bhardlimit * BLOCK_SIZE;
        if (hardlimit > aQuotaInfo.dqb_curspace) {
          return hardlimit - aQuotaInfo.dqb_curspace;
        }
        return 0;
      },
#endif
      aDiskSpaceAvailable);
}

NS_IMETHODIMP
nsLocalFile::GetDiskCapacity(int64_t* aDiskCapacity) {
  return GetDiskInfo(
      [](const struct STATFS& aStatInfo) {
        return aStatInfo.f_blocks * static_cast<uint64_t>(aStatInfo.F_BSIZE);
      },
#if defined(USE_LINUX_QUOTACTL)
      [](const struct dqblk& aQuotaInfo) {
        // dqb_bhardlimit is count of BLOCK_SIZE blocks
        return aQuotaInfo.dqb_bhardlimit * BLOCK_SIZE;
      },
#endif
      aDiskCapacity);
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile** aParent) {
  CHECK_mPath();
  if (NS_WARN_IF(!aParent)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aParent = nullptr;

  // if '/' we are at the top of the volume, return null
  if (mPath.EqualsLiteral("/")) {
    return NS_OK;
  }

  // <brendan, after jband> I promise to play nice
  char* buffer = mPath.BeginWriting();
  // find the last significant slash in buffer
  char* slashp = strrchr(buffer, '/');
  NS_ASSERTION(slashp, "non-canonical path?");
  if (!slashp) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  // for the case where we are at '/'
  if (slashp == buffer) {
    slashp++;
  }

  // temporarily terminate buffer at the last significant slash
  char c = *slashp;
  *slashp = '\0';

  nsCOMPtr<nsIFile> localFile;
  nsresult rv = NS_NewNativeLocalFile(nsDependentCString(buffer), true,
                                      getter_AddRefs(localFile));

  // make buffer whole again
  *slashp = c;

  if (NS_FAILED(rv)) {
    return rv;
  }

  localFile.forget(aParent);
  return NS_OK;
}

/*
 * The results of Exists, isWritable and isReadable are not cached.
 */

NS_IMETHODIMP
nsLocalFile::Exists(bool* aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = (access(mPath.get(), F_OK) == 0);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(bool* aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = (access(mPath.get(), W_OK) == 0);
  if (*aResult || errno == EACCES) {
    return NS_OK;
  }
  return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsReadable(bool* aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = (access(mPath.get(), R_OK) == 0);
  if (*aResult || errno == EACCES) {
    return NS_OK;
  }
  return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsExecutable(bool* aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  // Check extension (bug 663899). On certain platforms, the file
  // extension may cause the OS to treat it as executable regardless of
  // the execute bit, such as .jar on Mac OS X. We borrow the code from
  // nsLocalFileWin, slightly modified.

  // Don't be fooled by symlinks.
  bool symLink;
  nsresult rv = IsSymlink(&symLink);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsAutoString path;
  if (symLink) {
    GetTarget(path);
  } else {
    GetPath(path);
  }

  int32_t dotIdx = path.RFindChar(char16_t('.'));
  if (dotIdx != kNotFound) {
    // Convert extension to lower case.
    char16_t* p = path.BeginWriting();
    for (p += dotIdx + 1; *p; ++p) {
      *p += (*p >= L'A' && *p <= L'Z') ? 'a' - 'A' : 0;
    }

    // Search for any of the set of executable extensions.
    static const char* const executableExts[] = {
#ifdef MOZ_WIDGET_COCOA
        "afploc",  // Can point to other files.
#endif
        "air",  // Adobe AIR installer
#ifdef MOZ_WIDGET_COCOA
        "atloc",    // Can point to other files.
        "fileloc",  // File location files can be used to point to other
                    // files.
        "ftploc",   // Can point to other files.
        "inetloc",  // Shouldn't be able to do the same, but can, due to
                    // macOS vulnerabilities.
#endif
        "jar"  // java application bundle
    };
    nsDependentSubstring ext = Substring(path, dotIdx + 1);
    for (auto executableExt : executableExts) {
      if (ext.EqualsASCII(executableExt)) {
        // Found a match.  Set result and quit.
        *aResult = true;
        return NS_OK;
      }
    }
  }

  // On OS X, then query Launch Services.
#ifdef MOZ_WIDGET_COCOA
  // Certain Mac applications, such as Classic applications, which
  // run under Rosetta, might not have the +x mode bit but are still
  // considered to be executable by Launch Services (bug 646748).
  CFURLRef url;
  if (NS_FAILED(GetCFURL(&url))) {
    return NS_ERROR_FAILURE;
  }

  LSRequestedInfo theInfoRequest = kLSRequestAllInfo;
  LSItemInfoRecord theInfo;
  OSStatus result = ::LSCopyItemInfoForURL(url, theInfoRequest, &theInfo);
  ::CFRelease(url);
  if (result == noErr) {
    if ((theInfo.flags & kLSItemInfoIsApplication) != 0) {
      *aResult = true;
      return NS_OK;
    }
  }
#endif

  // Then check the execute bit.
  *aResult = (access(mPath.get(), X_OK) == 0);
#ifdef SOLARIS
  // On Solaris, access will always return 0 for root user, however
  // the file is only executable if S_IXUSR | S_IXGRP | S_IXOTH is set.
  // See bug 351950, https://bugzilla.mozilla.org/show_bug.cgi?id=351950
  if (*aResult) {
    struct STAT buf;

    *aResult = (STAT(mPath.get(), &buf) == 0);
    if (*aResult || errno == EACCES) {
      *aResult = *aResult && (buf.st_mode & (S_IXUSR | S_IXGRP | S_IXOTH));
      return NS_OK;
    }

    return NSRESULT_FOR_ERRNO();
  }
#endif
  if (*aResult || errno == EACCES) {
    return NS_OK;
  }
  return NSRESULT_FOR_ERRNO();
}

NS_IMETHODIMP
nsLocalFile::IsDirectory(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;
  ENSURE_STAT_CACHE();
  *aResult = S_ISDIR(mCachedStat.st_mode);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsFile(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;
  ENSURE_STAT_CACHE();
  *aResult = S_ISREG(mCachedStat.st_mode);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  nsACString::const_iterator begin, end;
  LocateNativeLeafName(begin, end);
  *aResult = (*begin == '.');
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  CHECK_mPath();

  struct STAT symStat;
  if (LSTAT(mPath.get(), &symStat) == -1) {
    return NSRESULT_FOR_ERRNO();
  }
  *aResult = S_ISLNK(symStat.st_mode);
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  ENSURE_STAT_CACHE();
  *aResult = S_ISCHR(mCachedStat.st_mode) || S_ISBLK(mCachedStat.st_mode) ||
#ifdef S_ISSOCK
             S_ISSOCK(mCachedStat.st_mode) ||
#endif
             S_ISFIFO(mCachedStat.st_mode);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile* aInFile, bool* aResult) {
  if (NS_WARN_IF(!aInFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;

  nsAutoCString inPath;
  nsresult rv = aInFile->GetNativePath(inPath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // We don't need to worry about "/foo/" vs. "/foo" here
  // because trailing slashes are stripped on init.
  *aResult = !strcmp(inPath.get(), mPath.get());
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile* aInFile, bool* aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aInFile)) {
    return NS_ERROR_INVALID_ARG;
  }
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString inPath;
  nsresult rv;

  if (NS_FAILED(rv = aInFile->GetNativePath(inPath))) {
    return rv;
  }

  *aResult = false;

  ssize_t len = mPath.Length();
  if (strncmp(mPath.get(), inPath.get(), len) == 0) {
    // Now make sure that the |aInFile|'s path has a separator at len,
    // which implies that it has more components after len.
    if (inPath[len] == '/') {
      *aResult = true;
    }
  }

  return NS_OK;
}

static nsresult ReadLinkSafe(const nsCString& aTarget, int32_t aExpectedSize,
                             nsACString& aOutBuffer) {
  // If we call readlink with a buffer size S it returns S, then we cannot tell
  // if the buffer was big enough to hold the entire path. We allocate an
  // additional byte so we can check if the buffer was large enough.
  const auto allocSize = CheckedInt<size_t>(aExpectedSize) + 1;
  if (!allocSize.isValid()) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  auto result = aOutBuffer.BulkWrite(allocSize.value(), 0, false);
  if (result.isErr()) {
    return result.unwrapErr();
  }

  auto handle = result.unwrap();

  while (true) {
    ssize_t bytesWritten =
        readlink(aTarget.get(), handle.Elements(), handle.Length());
    if (bytesWritten < 0) {
      return NSRESULT_FOR_ERRNO();
    }

    // written >= 0 so it is safe to cast to size_t.
    if ((size_t)bytesWritten < handle.Length()) {
      // Target might have changed since the lstat call, or lstat might lie, see
      // bug 1791029.
      handle.Finish(bytesWritten, false);
      return NS_OK;
    }

    // The buffer was not large enough, so double it and try again.
    auto restartResult = handle.RestartBulkWrite(handle.Length() * 2, 0, false);
    if (restartResult.isErr()) {
      return restartResult.unwrapErr();
    }
  }
}

NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString& aResult) {
  CHECK_mPath();
  aResult.Truncate();

  struct STAT symStat;
  if (LSTAT(mPath.get(), &symStat) == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  if (!S_ISLNK(symStat.st_mode)) {
    return NS_ERROR_FILE_INVALID_PATH;
  }

  nsAutoCString target;
  nsresult rv = ReadLinkSafe(mPath, symStat.st_size, target);
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIFile> self(this);
  int32_t maxLinks = 40;
  while (true) {
    if (maxLinks-- == 0) {
      rv = NS_ERROR_FILE_UNRESOLVABLE_SYMLINK;
      break;
    }

    if (target[0] != '/') {
      nsCOMPtr<nsIFile> parent;
      if (NS_FAILED(rv = self->GetParent(getter_AddRefs(parent)))) {
        break;
      }
      if (NS_FAILED(rv = parent->AppendRelativeNativePath(target))) {
        break;
      }
      if (NS_FAILED(rv = parent->GetNativePath(aResult))) {
        break;
      }
      self = parent;
    } else {
      aResult = target;
    }

    const nsPromiseFlatCString& flatRetval = PromiseFlatCString(aResult);

    // Any failure in testing the current target we'll just interpret
    // as having reached our destiny.
    if (LSTAT(flatRetval.get(), &symStat) == -1) {
      break;
    }

    // And of course we're done if it isn't a symlink.
    if (!S_ISLNK(symStat.st_mode)) {
      break;
    }

    nsAutoCString newTarget;
    rv = ReadLinkSafe(flatRetval, symStat.st_size, newTarget);
    if (NS_FAILED(rv)) {
      break;
    }

    target = newTarget;
  }

  if (NS_FAILED(rv)) {
    aResult.Truncate();
  }
  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntriesImpl(nsIDirectoryEnumerator** aEntries) {
  RefPtr<nsDirEnumeratorUnix> dir = new nsDirEnumeratorUnix();

  nsresult rv = dir->Init(this, false);
  if (NS_FAILED(rv)) {
    *aEntries = nullptr;
  } else {
    dir.forget(aEntries);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary** aResult) {
  CHECK_mPath();
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcnt::SetActivityIsLegal(false);
#endif

  *aResult = PR_LoadLibrary(mPath.get());

#ifdef NS_BUILD_REFCNT_LOGGING
  nsTraceRefcnt::SetActivityIsLegal(true);
#endif

  if (!*aResult) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString& aPersistentDescriptor) {
  return GetNativePath(aPersistentDescriptor);
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString& aPersistentDescriptor) {
#ifdef MOZ_WIDGET_COCOA
  if (aPersistentDescriptor.IsEmpty()) {
    return NS_ERROR_INVALID_ARG;
  }

  // Support pathnames as user-supplied descriptors if they begin with '/'
  // or '~'.  These characters do not collide with the base64 set used for
  // encoding alias records.
  char first = aPersistentDescriptor.First();
  if (first == '/' || first == '~') {
    return InitWithNativePath(aPersistentDescriptor);
  }

  uint32_t dataSize = aPersistentDescriptor.Length();
  char* decodedData = PL_Base64Decode(
      PromiseFlatCString(aPersistentDescriptor).get(), dataSize, nullptr);
  if (!decodedData) {
    NS_ERROR("SetPersistentDescriptor was given bad data");
    return NS_ERROR_FAILURE;
  }

  // Cast to an alias record and resolve.
  AliasRecord aliasHeader = *(AliasPtr)decodedData;
  int32_t aliasSize = ::GetAliasSizeFromPtr(&aliasHeader);
  if (aliasSize >
      ((int32_t)dataSize * 3) / 4) {  // be paranoid about having too few data
    PR_Free(decodedData);             // PL_Base64Decode() uses PR_Malloc().
    return NS_ERROR_FAILURE;
  }

  nsresult rv = NS_OK;

  // Move the now-decoded data into the Handle.
  // The size of the decoded data is 3/4 the size of the encoded data. See
  // plbase64.h
  Handle newHandle = nullptr;
  if (::PtrToHand(decodedData, &newHandle, aliasSize) != noErr) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  PR_Free(decodedData);  // PL_Base64Decode() uses PR_Malloc().
  if (NS_FAILED(rv)) {
    return rv;
  }

  Boolean changed;
  FSRef resolvedFSRef;
  OSErr err = ::FSResolveAlias(nullptr, (AliasHandle)newHandle, &resolvedFSRef,
                               &changed);

  rv = MacErrorMapper(err);
  DisposeHandle(newHandle);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return InitWithFSRef(&resolvedFSRef);
#else
  return InitWithNativePath(aPersistentDescriptor);
#endif
}

NS_IMETHODIMP
nsLocalFile::Reveal() {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

#ifdef MOZ_WIDGET_GTK
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return NS_ERROR_FAILURE;
  }
  return giovfs->RevealFile(this);
#elif defined(MOZ_WIDGET_COCOA)
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::RevealFileInFinder(url);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
nsLocalFile::Launch() {
  if (!FilePreferences::IsAllowedPath(mPath)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

#ifdef MOZ_WIDGET_GTK
  nsCOMPtr<nsIGIOService> giovfs = do_GetService(NS_GIOSERVICE_CONTRACTID);
  if (!giovfs) {
    return NS_ERROR_FAILURE;
  }

  return giovfs->LaunchFile(mPath);
#elif defined(MOZ_WIDGET_ANDROID)
  // Not supported on GeckoView
  return NS_ERROR_NOT_IMPLEMENTED;
#elif defined(MOZ_WIDGET_COCOA)
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::OpenURL(url);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
#else
  return NS_ERROR_FAILURE;
#endif
}

nsresult NS_NewNativeLocalFile(const nsACString& aPath, bool aFollowSymlinks,
                               nsIFile** aResult) {
  RefPtr<nsLocalFile> file = new nsLocalFile();

  if (!aPath.IsEmpty()) {
    nsresult rv = file->InitWithNativePath(aPath);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }
  file.forget(aResult);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// unicode support
//-----------------------------------------------------------------------------

#define SET_UCS(func, ucsArg)                          \
  {                                                    \
    nsAutoCString buf;                                 \
    nsresult rv = NS_CopyUnicodeToNative(ucsArg, buf); \
    if (NS_FAILED(rv)) return rv;                      \
    return (func)(buf);                                \
  }

#define GET_UCS(func, ucsArg)                   \
  {                                             \
    nsAutoCString buf;                          \
    nsresult rv = (func)(buf);                  \
    if (NS_FAILED(rv)) return rv;               \
    return NS_CopyNativeToUnicode(buf, ucsArg); \
  }

#define SET_UCS_2ARGS_2(func, opaqueArg, ucsArg)       \
  {                                                    \
    nsAutoCString buf;                                 \
    nsresult rv = NS_CopyUnicodeToNative(ucsArg, buf); \
    if (NS_FAILED(rv)) return rv;                      \
    return (func)(opaqueArg, buf);                     \
  }

// Unicode interface Wrapper
nsresult nsLocalFile::InitWithPath(const nsAString& aFilePath) {
  SET_UCS(InitWithNativePath, aFilePath);
}
nsresult nsLocalFile::Append(const nsAString& aNode) {
  SET_UCS(AppendNative, aNode);
}
nsresult nsLocalFile::AppendRelativePath(const nsAString& aNode) {
  SET_UCS(AppendRelativeNativePath, aNode);
}
nsresult nsLocalFile::GetLeafName(nsAString& aLeafName) {
  GET_UCS(GetNativeLeafName, aLeafName);
}
nsresult nsLocalFile::SetLeafName(const nsAString& aLeafName) {
  SET_UCS(SetNativeLeafName, aLeafName);
}
nsresult nsLocalFile::GetPath(nsAString& aResult) {
  return NS_CopyNativeToUnicode(mPath, aResult);
}
nsresult nsLocalFile::CopyTo(nsIFile* aNewParentDir,
                             const nsAString& aNewName) {
  SET_UCS_2ARGS_2(CopyToNative, aNewParentDir, aNewName);
}
nsresult nsLocalFile::CopyToFollowingLinks(nsIFile* aNewParentDir,
                                           const nsAString& aNewName) {
  SET_UCS_2ARGS_2(CopyToFollowingLinksNative, aNewParentDir, aNewName);
}
nsresult nsLocalFile::MoveTo(nsIFile* aNewParentDir,
                             const nsAString& aNewName) {
  SET_UCS_2ARGS_2(MoveToNative, aNewParentDir, aNewName);
}
NS_IMETHODIMP
nsLocalFile::MoveToFollowingLinks(nsIFile* aNewParentDir,
                                  const nsAString& aNewName) {
  SET_UCS_2ARGS_2(MoveToFollowingLinksNative, aNewParentDir, aNewName);
}

NS_IMETHODIMP
nsLocalFile::RenameTo(nsIFile* aNewParentDir, const nsAString& aNewName) {
  SET_UCS_2ARGS_2(RenameToNative, aNewParentDir, aNewName);
}

NS_IMETHODIMP
nsLocalFile::RenameToNative(nsIFile* aNewParentDir,
                            const nsACString& aNewName) {
  nsresult rv;

  // check to make sure that this has been initialized properly
  CHECK_mPath();

  // check to make sure that we have a new parent
  nsAutoCString newPathName;
  rv = GetNativeTargetPathName(aNewParentDir, aNewName, newPathName);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!FilePreferences::IsAllowedPath(newPathName)) {
    return NS_ERROR_FILE_ACCESS_DENIED;
  }

  // try for atomic rename
  if (rename(mPath.get(), newPathName.get()) < 0) {
    if (errno == EXDEV) {
      rv = NS_ERROR_FILE_ACCESS_DENIED;
    } else {
      rv = NSRESULT_FOR_ERRNO();
    }
  }

  return rv;
}

nsresult nsLocalFile::GetTarget(nsAString& aResult) {
  GET_UCS(GetNativeTarget, aResult);
}

nsresult NS_NewLocalFile(const nsAString& aPath, bool aFollowLinks,
                         nsIFile** aResult) {
  nsAutoCString buf;
  nsresult rv = NS_CopyUnicodeToNative(aPath, buf);
  if (NS_FAILED(rv)) {
    return rv;
  }
  return NS_NewNativeLocalFile(buf, aFollowLinks, aResult);
}

// nsILocalFileMac

#ifdef MOZ_WIDGET_COCOA

NS_IMETHODIMP
nsLocalFile::HasXAttr(const nsACString& aAttrName, bool* aHasAttr) {
  NS_ENSURE_ARG_POINTER(aHasAttr);

  nsAutoCString attrName{aAttrName};

  ssize_t size = getxattr(mPath.get(), attrName.get(), nullptr, 0, 0, 0);
  if (size == -1) {
    if (errno == ENOATTR) {
      *aHasAttr = false;
    } else {
      return NSRESULT_FOR_ERRNO();
    }
  } else {
    *aHasAttr = true;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetXAttr(const nsACString& aAttrName,
                      nsTArray<uint8_t>& aAttrValue) {
  aAttrValue.Clear();

  nsAutoCString attrName{aAttrName};

  ssize_t size = getxattr(mPath.get(), attrName.get(), nullptr, 0, 0, 0);

  if (size == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  for (;;) {
    aAttrValue.SetCapacity(size);

    // The attribute can change between our first call and this call, so we need
    // to re-check the size and possibly call with a larger buffer.
    ssize_t newSize = getxattr(mPath.get(), attrName.get(),
                               aAttrValue.Elements(), size, 0, 0);
    if (newSize == -1) {
      return NSRESULT_FOR_ERRNO();
    }

    if (newSize <= size) {
      aAttrValue.SetLength(newSize);
      break;
    } else {
      size = newSize;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetXAttr(const nsACString& aAttrName,
                      const nsTArray<uint8_t>& aAttrValue) {
  nsAutoCString attrName{aAttrName};

  if (setxattr(mPath.get(), attrName.get(), aAttrValue.Elements(),
               aAttrValue.Length(), 0, 0) == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::DelXAttr(const nsACString& aAttrName) {
  nsAutoCString attrName{aAttrName};

  // Ignore removing an attribute that does not exist.
  if (removexattr(mPath.get(), attrName.get(), 0) == -1) {
    return NSRESULT_FOR_ERRNO();
  }

  return NS_OK;
}

static nsresult MacErrorMapper(OSErr inErr) {
  nsresult outErr;

  switch (inErr) {
    case noErr:
      outErr = NS_OK;
      break;

    case fnfErr:
    case afpObjectNotFound:
    case afpDirNotFound:
      outErr = NS_ERROR_FILE_NOT_FOUND;
      break;

    case dupFNErr:
    case afpObjectExists:
      outErr = NS_ERROR_FILE_ALREADY_EXISTS;
      break;

    case dskFulErr:
    case afpDiskFull:
      outErr = NS_ERROR_FILE_NO_DEVICE_SPACE;
      break;

    case fLckdErr:
    case afpVolLocked:
      outErr = NS_ERROR_FILE_IS_LOCKED;
      break;

    case afpAccessDenied:
      outErr = NS_ERROR_FILE_ACCESS_DENIED;
      break;

    case afpDirNotEmpty:
      outErr = NS_ERROR_FILE_DIR_NOT_EMPTY;
      break;

    // Can't find good map for some
    case bdNamErr:
      outErr = NS_ERROR_FAILURE;
      break;

    default:
      outErr = NS_ERROR_FAILURE;
      break;
  }

  return outErr;
}

static nsresult CFStringReftoUTF8(CFStringRef aInStrRef, nsACString& aOutStr) {
  // first see if the conversion would succeed and find the length of the
  // result
  CFIndex usedBufLen, inStrLen = ::CFStringGetLength(aInStrRef);
  CFIndex charsConverted = ::CFStringGetBytes(
      aInStrRef, CFRangeMake(0, inStrLen), kCFStringEncodingUTF8, 0, false,
      nullptr, 0, &usedBufLen);
  if (charsConverted == inStrLen) {
    // all characters converted, do the actual conversion
    aOutStr.SetLength(usedBufLen);
    if (aOutStr.Length() != (unsigned int)usedBufLen) {
      return NS_ERROR_OUT_OF_MEMORY;
    }
    UInt8* buffer = (UInt8*)aOutStr.BeginWriting();
    ::CFStringGetBytes(aInStrRef, CFRangeMake(0, inStrLen),
                       kCFStringEncodingUTF8, 0, false, buffer, usedBufLen,
                       &usedBufLen);
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::InitWithCFURL(CFURLRef aCFURL) {
  UInt8 path[PATH_MAX];
  if (::CFURLGetFileSystemRepresentation(aCFURL, true, path, PATH_MAX)) {
    nsDependentCString nativePath((char*)path);
    return InitWithNativePath(nativePath);
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::InitWithFSRef(const FSRef* aFSRef) {
  if (NS_WARN_IF(!aFSRef)) {
    return NS_ERROR_INVALID_ARG;
  }

  CFURLRef newURLRef = ::CFURLCreateFromFSRef(kCFAllocatorDefault, aFSRef);
  if (newURLRef) {
    nsresult rv = InitWithCFURL(newURLRef);
    ::CFRelease(newURLRef);
    return rv;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetCFURL(CFURLRef* aResult) {
  CHECK_mPath();

  bool isDir;
  IsDirectory(&isDir);
  *aResult = ::CFURLCreateFromFileSystemRepresentation(
      kCFAllocatorDefault, (UInt8*)mPath.get(), mPath.Length(), isDir);

  return (*aResult ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsLocalFile::GetFSRef(FSRef* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsresult rv = NS_ERROR_FAILURE;

  CFURLRef url = nullptr;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    if (::CFURLGetFSRef(url, aResult)) {
      rv = NS_OK;
    }
    ::CFRelease(url);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetFSSpec(FSSpec* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }

  FSRef fsRef;
  nsresult rv = GetFSRef(&fsRef);
  if (NS_SUCCEEDED(rv)) {
    OSErr err = ::FSGetCatalogInfo(&fsRef, kFSCatInfoNone, nullptr, nullptr,
                                   aResult, nullptr);
    return MacErrorMapper(err);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetFileSizeWithResFork(int64_t* aFileSizeWithResFork) {
  if (NS_WARN_IF(!aFileSizeWithResFork)) {
    return NS_ERROR_INVALID_ARG;
  }

  FSRef fsRef;
  nsresult rv = GetFSRef(&fsRef);
  if (NS_FAILED(rv)) {
    return rv;
  }

  FSCatalogInfo catalogInfo;
  OSErr err =
      ::FSGetCatalogInfo(&fsRef, kFSCatInfoDataSizes + kFSCatInfoRsrcSizes,
                         &catalogInfo, nullptr, nullptr, nullptr);
  if (err != noErr) {
    return MacErrorMapper(err);
  }

  *aFileSizeWithResFork =
      catalogInfo.dataLogicalSize + catalogInfo.rsrcLogicalSize;
  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFileType(OSType* aFileType) {
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::GetFileTypeCode(url, aFileType);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetFileType(OSType aFileType) {
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::SetFileTypeCode(url, aFileType);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetFileCreator(OSType* aFileCreator) {
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::GetFileCreatorCode(url, aFileCreator);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::SetFileCreator(OSType aFileCreator) {
  CFURLRef url;
  if (NS_SUCCEEDED(GetCFURL(&url))) {
    nsresult rv = CocoaFileUtils::SetFileCreatorCode(url, aFileCreator);
    ::CFRelease(url);
    return rv;
  }
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::LaunchWithDoc(nsIFile* aDocToLoad, bool aLaunchInBackground) {
  bool isExecutable;
  nsresult rv = IsExecutable(&isExecutable);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isExecutable) {
    return NS_ERROR_FILE_EXECUTION_FAILED;
  }

  FSRef appFSRef, docFSRef;
  rv = GetFSRef(&appFSRef);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (aDocToLoad) {
    nsCOMPtr<nsILocalFileMac> macDoc = do_QueryInterface(aDocToLoad);
    rv = macDoc->GetFSRef(&docFSRef);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  LSLaunchFlags theLaunchFlags = kLSLaunchDefaults;
  LSLaunchFSRefSpec thelaunchSpec;

  if (aLaunchInBackground) {
    theLaunchFlags |= kLSLaunchDontSwitch;
  }
  memset(&thelaunchSpec, 0, sizeof(LSLaunchFSRefSpec));

  thelaunchSpec.appRef = &appFSRef;
  if (aDocToLoad) {
    thelaunchSpec.numDocs = 1;
    thelaunchSpec.itemRefs = &docFSRef;
  }
  thelaunchSpec.launchFlags = theLaunchFlags;

  OSErr err = ::LSOpenFromRefSpec(&thelaunchSpec, nullptr);
  if (err != noErr) {
    return MacErrorMapper(err);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenDocWithApp(nsIFile* aAppToOpenWith, bool aLaunchInBackground) {
  FSRef docFSRef;
  nsresult rv = GetFSRef(&docFSRef);
  if (NS_FAILED(rv)) {
    return rv;
  }

  if (!aAppToOpenWith) {
    OSErr err = ::LSOpenFSRef(&docFSRef, nullptr);
    return MacErrorMapper(err);
  }

  nsCOMPtr<nsILocalFileMac> appFileMac = do_QueryInterface(aAppToOpenWith, &rv);
  if (!appFileMac) {
    return rv;
  }

  bool isExecutable;
  rv = appFileMac->IsExecutable(&isExecutable);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if (!isExecutable) {
    return NS_ERROR_FILE_EXECUTION_FAILED;
  }

  FSRef appFSRef;
  rv = appFileMac->GetFSRef(&appFSRef);
  if (NS_FAILED(rv)) {
    return rv;
  }

  LSLaunchFlags theLaunchFlags = kLSLaunchDefaults;
  LSLaunchFSRefSpec thelaunchSpec;

  if (aLaunchInBackground) {
    theLaunchFlags |= kLSLaunchDontSwitch;
  }
  memset(&thelaunchSpec, 0, sizeof(LSLaunchFSRefSpec));

  thelaunchSpec.appRef = &appFSRef;
  thelaunchSpec.numDocs = 1;
  thelaunchSpec.itemRefs = &docFSRef;
  thelaunchSpec.launchFlags = theLaunchFlags;

  OSErr err = ::LSOpenFromRefSpec(&thelaunchSpec, nullptr);
  if (err != noErr) {
    return MacErrorMapper(err);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsPackage(bool* aResult) {
  if (NS_WARN_IF(!aResult)) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = false;

  CFURLRef url;
  nsresult rv = GetCFURL(&url);
  if (NS_FAILED(rv)) {
    return rv;
  }

  LSItemInfoRecord info;
  OSStatus status =
      ::LSCopyItemInfoForURL(url, kLSRequestBasicFlagsOnly, &info);

  ::CFRelease(url);

  if (status != noErr) {
    return NS_ERROR_FAILURE;
  }

  *aResult = !!(info.flags & kLSItemInfoIsPackage);

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetBundleDisplayName(nsAString& aOutBundleName) {
  bool isPackage = false;
  nsresult rv = IsPackage(&isPackage);
  if (NS_FAILED(rv) || !isPackage) {
    return NS_ERROR_FAILURE;
  }

  nsAutoString name;
  rv = GetLeafName(name);
  if (NS_FAILED(rv)) {
    return rv;
  }

  int32_t length = name.Length();
  if (Substring(name, length - 4, length).EqualsLiteral(".app")) {
    // 4 characters in ".app"
    aOutBundleName = Substring(name, 0, length - 4);
  } else {
    aOutBundleName = name;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetBundleIdentifier(nsACString& aOutBundleIdentifier) {
  nsresult rv = NS_ERROR_FAILURE;

  CFURLRef urlRef;
  if (NS_SUCCEEDED(GetCFURL(&urlRef))) {
    CFBundleRef bundle = ::CFBundleCreate(nullptr, urlRef);
    if (bundle) {
      CFStringRef bundleIdentifier = ::CFBundleGetIdentifier(bundle);
      if (bundleIdentifier) {
        rv = CFStringReftoUTF8(bundleIdentifier, aOutBundleIdentifier);
      }
      ::CFRelease(bundle);
    }
    ::CFRelease(urlRef);
  }

  return rv;
}

NS_IMETHODIMP
nsLocalFile::GetBundleContentsLastModifiedTime(int64_t* aLastModTime) {
  CHECK_mPath();
  if (NS_WARN_IF(!aLastModTime)) {
    return NS_ERROR_INVALID_ARG;
  }

  bool isPackage = false;
  nsresult rv = IsPackage(&isPackage);
  if (NS_FAILED(rv) || !isPackage) {
    return GetLastModifiedTime(aLastModTime);
  }

  nsAutoCString infoPlistPath(mPath);
  infoPlistPath.AppendLiteral("/Contents/Info.plist");
  PRFileInfo64 info;
  if (PR_GetFileInfo64(infoPlistPath.get(), &info) != PR_SUCCESS) {
    return GetLastModifiedTime(aLastModTime);
  }
  int64_t modTime = int64_t(info.modifyTime);
  if (modTime == 0) {
    *aLastModTime = 0;
  } else {
    *aLastModTime = modTime / int64_t(PR_USEC_PER_MSEC);
  }

  return NS_OK;
}

NS_IMETHODIMP nsLocalFile::InitWithFile(nsIFile* aFile) {
  if (NS_WARN_IF(!aFile)) {
    return NS_ERROR_INVALID_ARG;
  }

  nsAutoCString nativePath;
  nsresult rv = aFile->GetNativePath(nativePath);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return InitWithNativePath(nativePath);
}

nsresult NS_NewLocalFileWithFSRef(const FSRef* aFSRef, bool aFollowLinks,
                                  nsILocalFileMac** aResult) {
  RefPtr<nsLocalFile> file = new nsLocalFile();

  nsresult rv = file->InitWithFSRef(aFSRef);
  if (NS_FAILED(rv)) {
    return rv;
  }
  file.forget(aResult);
  return NS_OK;
}

nsresult NS_NewLocalFileWithCFURL(const CFURLRef aURL, bool aFollowLinks,
                                  nsILocalFileMac** aResult) {
  RefPtr<nsLocalFile> file = new nsLocalFile();

  nsresult rv = file->InitWithCFURL(aURL);
  if (NS_FAILED(rv)) {
    return rv;
  }
  file.forget(aResult);
  return NS_OK;
}

#endif
