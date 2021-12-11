/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//-----------------------------------------------------------------------------
// Non-Windows
//-----------------------------------------------------------------------------
#ifndef XP_WIN

#  include "nsAString.h"
#  include "nsReadableUtils.h"
#  include "nsString.h"

nsresult NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput) {
  CopyUTF8toUTF16(aInput, aOutput);
  return NS_OK;
}

nsresult NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput) {
  CopyUTF16toUTF8(aInput, aOutput);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// XP_WIN
//-----------------------------------------------------------------------------
#else

#  include <windows.h>
#  include "nsString.h"
#  include "nsAString.h"
#  include "nsReadableUtils.h"

using namespace mozilla;

nsresult NS_CopyNativeToUnicode(const nsACString& aInput, nsAString& aOutput) {
  uint32_t inputLen = aInput.Length();

  nsACString::const_iterator iter;
  aInput.BeginReading(iter);

  const char* buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;
  int n = ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, nullptr, 0);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    char16ptr_t result = aOutput.BeginWriting();
    ::MultiByteToWideChar(CP_ACP, 0, buf, inputLen, result, resultLen);
  }
  return NS_OK;
}

nsresult NS_CopyUnicodeToNative(const nsAString& aInput, nsACString& aOutput) {
  uint32_t inputLen = aInput.Length();

  nsAString::const_iterator iter;
  aInput.BeginReading(iter);

  char16ptr_t buf = iter.get();

  // determine length of result
  uint32_t resultLen = 0;

  int n = ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, nullptr, 0, nullptr,
                                nullptr);
  if (n > 0) {
    resultLen += n;
  }

  // allocate sufficient space
  if (!aOutput.SetLength(resultLen, fallible)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  if (resultLen > 0) {
    char* result = aOutput.BeginWriting();

    // default "defaultChar" is '?', which is an illegal character on windows
    // file system.  That will cause file uncreatable. Change it to '_'
    const char defaultChar = '_';

    ::WideCharToMultiByte(CP_ACP, 0, buf, inputLen, result, resultLen,
                          &defaultChar, nullptr);
  }
  return NS_OK;
}

#endif
