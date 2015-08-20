/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsString.h"
#include "MacHelpers.h"

#import <Foundation/Foundation.h>

namespace mozilla {

nsresult
GetSelectedCityInfo(nsAString& aCountryCode)
{
  NSDictionary* selected_city =
    [[NSUserDefaults standardUserDefaults]
      objectForKey:@"com.apple.preferences.timezone.selected_city"];
  NSString *countryCode = (NSString *)
    [selected_city objectForKey:@"CountryCode"];
  const char *countryCodeUTF8 = [countryCode UTF8String];

  if (!countryCodeUTF8) {
    return NS_ERROR_FAILURE;
  }

  AppendUTF8toUTF16(countryCodeUTF8, aCountryCode);
  return NS_OK;
}

} // namespace Mozilla

