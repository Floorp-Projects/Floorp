/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAlertsIconListener.h"
#include "imgIContainer.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "nsNetUtil.h"
#include "nsIImageToPixbuf.h"
#include "nsIStringBundle.h"
#include "nsIObserverService.h"
#include "nsCRT.h"

#include <dlfcn.h>
#include <gdk/gdk.h>

static bool gHasActions = false;
static bool gHasCaps = false;

void* nsAlertsIconListener::libNotifyHandle = nullptr;
bool nsAlertsIconListener::libNotifyNotAvail = false;
nsAlertsIconListener::notify_is_initted_t nsAlertsIconListener::notify_is_initted = nullptr;
nsAlertsIconListener::notify_init_t nsAlertsIconListener::notify_init = nullptr;
nsAlertsIconListener::notify_get_server_caps_t nsAlertsIconListener::notify_get_server_caps = nullptr;
nsAlertsIconListener::notify_notification_new_t nsAlertsIconListener::notify_notification_new = nullptr;
nsAlertsIconListener::notify_notification_show_t nsAlertsIconListener::notify_notification_show = nullptr;
nsAlertsIconListener::notify_notification_set_icon_from_pixbuf_t nsAlertsIconListener::notify_notification_set_icon_from_pixbuf = nullptr;
nsAlertsIconListener::notify_notification_add_action_t nsAlertsIconListener::notify_notification_add_action = nullptr;

static void notify_action_cb(NotifyNotification *notification,
                             gchar *action, gpointer user_data)
{
  nsAlertsIconListener* alert = static_cast<nsAlertsIconListener*> (user_data);
  alert->SendCallback();
}

static void notify_closed_marshal(GClosure* closure,
                                  GValue* return_value,
                                  guint n_param_values,
                                  const GValue* param_values,
                                  gpointer invocation_hint,
                                  gpointer marshal_data)
{
  NS_ABORT_IF_FALSE(n_param_values >= 1, "No object in params");

  nsAlertsIconListener* alert =
    static_cast<nsAlertsIconListener*>(closure->data);
  alert->SendClosed();
  NS_RELEASE(alert);
}

static GdkPixbuf*
GetPixbufFromImgRequest(imgIRequest* aRequest)
{
  nsCOMPtr<imgIContainer> image;
  nsresult rv = aRequest->GetImage(getter_AddRefs(image));
  if (NS_FAILED(rv)) {
    return nullptr;
  }

  nsCOMPtr<nsIImageToPixbuf> imgToPixbuf =
    do_GetService("@mozilla.org/widget/image-to-gdk-pixbuf;1");

  return imgToPixbuf->ConvertImageToPixbuf(image);
}

NS_IMPL_ISUPPORTS(nsAlertsIconListener, imgINotificationObserver,
                  nsIObserver, nsISupportsWeakReference)

nsAlertsIconListener::nsAlertsIconListener()
: mLoadedFrame(false),
  mNotification(nullptr)
{
  if (!libNotifyHandle && !libNotifyNotAvail) {
    libNotifyHandle = dlopen("libnotify.so.4", RTLD_LAZY);
    if (!libNotifyHandle) {
      libNotifyHandle = dlopen("libnotify.so.1", RTLD_LAZY);
      if (!libNotifyHandle) {
        libNotifyNotAvail = true;
        return;
      }
    }

    notify_is_initted = (notify_is_initted_t)dlsym(libNotifyHandle, "notify_is_initted");
    notify_init = (notify_init_t)dlsym(libNotifyHandle, "notify_init");
    notify_get_server_caps = (notify_get_server_caps_t)dlsym(libNotifyHandle, "notify_get_server_caps");
    notify_notification_new = (notify_notification_new_t)dlsym(libNotifyHandle, "notify_notification_new");
    notify_notification_show = (notify_notification_show_t)dlsym(libNotifyHandle, "notify_notification_show");
    notify_notification_set_icon_from_pixbuf = (notify_notification_set_icon_from_pixbuf_t)dlsym(libNotifyHandle, "notify_notification_set_icon_from_pixbuf");
    notify_notification_add_action = (notify_notification_add_action_t)dlsym(libNotifyHandle, "notify_notification_add_action");
    if (!notify_is_initted || !notify_init || !notify_get_server_caps || !notify_notification_new || !notify_notification_show || !notify_notification_set_icon_from_pixbuf || !notify_notification_add_action) {
      dlclose(libNotifyHandle);
      libNotifyHandle = nullptr;
    }
  }
}

