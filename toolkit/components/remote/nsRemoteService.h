/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:expandtab:shiftwidth=2:tabstop=2:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsRemoteService_h__
#define __nsRemoteService_h__

#include "nsIRemoteService.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsPIDOMWindow.h"

class nsRemoteService final : public nsIRemoteService,
                              public nsIObserver
{
public:
  // We will be a static singleton, so don't use the ordinary methods.
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREMOTESERVICE
  NS_DECL_NSIOBSERVER

  static const char*
  HandleCommandLine(const char* aBuffer, nsIDOMWindow* aWindow,
                    uint32_t aTimestamp);

  nsCOMPtr<nsIRemoteService> mDBusRemoteService;
  nsCOMPtr<nsIRemoteService> mGtkRemoteService;

  nsRemoteService()
    {}
private:
  ~nsRemoteService();

  static void
  SetDesktopStartupIDOrTimestamp(const nsACString& aDesktopStartupID,
                                 uint32_t aTimestamp);
};

#endif // __nsRemoteService_h__
