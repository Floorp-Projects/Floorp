/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_Authenticode_h
#define mozilla_Authenticode_h

#include "mozilla/Maybe.h"
#include "mozilla/UniquePtr.h"

namespace mozilla {

class Authenticode {
 public:
  virtual UniquePtr<wchar_t[]> GetBinaryOrgName(const wchar_t* aFilePath) = 0;
};

}  // namespace mozilla

#endif  // mozilla_Authenticode_h
