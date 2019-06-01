/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MinidumpAnalyzerUtils_h
#define MinidumpAnalyzerUtils_h

#ifdef XP_WIN
#  include <windows.h>
#endif

#include <algorithm>
#include <map>
#include <memory>
#include <string>

namespace CrashReporter {

struct MinidumpAnalyzerOptions {
  bool fullMinidump;
  std::string forceUseModule;
};

extern MinidumpAnalyzerOptions gMinidumpAnalyzerOptions;

#ifdef XP_WIN

static inline std::string WideToMBCP(const std::wstring& wide, unsigned int cp,
                                     bool* success = nullptr) {
  int bufferCharLen = WideCharToMultiByte(cp, 0, wide.c_str(), wide.length(),
                                          nullptr, 0, nullptr, nullptr);
  if (!bufferCharLen) {
    if (success) {
      *success = false;
    }

    return "";
  }

  auto buffer = std::make_unique<char[]>(bufferCharLen);
  if (!buffer) {
    if (success) {
      *success = false;
    }

    return "";
  }

  int result =
      WideCharToMultiByte(cp, 0, wide.c_str(), wide.length(), buffer.get(),
                          bufferCharLen, nullptr, nullptr);
  if (success) {
    *success = result > 0;
  }

  return std::string(buffer.get(), result);
}

static inline std::wstring MBCPToWide(const std::string& aMbStr,
                                      unsigned int aCodepage,
                                      bool* aSuccess = nullptr) {
  int bufferCharLen = MultiByteToWideChar(aCodepage, 0, aMbStr.c_str(),
                                          aMbStr.length(), nullptr, 0);
  if (!bufferCharLen) {
    if (aSuccess) {
      *aSuccess = false;
    }

    return L"";
  }

  auto buffer = std::make_unique<wchar_t[]>(bufferCharLen);
  if (!buffer) {
    if (aSuccess) {
      *aSuccess = false;
    }

    return L"";
  }

  int result =
      MultiByteToWideChar(aCodepage, 0, aMbStr.c_str(), aMbStr.length(),
                          buffer.get(), bufferCharLen);
  if (aSuccess) {
    *aSuccess = result > 0;
  }

  return std::wstring(buffer.get(), result);
}

static inline std::string WideToUTF8(const std::wstring& aWide,
                                     bool* aSuccess = nullptr) {
  return WideToMBCP(aWide, CP_UTF8, aSuccess);
}

static inline std::wstring UTF8ToWide(const std::string& aUtf8Str,
                                      bool* aSuccess = nullptr) {
  return MBCPToWide(aUtf8Str, CP_UTF8, aSuccess);
}

static inline std::string WideToMBCS(const std::wstring& aWide,
                                     bool* aSuccess = nullptr) {
  return WideToMBCP(aWide, CP_ACP, aSuccess);
}

static inline std::string UTF8ToMBCS(const std::string& aUtf8) {
  return WideToMBCS(UTF8ToWide(aUtf8));
}

#endif  // XP_WIN

}  // namespace CrashReporter

#endif  // MinidumpAnalyzerUtils_h
