/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef nsPrintSettingsServiceAndroid_h
#define nsPrintSettingsServiceAndroid_h

#include "nsPrintSettingsService.h"
#include "nsIPrintSettings.h"

class nsPrintSettingsServiceAndroid final : public nsPrintSettingsService
{
public:
  nsPrintSettingsServiceAndroid() {}
  virtual ~nsPrintSettingsServiceAndroid() {}

  nsresult _CreatePrintSettings(nsIPrintSettings** _retval) override;
};

#endif // nsPrintSettingsServiceAndroid_h
