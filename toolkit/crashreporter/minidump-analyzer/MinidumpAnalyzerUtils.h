/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MinidumpAnalyzerUtils_h
#define MinidumpAnalyzerUtils_h

#ifdef XP_WIN
#include <windows.h>
#endif

#include <vector>
#include <map>
#include <string>
#include <algorithm>

namespace CrashReporter {

struct MinidumpAnalyzerOptions {
  bool fullMinidump;
  std::string forceUseModule;
};

extern MinidumpAnalyzerOptions gMinidumpAnalyzerOptions;

#ifdef XP_WIN

#if !defined(_MSC_VER)
static inline std::string
WideToMBCP(const std::wstring& wide, unsigned int cp, bool* success = nullptr)
{
  char* buffer = nullptr;
  int buffer_size = WideCharToMultiByte(cp, 0, wide.c_str(),
                                        -1, nullptr, 0, nullptr, nullptr);
  if (buffer_size == 0) {
    if (success) {
      *success = false;
    }

    return "";
  }

  buffer = new char[buffer_size];
  if (buffer == nullptr) {
    if (success) {
      *success = false;
    }

    return "";
  }

  WideCharToMultiByte(cp, 0, wide.c_str(),
                      -1, buffer, buffer_size, nullptr, nullptr);
  std::string mb = buffer;
  delete [] buffer;

  if (success) {
    *success = true;
  }

  return mb;
}
#endif /* !defined(_MSC_VER) */

static inline std::wstring
UTF8ToWide(const std::string& aUtf8Str, bool *aSuccess = nullptr)
{
  wchar_t* buffer = nullptr;
  int buffer_size = MultiByteToWideChar(CP_UTF8, 0, aUtf8Str.c_str(),
                                        -1, nullptr, 0);
  if (buffer_size == 0) {
    if (aSuccess) {
      *aSuccess = false;
    }

    return L"";
  }

  buffer = new wchar_t[buffer_size];

  if (buffer == nullptr) {
    if (aSuccess) {
      *aSuccess = false;
    }

    return L"";
  }

  MultiByteToWideChar(CP_UTF8, 0, aUtf8Str.c_str(),
                      -1, buffer, buffer_size);
  std::wstring str = buffer;
  delete [] buffer;

  if (aSuccess) {
    *aSuccess = true;
  }

  return str;
}

static inline std::string
WideToMBCS(const std::wstring &inp) {
  int buffer_size = WideCharToMultiByte(CP_ACP, 0, inp.c_str(), -1,
                                        nullptr, 0, NULL, NULL);
  if (buffer_size == 0) {
    return "";
  }

  std::vector<char> buffer(buffer_size);
  buffer[0] = 0;

  WideCharToMultiByte(CP_ACP, 0, inp.c_str(), -1,
                      buffer.data(), buffer_size, NULL, NULL);

  return buffer.data();
}

static inline std::string
UTF8toMBCS(const std::string &inp) {
  std::wstring wide = UTF8ToWide(inp);
  std::string ret = WideToMBCS(wide);
  return ret;
}

#endif // XP_WIN

// Check if a file exists at the specified path

static inline bool
FileExists(const std::string& aPath)
{
#if defined(XP_WIN)
  DWORD attrs = GetFileAttributes(UTF8ToWide(aPath).c_str());
  return (attrs != INVALID_FILE_ATTRIBUTES);
#else // Non-Windows
  struct stat sb;
  int ret = stat(aPath.c_str(), &sb);
  if (ret == -1 || !(sb.st_mode & S_IFREG)) {
    return false;
  }

  return true;
#endif // XP_WIN
}

} // namespace

#endif // MinidumpAnalyzerUtils_h
