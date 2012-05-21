/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#ifndef __nsUserInfoMac_h
#define __nsUserInfoMac_h

#include "nsIUserInfo.h"
#include "nsReadableUtils.h"

class nsUserInfo: public nsIUserInfo
{
public:
  nsUserInfo();
  virtual ~nsUserInfo() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIUSERINFO
  
  nsresult GetPrimaryEmailAddress(nsCString &aEmailAddress);  
};

#endif /* __nsUserInfo_h */
