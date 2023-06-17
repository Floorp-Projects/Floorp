/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_nativeosfileinternalservice_h__
#define mozilla_nativeosfileinternalservice_h__

#include "nsINativeOSFileInternals.h"

#include "nsISupports.h"

namespace mozilla {

class NativeOSFileInternalsService final
    : public nsINativeOSFileInternalsService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINATIVEOSFILEINTERNALSSERVICE
 private:
  ~NativeOSFileInternalsService() = default;
  // Avoid accidental use of built-in operator=
  void operator=(const NativeOSFileInternalsService& other) = delete;
};

}  // namespace mozilla

#endif  // mozilla_finalizationwitnessservice_h__
