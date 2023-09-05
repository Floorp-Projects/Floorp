/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "MacHelpers.h"
#include "MacStringHelpers.h"
#include "nsObjCExceptions.h"

#import <Foundation/Foundation.h>

namespace mozilla {

nsresult GetSelectedCityInfo(nsAString& aCountryCode) {
  NS_OBJC_BEGIN_TRY_BLOCK_RETURN;

  // Can be replaced with [[NSLocale currentLocale] countryCode] once we build
  // with the 10.12 SDK.
  id countryCode = [[NSLocale currentLocale] objectForKey:NSLocaleCountryCode];

  if (![countryCode isKindOfClass:[NSString class]]) {
    return NS_ERROR_FAILURE;
  }

  return mozilla::CopyCocoaStringToXPCOMString((NSString*)countryCode,
                                               aCountryCode);

  NS_OBJC_END_TRY_BLOCK_RETURN(NS_ERROR_FAILURE);
}

}  // namespace mozilla
