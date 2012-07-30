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

#include <gdk/gdk.h>

// Compatibility macro for <libnotify-0.7
#ifndef NOTIFY_CHECK_VERSION
#define NOTIFY_CHECK_VERSION(x,y,z) 0
#endif

static bool gHasActions = false;
static bool gHasCaps = false;

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

NS_IMPL_ISUPPORTS4(nsAlertsIconListener, imgIContainerObserver,
                   imgIDecoderObserver, nsIObserver, nsISupportsWeakReference)

nsAlertsIconListener::nsAlertsIconListener()
: mLoadedFrame(false),
  mNotification(NULL)
{
}

nsAlertsIconListener::~nsAlertsIconListener()
{
  if (mIconRequest)
    mIconRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
}

NS_IMETHODIMP
nsAlertsIconListener::OnStartRequest(imgIRequest* aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStartDecode(imgIRequest* aRequest)
{
  return NS_OK;
}


NS_IMETHODIMP
nsAlertsIconListener::OnStartContainer(imgIRequest* aRequest,
                                       imgIContainer* aContainer)
{
  return NS_OK;
}


NS_IMETHODIMP
nsAlertsIconListener::OnStartFrame(imgIRequest* aRequest,
                                   PRUint32 aFrame)
{
  return NS_OK;
}


NS_IMETHODIMP
nsAlertsIconListener::OnDataAvailable(imgIRequest* aRequest,
                                      bool aCurrentFrame,
                                      const nsIntRect* aRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStopContainer(imgIRequest* aRequest,
                                      imgIContainer* aContainer)
{
  return NS_OK;
}


NS_IMETHODIMP
nsAlertsIconListener::OnStopDecode(imgIRequest* aRequest,
                                   nsresult status,
                                   const PRUnichar* statusArg)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::FrameChanged(imgIRequest* aRequest, 
                                   imgIContainer* aContainer,
                                   const nsIntRect* aDirtyRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStopRequest(imgIRequest* aRequest,
                                    bool aIsLastPart)
{
  PRUint32 imgStatus = imgIRequest::STATUS_ERROR;
  nsresult rv = aRequest->GetImageStatus(&imgStatus);
  NS_ENSURE_SUCCESS(rv, rv);
  if (imgStatus == imgIRequest::STATUS_ERROR && !mLoadedFrame) {
    // We have an error getting the image. Display the notification with no icon.
    ShowAlert(NULL);
  }

  if (mIconRequest) {
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nullptr;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnDiscard(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnImageIsAnimated(imgIRequest *aRequest)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStopFrame(imgIRequest* aRequest,
                                  PRUint32 aFrame)
{
  if (aRequest != mIconRequest)
    return NS_ERROR_FAILURE;

  if (mLoadedFrame)
    return NS_OK; // only use one frame

  nsCOMPtr<imgIContainer> image;
  nsresult rv = aRequest->GetImage(getter_AddRefs(image));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIImageToPixbuf> imgToPixbuf =
    do_GetService("@mozilla.org/widget/image-to-gdk-pixbuf;1");

  GdkPixbuf* imagePixbuf = imgToPixbuf->ConvertImageToPixbuf(image);
  if (!imagePixbuf)
    return NS_ERROR_FAILURE;

  ShowAlert(imagePixbuf);

  g_object_unref(imagePixbuf);

  mLoadedFrame = true;
  return NS_OK;
}

nsresult
nsAlertsIconListener::ShowAlert(GdkPixbuf* aPixbuf)
{
  mNotification = notify_notification_new(mAlertTitle.get(),
                                          mAlertText.get(),
                                          NULL
// >=libnotify-0.7.0 has no support for attaching to widgets
#if !NOTIFY_CHECK_VERSION(0,7,0)
                                          , NULL
#endif
                                          );

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
                                   notify_action_cb, this, NULL);
  }

  // Fedora 10 calls NotifyNotification "closed" signal handlers with a
  // different signature, so a marshaller is used instead of a C callback to
  // get the user_data (this) in a parseable format.  |closure| is created
  // with a floating reference, which gets sunk by g_signal_connect_closure().
  GClosure* closure = g_closure_new_simple(sizeof(GClosure), this);
  g_closure_set_marshal(closure, notify_closed_marshal);
  mClosureHandler = g_signal_connect_closure(mNotification, "closed", closure, FALSE);
  gboolean result = notify_notification_show(mNotification, NULL);

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
    return ShowAlert(NULL);

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  if (!il)
    return ShowAlert(NULL);

  return il->LoadImage(imageUri, nullptr, nullptr, nullptr, nullptr, this,
                       nullptr, nsIRequest::LOAD_NORMAL, nullptr, nullptr,
                       nullptr, getter_AddRefs(mIconRequest));
}

void
nsAlertsIconListener::SendCallback()
{
  if (mAlertListener)
    mAlertListener->Observe(NULL, "alertclickcallback", mAlertCookie.get());
}

void
nsAlertsIconListener::SendClosed()
{
  if (mNotification) {
    g_object_unref(mNotification);
    mNotification = NULL;
  }
  if (mAlertListener)
    mAlertListener->Observe(NULL, "alertfinished", mAlertCookie.get());
}

NS_IMETHODIMP
nsAlertsIconListener::Observe(nsISupports *aSubject, const char *aTopic,
                              const PRUnichar *aData) {
  // We need to close any open notifications upon application exit, otherwise
  // we will leak since libnotify holds a ref for us.
  if (!nsCRT::strcmp(aTopic, "quit-application") && mNotification) {
    g_signal_handler_disconnect(mNotification, mClosureHandler);
    g_object_unref(mNotification);
    mNotification = NULL;
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
  if (!notify_is_initted()) {
    // Give the name of this application to libnotify
    nsCOMPtr<nsIStringBundleService> bundleService = 
      do_GetService(NS_STRINGBUNDLE_CONTRACTID);

    nsCAutoString appShortName;
    if (bundleService) {
      nsCOMPtr<nsIStringBundle> bundle;
      bundleService->CreateBundle("chrome://branding/locale/brand.properties",
                                  getter_AddRefs(bundle));
      nsAutoString appName;

      if (bundle) {
        bundle->GetStringFromName(NS_LITERAL_STRING("brandShortName").get(),
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
      for (GList* cap = server_caps; cap != NULL; cap = cap->next) {
        if (!strcmp((char*) cap->data, "actions")) {
          gHasActions = true;
          break;
        }
      }
      g_list_foreach(server_caps, (GFunc)g_free, NULL);
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
