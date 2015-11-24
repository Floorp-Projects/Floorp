/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsIconListener_h__
#define nsAlertsIconListener_h__

#include "nsCOMPtr.h"
#include "imgINotificationObserver.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

class imgIRequest;

struct NotifyNotification;

class nsAlertsIconListener : public imgINotificationObserver,
                             public nsIObserver,
                             public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_IMGINOTIFICATIONOBSERVER
  NS_DECL_NSIOBSERVER

  nsAlertsIconListener();

  nsresult InitAlertAsync(const nsAString & aImageUrl,
                          const nsAString & aAlertTitle, 
                          const nsAString & aAlertText,
                          bool aAlertTextClickable,
                          const nsAString & aAlertCookie,
                          nsIObserver * aAlertListener,
                          bool aInPrivateBrowsing);

  void SendCallback();
  void SendClosed();

protected:
  virtual ~nsAlertsIconListener();

  nsresult OnLoadComplete(imgIRequest* aRequest);
  nsresult OnFrameComplete(imgIRequest* aRequest);

  /**
   * The only difference between libnotify.so.4 and libnotify.so.1 for these symbols
   * is that notify_notification_new takes three arguments in libnotify.so.4 and
   * four in libnotify.so.1.
   * Passing the fourth argument as NULL is binary compatible.
   */
  typedef void (*NotifyActionCallback)(NotifyNotification*, char*, gpointer);
  typedef bool (*notify_is_initted_t)(void);
  typedef bool (*notify_init_t)(const char*);
  typedef GList* (*notify_get_server_caps_t)(void);
  typedef NotifyNotification* (*notify_notification_new_t)(const char*, const char*, const char*, const char*);
  typedef bool (*notify_notification_show_t)(void*, char*);
  typedef void (*notify_notification_set_icon_from_pixbuf_t)(void*, GdkPixbuf*);
  typedef void (*notify_notification_add_action_t)(void*, const char*, const char*, NotifyActionCallback, gpointer, GFreeFunc);

  nsCOMPtr<imgIRequest> mIconRequest;
  nsCString mAlertTitle;
  nsCString mAlertText;

  nsCOMPtr<nsIObserver> mAlertListener;
  nsString mAlertCookie;

  bool mLoadedFrame;
  bool mAlertHasAction;

  static void* libNotifyHandle;
  static bool libNotifyNotAvail;
  static notify_is_initted_t notify_is_initted;
  static notify_init_t notify_init;
  static notify_get_server_caps_t notify_get_server_caps;
  static notify_notification_new_t notify_notification_new;
  static notify_notification_show_t notify_notification_show;
  static notify_notification_set_icon_from_pixbuf_t notify_notification_set_icon_from_pixbuf;
  static notify_notification_add_action_t notify_notification_add_action;
  NotifyNotification* mNotification;
  gulong mClosureHandler;

  nsresult StartRequest(const nsAString & aImageUrl, bool aInPrivateBrowsing);
  nsresult ShowAlert(GdkPixbuf* aPixbuf);
};

#endif
