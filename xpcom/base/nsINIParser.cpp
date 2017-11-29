/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Moz headers (alphabetical)
#include "nsCRTGlue.h"
#include "nsError.h"
#include "nsIFile.h"
#include "nsINIParser.h"
#include "mozilla/FileUtils.h" // AutoFILE
#include "mozilla/ResultExtensions.h"
#include "mozilla/URLPreloader.h"

// System headers (alphabetical)
#include <stdio.h>
#include <stdlib.h>
#ifdef XP_WIN
#include <windows.h>
#endif

using namespace mozilla;

#ifdef XP_WIN
inline FILE*
TS_tfopen(const char* aPath, const wchar_t* aMode)
{
  wchar_t wPath[MAX_PATH];
  MultiByteToWideChar(CP_UTF8, 0, aPath, -1, wPath, MAX_PATH);
  return _wfopen(wPath, aMode);
}
#else
inline FILE*
TS_tfopen(const char* aPath, const char* aMode)
{
  return fopen(aPath, aMode);
}
#endif

// Stack based FILE wrapper to ensure that fclose is called, copied from
// toolkit/mozapps/update/updater/readstrings.cpp

class AutoFILE
{
public:
  explicit AutoFILE(FILE* aFp = nullptr) : fp_(aFp) {}
  ~AutoFILE()
  {
    if (fp_) {
      fclose(fp_);
    }
  }
  operator FILE*() { return fp_; }
  FILE** operator&() { return &fp_; }
  void operator=(FILE* aFp) { fp_ = aFp; }
private:
  FILE* fp_;
};

nsresult
nsINIParser::Init(nsIFile* aFile)
{
  nsCString result;
  MOZ_TRY_VAR(result, URLPreloader::ReadFile(aFile));

  return InitFromString(result);
}

static const char kNL[] = "\r\n";
static const char kEquals[] = "=";
static const char kWhitespace[] = " \t";
static const char kRBracket[] = "]";

nsresult
nsINIParser::InitFromString(const nsCString& aStr)
{
  char* buffer;

  if (StringHead(aStr, 3) == "\xEF\xBB\xBF") {
    // Someone set us up the Utf-8 BOM
    // This case is easy, since we assume that BOM-less
    // files are Utf-8 anyway.  Just skip the BOM and process as usual.
    mFileContents.Append(aStr);
    buffer = mFileContents.BeginWriting() + 3;
  } else {
    if (StringHead(aStr, 2) == "\xFF\xFE") {
      // Someone set us up the Utf-16LE BOM
      nsDependentSubstring str(reinterpret_cast<const char16_t*>(aStr.get()),
                               aStr.Length() / 2);

      AppendUTF16toUTF8(Substring(str, 1), mFileContents);
    } else {
      mFileContents.Append(aStr);
    }

    buffer = mFileContents.BeginWriting();
  }

  char* currSection = nullptr;

  // outer loop tokenizes into lines
  while (char* token = NS_strtok(kNL, &buffer)) {
    if (token[0] == '#' || token[0] == ';') { // it's a comment
      continue;
    }

    token = (char*)NS_strspnp(kWhitespace, token);
    if (!*token) { // empty line
      continue;
    }

    if (token[0] == '[') { // section header!
      ++token;
      currSection = token;

      char* rb = NS_strtok(kRBracket, &token);
      if (!rb || NS_strtok(kWhitespace, &token)) {
        // there's either an unclosed [Section or a [Section]Moretext!
        // we could frankly decide that this INI file is malformed right
        // here and stop, but we won't... keep going, looking for
        // a well-formed [section] to continue working with
        currSection = nullptr;
      }

      continue;
    }

    if (!currSection) {
      // If we haven't found a section header (or we found a malformed
      // section header), don't bother parsing this line.
      continue;
    }

    char* key = token;
    char* e = NS_strtok(kEquals, &token);
    if (!e || !token) {
      continue;
    }

    INIValue* v;
    if (!mSections.Get(currSection, &v)) {
      v = new INIValue(key, token);
      if (!v) {
        return NS_ERROR_OUT_OF_MEMORY;
      }

      mSections.Put(currSection, v);
      continue;
    }

    // Check whether this key has already been specified; overwrite
    // if so, or append if not.
    while (v) {
      if (!strcmp(key, v->key)) {
        v->value = token;
        break;
      }
      if (!v->next) {
        v->next = MakeUnique<INIValue>(key, token);
        if (!v->next) {
          return NS_ERROR_OUT_OF_MEMORY;
        }
        break;
      }
      v = v->next.get();
    }
    NS_ASSERTION(v, "v should never be null coming out of this loop");
  }

  return NS_OK;
}

nsresult
nsINIParser::GetString(const char* aSection, const char* aKey,
                       nsACString& aResult)
{
  INIValue* val;
  mSections.Get(aSection, &val);

  while (val) {
    if (strcmp(val->key, aKey) == 0) {
      aResult.Assign(val->value);
      return NS_OK;
    }

    val = val->next.get();
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsINIParser::GetString(const char* aSection, const char* aKey,
                       char* aResult, uint32_t aResultLen)
{
  INIValue* val;
  mSections.Get(aSection, &val);

  while (val) {
    if (strcmp(val->key, aKey) == 0) {
      strncpy(aResult, val->value, aResultLen);
      aResult[aResultLen - 1] = '\0';
      if (strlen(val->value) >= aResultLen) {
        return NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;
      }

      return NS_OK;
    }

    val = val->next.get();
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsINIParser::GetSections(INISectionCallback aCB, void* aClosure)
{
  for (auto iter = mSections.Iter(); !iter.Done(); iter.Next()) {
    if (!aCB(iter.Key(), aClosure)) {
      break;
    }
  }
  return NS_OK;
}

nsresult
nsINIParser::GetStrings(const char* aSection,
                        INIStringCallback aCB, void* aClosure)
{
  INIValue* val;

  for (mSections.Get(aSection, &val);
       val;
       val = val->next.get()) {

    if (!aCB(val->key, val->value, aClosure)) {
      return NS_OK;
    }
  }

  return NS_OK;
}
