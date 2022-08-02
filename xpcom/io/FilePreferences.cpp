/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePreferences.h"

#include "mozilla/Atomics.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/Preferences.h"
#include "mozilla/StaticMutex.h"
#include "mozilla/StaticPtr.h"
#include "mozilla/TextUtils.h"
#include "mozilla/Tokenizer.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsString.h"

namespace mozilla {
namespace FilePreferences {

static StaticMutex sMutex;

static bool sBlockUNCPaths = false;
typedef nsTArray<nsString> WinPaths;

static WinPaths& PathAllowlist() MOZ_REQUIRES(sMutex) {
  sMutex.AssertCurrentThreadOwns();

  static WinPaths sPaths MOZ_GUARDED_BY(sMutex);
  return sPaths;
}

#ifdef XP_WIN
const auto kDevicePathSpecifier = u"\\\\?\\"_ns;

typedef char16_t char_path_t;
#else
typedef char char_path_t;
#endif

// Initially false to make concurrent consumers acquire the lock and sync.
// The plain bool is synchronized with sMutex, the atomic one is for a quick
// check w/o the need to acquire the lock on the hot path.
static bool sForbiddenPathsEmpty = false;
static Atomic<bool, Relaxed> sForbiddenPathsEmptyQuickCheck{false};

typedef nsTArray<nsTString<char_path_t>> Paths;
static StaticAutoPtr<Paths> sForbiddenPaths;

static Paths& ForbiddenPaths() {
  sMutex.AssertCurrentThreadOwns();
  if (!sForbiddenPaths) {
    sForbiddenPaths = new nsTArray<nsTString<char_path_t>>();
    ClearOnShutdown(&sForbiddenPaths);
  }
  return *sForbiddenPaths;
}

static void AllowUNCDirectory(char const* directory) {
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(directory, getter_AddRefs(file));
  if (!file) {
    return;
  }

  nsString path;
  if (NS_FAILED(file->GetTarget(path))) {
    return;
  }

  // The allowlist makes sense only for UNC paths, because this code is used
  // to block only UNC paths, hence, no need to add non-UNC directories here
  // as those would never pass the check.
  if (!StringBeginsWith(path, u"\\\\"_ns)) {
    return;
  }

  StaticMutexAutoLock lock(sMutex);

  if (!PathAllowlist().Contains(path)) {
    PathAllowlist().AppendElement(path);
  }
}

void InitPrefs() {
  sBlockUNCPaths =
      Preferences::GetBool("network.file.disable_unc_paths", false);

  nsTAutoString<char_path_t> forbidden;
#ifdef XP_WIN
  Preferences::GetString("network.file.path_blacklist", forbidden);
#else
  Preferences::GetCString("network.file.path_blacklist", forbidden);
#endif

  StaticMutexAutoLock lock(sMutex);

  if (forbidden.IsEmpty()) {
    sForbiddenPathsEmptyQuickCheck = (sForbiddenPathsEmpty = true);
    return;
  }

  ForbiddenPaths().Clear();
  TTokenizer<char_path_t> p(forbidden);
  while (!p.CheckEOF()) {
    nsTString<char_path_t> path;
    Unused << p.ReadUntil(TTokenizer<char_path_t>::Token::Char(','), path);
    path.Trim(" ");
    if (!path.IsEmpty()) {
      ForbiddenPaths().AppendElement(path);
    }
    Unused << p.CheckChar(',');
  }

  sForbiddenPathsEmptyQuickCheck =
      (sForbiddenPathsEmpty = ForbiddenPaths().Length() == 0);
}

void InitDirectoriesAllowlist() {
  // NS_GRE_DIR is the installation path where the binary resides.
  AllowUNCDirectory(NS_GRE_DIR);
  // NS_APP_USER_PROFILE_50_DIR and NS_APP_USER_PROFILE_LOCAL_50_DIR are the two
  // parts of the profile we store permanent and local-specific data.
  AllowUNCDirectory(NS_APP_USER_PROFILE_50_DIR);
  AllowUNCDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR);
}

namespace {  // anon

template <typename TChar>
class TNormalizer : public TTokenizer<TChar> {
  typedef TTokenizer<TChar> base;

 public:
  typedef typename base::Token Token;

  TNormalizer(const nsTSubstring<TChar>& aFilePath, const Token& aSeparator)
      : TTokenizer<TChar>(aFilePath), mSeparator(aSeparator) {}

  bool Get(nsTSubstring<TChar>& aNormalizedFilePath) {
    aNormalizedFilePath.Truncate();

    // Windows UNC paths begin with double separator (\\)
    // Linux paths begin with just one separator (/)
    // If we want to use the normalizer for regular windows paths this code
    // will need to be updated.
#ifdef XP_WIN
    if (base::Check(mSeparator)) {
      aNormalizedFilePath.Append(mSeparator.AsChar());
    }
#endif

    if (base::Check(mSeparator)) {
      aNormalizedFilePath.Append(mSeparator.AsChar());
    }

    while (base::HasInput()) {
      if (!ConsumeName()) {
        return false;
      }
    }

    for (auto const& name : mStack) {
      aNormalizedFilePath.Append(name);
    }

    return true;
  }

 private:
  bool ConsumeName() {
    if (base::CheckEOF()) {
      return true;
    }

    if (CheckCurrentDir()) {
      return true;
    }

    if (CheckParentDir()) {
      if (!mStack.Length()) {
        // This means there are more \.. than valid names
        return false;
      }

      mStack.RemoveLastElement();
      return true;
    }

    nsTDependentSubstring<TChar> name;
    if (base::ReadUntil(mSeparator, name, base::INCLUDE_LAST) &&
        name.Length() == 1) {
      // this means an empty name (a lone slash), which is illegal
      return false;
    }
    mStack.AppendElement(name);

    return true;
  }

