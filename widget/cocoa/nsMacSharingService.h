/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsMacSharingService_h_
#define nsMacSharingService_h_

#include "nsIMacSharingService.h"

class nsMacSharingService : public nsIMacSharingService
{
public:
  nsMacSharingService() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACSHARINGSERVICE

protected:
  virtual ~nsMacSharingService() {}
};

#endif // nsMacSharingService_h_