nsAlertsIconListener::~nsAlertsIconListener()
{
  if (mIconRequest)
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
  // Don't dlclose libnotify as it uses atexit().
}

NS_IMETHODIMP
nsAlertsIconListener::Notify(imgIRequest *aRequest, int32_t aType, const nsIntRect* aData)
{
  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    return OnLoadComplete(aRequest);
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    return OnFrameComplete(aRequest);
  }

  return NS_OK;
}

nsresult
nsAlertsIconListener::OnLoadComplete(imgIRequest* aRequest)
{
  NS_ASSERTION(mIconRequest == aRequest, "aRequest does not match!");

  uint32_t imgStatus = imgIRequest::STATUS_ERROR;
  nsresult rv = aRequest->GetImageStatus(&imgStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  if ((imgStatus & imgIRequest::STATUS_ERROR) && !mLoadedFrame) {
    // We have an error getting the image. Display the notification with no icon.
    ShowAlert(nullptr);

    // Cancel any pending request
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }

  return NS_OK;
}

nsresult
nsAlertsIconListener::OnFrameComplete(imgIRequest* aRequest)
{
  NS_ASSERTION(mIconRequest == aRequest, "aRequest does not match!");

  if (mLoadedFrame)
    return NS_OK; // only use one frame

  GdkPixbuf* imagePixbuf = GetPixbufFromImgRequest(aRequest);
  if (!imagePixbuf) {
    ShowAlert(nullptr);
  } else {
    ShowAlert(imagePixbuf);
    g_object_unref(imagePixbuf);
  }

  mLoadedFrame = true;

  // Cancel any pending request (multipart image loading/decoding for instance)
  mIconRequest->Cancel(NS_BINDING_ABORTED);
  mIconRequest = nullptr;

  return NS_OK;
}

nsresult
nsAlertsIconListener::ShowAlert(GdkPixbuf* aPixbuf)
{
  mNotification = notify_notification_new(mAlertTitle.get(), mAlertText.get(),
                                          nullptr, nullptr);

  if (!mNotification)
    return NS_ERROR_OUT_OF_MEMORY;

  if (aPixbuf)
    notify_notification_set_icon_from_pixbuf(mNotification, aPixbuf);

  NS_ADDREF(this);
  if (mAlertHasAction) {
    // What we put as the label doesn't matter here, if the action
    // string is "default" then that makes the entire bubble clickable
    // rather than creating a button.
    notify_notification_add_action(mNotification, "default", "Activate",
                                   notify_action_cb, this, nullptr);
  }

  // Fedora 10 calls NotifyNotification "closed" signal handlers with a
  // different signature, so a marshaller is used instead of a C callback to
  // get the user_data (this) in a parseable format.  |closure| is created
  // with a floating reference, which gets sunk by g_signal_connect_closure().
  GClosure* closure = g_closure_new_simple(sizeof(GClosure), this);
  g_closure_set_marshal(closure, notify_closed_marshal);
  mClosureHandler = g_signal_connect_closure(mNotification, "closed", closure, FALSE);
  gboolean result = notify_notification_show(mNotification, nullptr);

  if (result && mAlertListener)
    mAlertListener->Observe(nullptr, "alertshow", mAlertCookie.get());

  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsAlertsIconListener::StartRequest(const nsAString & aImageUrl)
{
  if (mIconRequest) {
    // Another icon request is already in flight.  Kill it.
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }

  nsCOMPtr<nsIURI> imageUri;
  NS_NewURI(getter_AddRefs(imageUri), aImageUrl);
  if (!imageUri)
    return ShowAlert(nullptr);

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  if (!il)
    return ShowAlert(nullptr);

  nsresult rv = il->LoadImageXPCOM(imageUri, nullptr, nullptr,
                                   NS_LITERAL_STRING("default"), nullptr, nullptr,
                                   this, nullptr, nsIRequest::LOAD_NORMAL, nullptr,
                                   0 /* use default */, getter_AddRefs(mIconRequest));
  if (NS_FAILED(rv))
    return rv;

  mIconRequest->StartDecoding();

  return NS_OK;
}

void
nsAlertsIconListener::SendCallback()
{
  if (mAlertListener)
    mAlertListener->Observe(nullptr, "alertclickcallback", mAlertCookie.get());
}

void
nsAlertsIconListener::SendClosed()
{
  if (mNotification) {
    g_object_unref(mNotification);
    mNotification = nullptr;
  }
  if (mAlertListener)
    mAlertListener->Observe(nullptr, "alertfinished", mAlertCookie.get());
}

NS_IMETHODIMP
nsAlertsIconListener::Observe(nsISupports *aSubject, const char *aTopic,
                              const char16_t *aData) {
  // We need to close any open notifications upon application exit, otherwise
  // we will leak since libnotify holds a ref for us.
  if (!nsCRT::strcmp(aTopic, "quit-application") && mNotification) {
    g_signal_handler_disconnect(mNotification, mClosureHandler);
    g_object_unref(mNotification);
    mNotification = nullptr;
    Release(); // equivalent to NS_RELEASE(this)
  }
  return NS_OK;
}

nsresult
nsAlertsIconListener::InitAlertAsync(const nsAString & aImageUrl,
                                     const nsAString & aAlertTitle, 
                                     const nsAString & aAlertText,
                                     bool aAlertTextClickable,
                                     const nsAString & aAlertCookie,
                                     nsIObserver * aAlertListener)
{
  if (!libNotifyHandle)
    return NS_ERROR_FAILURE;

  if (!notify_is_initted()) {
    // Give the name of this application to libnotify
    nsCOMPtr<nsIStringBundleService> bundleService = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);

    nsAutoCString appShortName;
    if (bundleService) {
      nsCOMPtr<nsIStringBundle> bundle;
      bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                  getter_AddRefs(bundle));
      nsAutoString appName;

      if (bundle) {
        bundle->GetStringFromName(MOZ_UTF16("brandShortName"),
                                  getter_Copies(appName));
        appShortName = NS_ConvertUTF16toUTF8(appName);
      } else {
        NS_WARNING("brand.properties not present, using default application name");
        appShortName.AssignLiteral("Mozilla");
      }
    } else {
      appShortName.AssignLiteral("Mozilla");
    }

    if (!notify_init(appShortName.get()))
      return NS_ERROR_FAILURE;

    GList *server_caps = notify_get_server_caps();
    if (server_caps) {
      gHasCaps = true;
      for (GList* cap = server_caps; cap != nullptr; cap = cap->next) {
        if (!strcmp((char*) cap->data, "actions")) {
          gHasActions = true;
          break;
        }
      }
      g_list_foreach(server_caps, (GFunc)g_free, nullptr);
      g_list_free(server_caps);
    }
  }

  if (!gHasCaps) {
    // if notify_get_server_caps() failed above we need to assume
    // there is no notification-server to display anything
    return NS_ERROR_FAILURE;
  }

  if (!gHasActions && aAlertTextClickable)
    return NS_ERROR_FAILURE; // No good, fallback to XUL

  nsCOMPtr<nsIObserverService> obsServ =
      do_GetService("@mozilla.org/observer-service;1");
  if (obsServ)
    obsServ->AddObserver(this, "quit-application", true);

  // Workaround for a libnotify bug - blank titles aren't dealt with
  // properly so we use a space
  if (aAlertTitle.IsEmpty()) {
    mAlertTitle = NS_LITERAL_CSTRING(" ");
  } else {
    mAlertTitle = NS_ConvertUTF16toUTF8(aAlertTitle);
  }

  mAlertText = NS_ConvertUTF16toUTF8(aAlertText);
  mAlertHasAction = aAlertTextClickable;

  mAlertListener = aAlertListener;
  mAlertCookie = aAlertCookie;

  return StartRequest(aImageUrl);
}
