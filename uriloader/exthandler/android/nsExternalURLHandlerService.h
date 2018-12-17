/* -*- Mode: c++; c-basic-offset: 2; tab-width: 20; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSEXTERNALURLHANDLERSERVICE_H
#define NSEXTERNALURLHANDLERSERVICE_H

#include "nsIExternalURLHandlerService.h"

class nsExternalURLHandlerService final : public nsIExternalURLHandlerService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEXTERNALURLHANDLERSERVICE

  nsExternalURLHandlerService();

 private:
  ~nsExternalURLHandlerService();
};

#endif  // NSEXTERNALURLHANDLERSERVICE_H
