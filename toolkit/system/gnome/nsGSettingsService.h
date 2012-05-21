/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGSettingsService_h_
#define nsGSettingsService_h_

#include "nsIGSettingsService.h"

#define NS_GSETTINGSSERVICE_CID \
{0xbfd4a9d8, 0xd886, 0x4161, {0x81, 0xef, 0x88, 0x68, 0xda, 0x11, 0x41, 0x70}}

class nsGSettingsService : public nsIGSettingsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGSETTINGSSERVICE

  NS_HIDDEN_(nsresult) Init();

private:
  ~nsGSettingsService();
};

#endif

