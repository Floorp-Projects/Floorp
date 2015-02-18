/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsPackageKitService_h_
#define nsPackageKitService_h_

#include "nsIPackageKitService.h"

#define NS_PACKAGEKITSERVICE_CID \
{0x9c95515e, 0x611d, 0x11e4, {0xb9, 0x7e, 0x60, 0xa4, 0x4c, 0x71, 0x70, 0x42}}

class nsPackageKitService MOZ_FINAL : public nsIPackageKitService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPACKAGEKITSERVICE

  nsresult Init();

private:
  ~nsPackageKitService();
};

#endif
