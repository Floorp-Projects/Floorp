/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSANDROIDNETWORKLINKSERVICE_H_
#define NSANDROIDNETWORKLINKSERVICE_H_

#include "nsINetworkLinkService.h"

class nsAndroidNetworkLinkService: public nsINetworkLinkService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINETWORKLINKSERVICE

  nsAndroidNetworkLinkService();

private:
  virtual ~nsAndroidNetworkLinkService();
};

#endif /* NSANDROIDNETWORKLINKSERVICE_H_ */
