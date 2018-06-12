/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
* License, v. 2.0. If a copy of the MPL was not distributed with this
* file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "FilePreferences.h"

#include "mozilla/Preferences.h"
#include "mozilla/Tokenizer.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"

namespace mozilla {
namespace FilePreferences {

static bool sBlockUNCPaths = false;
typedef nsTArray<nsString> Paths;

static Paths& PathArray()
{
  static Paths sPaths;
  return sPaths;
}

static void AllowDirectory(char const* directory)
{
  nsCOMPtr<nsIFile> file;
  NS_GetSpecialDirectory(directory, getter_AddRefs(file));
  if (!file) {
    return;
  }

  nsString path;
  if (NS_FAILED(file->GetTarget(path))) {
    return;
  }

  // The whitelist makes sense only for UNC paths, because this code is used
  // to block only UNC paths, hence, no need to add non-UNC directories here
  // as those would never pass the check.
  if (!StringBeginsWith(path, NS_LITERAL_STRING("\\\\"))) {
    return;
  }

  if (!PathArray().Contains(path)) {
    PathArray().AppendElement(path);
  }
}

void InitPrefs()
{
  sBlockUNCPaths = Preferences::GetBool("network.file.disable_unc_paths", false);
}

void InitDirectoriesWhitelist()
{
  // NS_GRE_DIR is the installation path where the binary resides.
  AllowDirectory(NS_GRE_DIR);
  // NS_APP_USER_PROFILE_50_DIR and NS_APP_USER_PROFILE_LOCAL_50_DIR are the two
  // parts of the profile we store permanent and local-specific data.
  AllowDirectory(NS_APP_USER_PROFILE_50_DIR);
  AllowDirectory(NS_APP_USER_PROFILE_LOCAL_50_DIR);
}

namespace { // anon

class Normalizer : public Tokenizer16
{
public:
  Normalizer(const nsAString& aFilePath, const Token& aSeparator);
  bool Get(nsAString& aNormalizedFilePath);

private:
  bool ConsumeName();
  bool CheckParentDir();
  bool CheckCurrentDir();
  bool CheckSeparator();

  Token const mSeparator;
  nsTArray<nsDependentSubstring> mStack;
};

Normalizer::Normalizer(const nsAString& aFilePath, const Token& aSeparator)
  : Tokenizer16(aFilePath)
  , mSeparator(aSeparator)
{
}

bool Normalizer::Get(nsAString& aNormalizedFilePath)
{
  aNormalizedFilePath.Truncate();

  if (Check(mSeparator)) {
    aNormalizedFilePath.Append(mSeparator.AsChar());
  }
  if (Check(mSeparator)) {
    aNormalizedFilePath.Append(mSeparator.AsChar());
  }

  while (HasInput()) {
    if (!ConsumeName()) {
      return false;
    }
  }

  for (auto const& name : mStack) {
    aNormalizedFilePath.Append(name);
  }

  return true;
}

bool Normalizer::ConsumeName()
{
  if (CheckEOF()) {
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

  nsDependentSubstring name;
  if (ReadUntil(mSeparator, name, INCLUDE_LAST) && name.Length() == 1) {
    // this means and empty name (a lone slash), which is illegal
    return false;
  }
  mStack.AppendElement(name);

  return true;
}

bool Normalizer::CheckCurrentDir()
{
  nsString::const_char_iterator cursor = mCursor;
  if (CheckChar('.') && CheckSeparator()) {
    return true;
  }

  mCursor = cursor;
  return false;
}

bool Normalizer::CheckParentDir()
{
  nsString::const_char_iterator cursor = mCursor;
  if (CheckChar('.') && CheckChar('.') && CheckSeparator()) {
    return true;
  }

  mCursor = cursor;
  return false;
}

bool Normalizer::CheckSeparator()
{
  return Check(mSeparator) || CheckEOF();
}

} // anon

bool IsBlockedUNCPath(const nsAString& aFilePath)
{
  if (!sBlockUNCPaths) {
    return false;
  }

  if (!StringBeginsWith(aFilePath, NS_LITERAL_STRING("\\\\"))) {
    return false;
  }

  nsAutoString normalized;
  if (!Normalizer(aFilePath, Normalizer::Token::Char('\\')).Get(normalized)) {
    // Broken paths are considered invalid and thus inaccessible
    return true;
  }

  for (const auto& allowedPrefix : PathArray()) {
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

void testing::SetBlockUNCPaths(bool aBlock)
{
  sBlockUNCPaths = aBlock;
}

void testing::AddDirectoryToWhitelist(nsAString const & aPath)
{
  PathArray().AppendElement(aPath);
}

bool testing::NormalizePath(nsAString const & aPath, nsAString & aNormalized)
{
  Normalizer normalizer(aPath, Normalizer::Token::Char('\\'));
  return normalizer.Get(aNormalized);
}

} // ::FilePreferences
} // ::mozilla
