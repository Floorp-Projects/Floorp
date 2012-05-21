/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGConfService_h_
#define nsGConfService_h_

#include "nsIGConfService.h"
#include "gconf/gconf-client.h"

#define NS_GCONFSERVICE_CID \
{0xd96d5985, 0xa13a, 0x4bdc, {0x93, 0x86, 0xef, 0x34, 0x8d, 0x7a, 0x97, 0xa1}}

class nsGConfService : public nsIGConfService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGCONFSERVICE

  nsGConfService() : mClient(nsnull) {}
  NS_HIDDEN_(nsresult) Init();

private:
  ~nsGConfService() NS_HIDDEN;

  GConfClient *mClient;
};

#endif
