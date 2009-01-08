/* -*- Mode: C++; tab-width: 2; indent-tabs-mode:nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsAlertsIconListener.h"
#include "imgILoader.h"
#include "imgIRequest.h"
#include "gfxIImageFrame.h"
#include "nsIImage.h"
#include "nsNetUtil.h"
#include "nsIImageToPixbuf.h"
#include "nsIStringBundle.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <libnotify/notify.h>

NS_IMPL_ISUPPORTS2(nsAlertsIconListener, imgIContainerObserver, imgIDecoderObserver)

nsAlertsIconListener::nsAlertsIconListener()
: mLoadedFrame(PR_FALSE)
{
  MOZ_COUNT_CTOR(nsAlertsIconListener);
}

nsAlertsIconListener::~nsAlertsIconListener()
{
  MOZ_COUNT_DTOR(nsAlertsIconListener);

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
                                   gfxIImageFrame* aFrame)
{
  return NS_OK;
}


NS_IMETHODIMP
nsAlertsIconListener::OnDataAvailable(imgIRequest* aRequest,
                                      gfxIImageFrame* aFrame,
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
nsAlertsIconListener::FrameChanged(imgIContainer* aContainer,
                                   gfxIImageFrame* aFrame,
                                   nsIntRect* aDirtyRect)
{
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStopRequest(imgIRequest* aRequest,
                                    PRBool aIsLastPart)
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
    mIconRequest = nsnull;
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAlertsIconListener::OnStopFrame(imgIRequest* aRequest,
                                  gfxIImageFrame* aFrame)
{
  if (aRequest != mIconRequest)
    return NS_ERROR_FAILURE;

  if (mLoadedFrame)
    return NS_OK; // only use one frame

  nsCOMPtr<gfxIImageFrame> frame = aFrame;
  nsCOMPtr<nsIImage> image = do_GetInterface(frame);
  if (!image)
    return NS_ERROR_FAILURE;

  nsresult rv = image->LockImagePixels(PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIImageToPixbuf> imgToPixbuf =
    do_GetService("@mozilla.org/widget/image-to-gdk-pixbuf;1");

  GdkPixbuf* imagePixbuf = imgToPixbuf->ConvertImageToPixbuf(image);
  if (!imagePixbuf)
    return NS_ERROR_FAILURE;

  rv = image->UnlockImagePixels(PR_FALSE);
  if (NS_FAILED(rv))
    return rv;

  ShowAlert(imagePixbuf);

  g_object_unref(imagePixbuf);

  mLoadedFrame = PR_TRUE;
  return NS_OK;
}

nsresult
nsAlertsIconListener::ShowAlert(GdkPixbuf* aPixbuf)
{
  NotifyNotification* notify = notify_notification_new(mAlertTitle.get(),
                                                       mAlertText.get(),
                                                       NULL, NULL);
  if (!notify)
    return NS_ERROR_OUT_OF_MEMORY;

  if (aPixbuf)
    notify_notification_set_icon_from_pixbuf(notify, aPixbuf);

  notify_notification_set_timeout(notify, NOTIFY_EXPIRES_DEFAULT);
  gboolean result = notify_notification_show(notify, NULL);
  g_object_unref(notify);

  return result ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsAlertsIconListener::StartRequest(const nsAString & aImageUrl)
{
  if (mIconRequest) {
    // Another icon request is already in flight.  Kill it.
    mIconRequest->Cancel(NS_BINDING_ABORTED);
    mIconRequest = nsnull;
  }

  nsCOMPtr<nsIURI> imageUri;
  NS_NewURI(getter_AddRefs(imageUri), aImageUrl);
  if (!imageUri)
    return ShowAlert(NULL);

  nsCOMPtr<imgILoader> il(do_GetService("@mozilla.org/image/loader;1"));
  if (!il)
    return ShowAlert(NULL);

  return il->LoadImage(imageUri, nsnull, nsnull, nsnull, this,
                       nsnull, nsIRequest::LOAD_NORMAL, nsnull, nsnull,
                       getter_AddRefs(mIconRequest));
}

nsresult
nsAlertsIconListener::InitAlertAsync(const nsAString & aImageUrl,
                                     const nsAString & aAlertTitle, 
                                     const nsAString & aAlertText)
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
  }

  mAlertTitle = NS_ConvertUTF16toUTF8(aAlertTitle);
  mAlertText = NS_ConvertUTF16toUTF8(aAlertText);

  return StartRequest(aImageUrl);
}
