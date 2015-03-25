/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGIOService_h_
#define nsGIOService_h_

#include "nsIGIOService.h"

#define NS_GIOSERVICE_CID \
{0xe3a1f3c9, 0x3ae1, 0x4b40, {0xa5, 0xe0, 0x7b, 0x45, 0x7f, 0xc9, 0xa9, 0xad}}

class nsGIOService final : public nsIGIOService
{
  ~nsGIOService() {}

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGIOSERVICE
};

#endif