  bool CheckParentDir() {
    typename nsTString<TChar>::const_char_iterator cursor = base::mCursor;
    if (base::CheckChar('.') && base::CheckChar('.') && CheckSeparator()) {
      return true;
    }

    base::mCursor = cursor;
    return false;
  }

  bool CheckCurrentDir() {
    typename nsTString<TChar>::const_char_iterator cursor = base::mCursor;
    if (base::CheckChar('.') && CheckSeparator()) {
      return true;
    }

    base::mCursor = cursor;
    return false;
  }

  bool CheckSeparator() { return base::Check(mSeparator) || base::CheckEOF(); }

  Token const mSeparator;
  nsTArray<nsTDependentSubstring<TChar>> mStack;
};

#ifdef XP_WIN
bool IsDOSDevicePathWithDrive(const nsAString& aFilePath) {
  if (!StringBeginsWith(aFilePath, kDevicePathSpecifier)) {
    return false;
  }

  const auto pathNoPrefix =
      nsDependentSubstring(aFilePath, kDevicePathSpecifier.Length());

  // After the device path specifier, the rest of file path can be:
  // - starts with the volume or drive. e.g. \\?\C:\...
  // - UNCs. e.g. \\?\UNC\Server\Share\Test\Foo.txt
  // - device UNCs. e.g. \\?\server1\e:\utilities\\filecomparer\...
  // The first case should not be blocked by IsBlockedUNCPath.
  if (!StartsWithDiskDesignatorAndBackslash(pathNoPrefix)) {
    return false;
  }

  return true;
}
#endif

}  // namespace

bool IsBlockedUNCPath(const nsAString& aFilePath) {
  typedef TNormalizer<char16_t> Normalizer;
  if (!sBlockUNCPaths) {
    return false;
  }

  if (!StringBeginsWith(aFilePath, u"\\\\"_ns)) {
    return false;
  }

#ifdef XP_WIN
  // ToDo: We don't need to check this once we can check if there is a valid
  // server or host name that is prefaced by "\\".
  // https://docs.microsoft.com/en-us/dotnet/standard/io/file-path-formats
  if (IsDOSDevicePathWithDrive(aFilePath)) {
    return false;
  }
#endif

  nsAutoString normalized;
  if (!Normalizer(aFilePath, Normalizer::Token::Char('\\')).Get(normalized)) {
    // Broken paths are considered invalid and thus inaccessible
    return true;
  }

  StaticMutexAutoLock lock(sMutex);

  for (const auto& allowedPrefix : PathAllowlist()) {
    if (StringBeginsWith(normalized, allowedPrefix)) {
      if (normalized.Length() == allowedPrefix.Length()) {
        return false;
      }
      if (normalized[allowedPrefix.Length()] == L'\\') {
        return false;
      }

      // When we are here, the path has a form "\\path\prefixevil"
      // while we have an allowed prefix of "\\path\prefix".
      // Note that we don't want to add a slash to the end of a prefix
      // so that opening the directory (no slash at the end) still works.
      break;
    }
  }

  return true;
}

#ifdef XP_WIN
const char kPathSeparator = '\\';
#else
const char kPathSeparator = '/';
#endif

bool IsAllowedPath(const nsTSubstring<char_path_t>& aFilePath) {
  typedef TNormalizer<char_path_t> Normalizer;

  // An atomic quick check out of the lock, because this is mostly `true`.
  if (sForbiddenPathsEmptyQuickCheck) {
    return true;
  }

  StaticMutexAutoLock lock(sMutex);

  if (sForbiddenPathsEmpty) {
    return true;
  }

  // If sForbidden has been cleared at shutdown, we must avoid calling
  // ForbiddenPaths() again, as that will recreate the array and we will leak.
  if (!sForbiddenPaths) {
    return true;
  }

  nsTAutoString<char_path_t> normalized;
  if (!Normalizer(aFilePath, Normalizer::Token::Char(kPathSeparator))
           .Get(normalized)) {
    // Broken paths are considered invalid and thus inaccessible
    return false;
  }

  for (const auto& prefix : ForbiddenPaths()) {
    if (StringBeginsWith(normalized, prefix)) {
      if (normalized.Length() > prefix.Length() &&
          normalized[prefix.Length()] != kPathSeparator) {
        continue;
      }
      return false;
    }
  }

  return true;
}

#ifdef XP_WIN
bool StartsWithDiskDesignatorAndBackslash(const nsAString& aAbsolutePath) {
  // aAbsolutePath can only be (in regular expression):
  // UNC path: ^\\\\.*
  // A single backslash: ^\\.*
  // A disk designator with a backslash: ^[A-Za-z]:\\.*
  return aAbsolutePath.Length() >= 3 && IsAsciiAlpha(aAbsolutePath.CharAt(0)) &&
         aAbsolutePath.CharAt(1) == L':' &&
         aAbsolutePath.CharAt(2) == kPathSeparator;
}
#endif

void testing::SetBlockUNCPaths(bool aBlock) { sBlockUNCPaths = aBlock; }

void testing::AddDirectoryToAllowlist(nsAString const& aPath) {
  StaticMutexAutoLock lock(sMutex);
  PathAllowlist().AppendElement(aPath);
}

bool testing::NormalizePath(nsAString const& aPath, nsAString& aNormalized) {
  typedef TNormalizer<char16_t> Normalizer;
  Normalizer normalizer(aPath, Normalizer::Token::Char('\\'));
  return normalizer.Get(aNormalized);
}

}  // namespace FilePreferences
}  // namespace mozilla
