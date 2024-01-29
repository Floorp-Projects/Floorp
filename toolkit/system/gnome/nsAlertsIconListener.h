/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsAlertsIconListener_h__
#define nsAlertsIconListener_h__

#include "nsCOMPtr.h"
#include "nsIAlertsService.h"
#include "nsString.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"

#include <gdk-pixbuf/gdk-pixbuf.h>

class nsIAlertNotification;
class nsICancelable;
class nsSystemAlertsService;

struct NotifyNotification;

class nsAlertsIconListener : public nsIAlertNotificationImageListener,
                             public nsIObserver,
                             public nsSupportsWeakReference {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIALERTNOTIFICATIONIMAGELISTENER
  NS_DECL_NSIOBSERVER

  nsAlertsIconListener(nsSystemAlertsService* aBackend,
                       const nsAString& aAlertName);

  nsresult InitAlertAsync(nsIAlertNotification* aAlert,
                          nsIObserver* aAlertListener);
  nsresult Close();

  void SendCallback();
  void SendClosed();

 protected:
  virtual ~nsAlertsIconListener();

  /**
   * The only difference between libnotify.so.4 and libnotify.so.1 for these
   * symbols is that notify_notification_new takes three arguments in
   * libnotify.so.4 and four in libnotify.so.1. Passing the fourth argument as
   * NULL is binary compatible.
   */
  using NotifyActionCallback = void (*)(NotifyNotification*, char*, gpointer);
  using notify_is_initted_t = bool (*)();
  using notify_init_t = bool (*)(const char*);
  using notify_get_server_caps_t = GList* (*)();
  using notify_notification_new_t = NotifyNotification* (*)(const char*,
                                                            const char*,
                                                            const char*,
                                                            const char*);
  using notify_notification_show_t = bool (*)(void*, GError**);
  using notify_notification_set_icon_from_pixbuf_t = void (*)(void*,
                                                              GdkPixbuf*);
  using notify_notification_add_action_t = void (*)(void*, const char*,
                                                    const char*,
                                                    NotifyActionCallback,
                                                    gpointer, GFreeFunc);
  using notify_notification_close_t = bool (*)(void*, GError**);
  using notify_notification_set_hint_t = void (*)(NotifyNotification*,
                                                  const char*, GVariant*);
  using notify_notification_set_timeout_t = void (*)(NotifyNotification*, gint);

  nsCOMPtr<nsICancelable> mIconRequest;
  nsCString mAlertTitle;
  nsCString mAlertText;

  nsCOMPtr<nsIObserver> mAlertListener;
  nsString mAlertCookie;
  nsString mAlertName;

  RefPtr<nsSystemAlertsService> mBackend;

  bool mAlertHasAction;
  bool mAlertIsSilent;
  bool mAlertRequiresInteraction;

  static void* libNotifyHandle;
  static bool libNotifyNotAvail;
  static notify_is_initted_t notify_is_initted;
  static notify_init_t notify_init;
  static notify_get_server_caps_t notify_get_server_caps;
  static notify_notification_new_t notify_notification_new;
  static notify_notification_show_t notify_notification_show;
  static notify_notification_set_icon_from_pixbuf_t
      notify_notification_set_icon_from_pixbuf;
  static notify_notification_add_action_t notify_notification_add_action;
  static notify_notification_close_t notify_notification_close;
  static notify_notification_set_hint_t notify_notification_set_hint;
  static notify_notification_set_timeout_t notify_notification_set_timeout;
  NotifyNotification* mNotification;
  gulong mClosureHandler;

  nsresult ShowAlert(GdkPixbuf* aPixbuf);

  void NotifyFinished();
};

#endif
