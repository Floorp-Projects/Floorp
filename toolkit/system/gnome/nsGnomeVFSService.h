/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGnomeVFSService_h_
#define nsGnomeVFSService_h_

#include "nsIGnomeVFSService.h"
#include "mozilla/Attributes.h"

#define NS_GNOMEVFSSERVICE_CID \
{0x5f43022c, 0x6194, 0x4b37, {0xb2, 0x6d, 0xe4, 0x10, 0x24, 0x62, 0x52, 0x64}}

class nsGnomeVFSService MOZ_FINAL : public nsIGnomeVFSService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGNOMEVFSSERVICE

  nsresult Init();
};

#endif
