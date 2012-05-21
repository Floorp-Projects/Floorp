/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsParentalControlsServiceWin_h__
#define nsParentalControlsServiceWin_h__

#include "nsIParentalControlsService.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsIURI.h"

// wpcevents.h requires this be elevated
#if (WINVER < 0x0600)
# undef WINVER
# define WINVER 0x0600
#endif

#include <wpcapi.h>
#include <wpcevent.h>

class nsParentalControlsServiceWin : public nsIParentalControlsService
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIPARENTALCONTROLSSERVICE

  nsParentalControlsServiceWin();
  virtual ~nsParentalControlsServiceWin();

private:
  bool mEnabled;
  REGHANDLE mProvider;
  IWindowsParentalControls * mPC;

  void LogFileDownload(bool blocked, nsIURI *aSource, nsIFile *aTarget);
};

#endif /* nsParentalControlsServiceWin_h__ */
