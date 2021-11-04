/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __mozilla_GkRustUtils_h
#define __mozilla_GkRustUtils_h

#include "nsString.h"

class GkRustUtils {
 public:
  static bool ParseSemVer(const nsACString& aVersion, uint64_t& aOutMajor,
                          uint64_t& aOutMinor, uint64_t& aOutPatch);
};

#endif
