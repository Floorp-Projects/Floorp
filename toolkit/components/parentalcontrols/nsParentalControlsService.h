/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsParentalControlsService_h__
#define nsParentalControlsService_h__

#include "nsIParentalControlsService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIURI.h"

#if defined(XP_WIN)
#  include <wpcapi.h>
#  include <wpcevent.h>
#endif

class nsParentalControlsService : public nsIParentalControlsService {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARENTALCONTROLSSERVICE

  nsParentalControlsService();

 protected:
  virtual ~nsParentalControlsService();

 private:
  bool mEnabled;
#if defined(XP_WIN)
  REGHANDLE mProvider;
  IWindowsParentalControls* mPC;
  void LogFileDownload(bool blocked, nsIURI* aSource, nsIFile* aTarget);
#endif
};

#endif /* nsParentalControlsService_h__ */
