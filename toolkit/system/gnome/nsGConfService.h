/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGConfService_h_
#define nsGConfService_h_

#include "nsIGConfService.h"
#include "mozilla/Attributes.h"

extern "C" {
  struct _GConfClient;
  typedef struct _GConfClient GConfClient;
}

class nsGConfService final : public nsIGConfService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIGCONFSERVICE

  nsGConfService() : mClient(nullptr) {}
  nsresult Init();

 private:
  ~nsGConfService();

  GConfClient *mClient;
};

#endif
