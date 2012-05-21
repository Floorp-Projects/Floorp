/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsIconListener_h__
#define nsAlertsIconListener_h__

#include "nsCOMPtr.h"
#include "imgIDecoderObserver.h"
#include "nsStringAPI.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

class imgIRequest;

class nsAlertsIconListener : public imgIDecoderObserver,
                             public nsIObserver,
                             public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGICONTAINEROBSERVER
  NS_DECL_IMGIDECODEROBSERVER
  NS_DECL_NSIOBSERVER

  nsAlertsIconListener();
  virtual ~nsAlertsIconListener();

  nsresult InitAlertAsync(const nsAString & aImageUrl,
                          const nsAString & aAlertTitle, 
                          const nsAString & aAlertText,
                          bool aAlertTextClickable,
                          const nsAString & aAlertCookie,
                          nsIObserver * aAlertListener);

  void SendCallback();
  void SendClosed();

protected:
  nsCOMPtr<imgIRequest> mIconRequest;
  nsCString mAlertTitle;
  nsCString mAlertText;

  nsCOMPtr<nsIObserver> mAlertListener;
  nsString mAlertCookie;

  bool mLoadedFrame;
  bool mAlertHasAction;

  NotifyNotification* mNotification;
  gulong mClosureHandler;

  nsresult StartRequest(const nsAString & aImageUrl);
  nsresult ShowAlert(GdkPixbuf* aPixbuf);
};

#endif
