/* This Source Code Form is subject to the terms of the Mozilla Pub
 * License, v. 2.0. If a copy of the MPL was not distributed with t
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AlertNotification.h"

#include "imgIContainer.h"
#include "imgINotificationObserver.h"
#include "imgIRequest.h"
#include "imgLoader.h"
#include "nsAlertsUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsContentUtils.h"
#include "nsNetUtil.h"
#include "nsServiceManagerUtils.h"

#include "mozilla/Unused.h"

namespace mozilla {

NS_IMPL_ISUPPORTS(AlertNotification, nsIAlertNotification)

AlertNotification::AlertNotification()
  : mTextClickable(false)
  , mInPrivateBrowsing(false)
{}

AlertNotification::~AlertNotification()
{}

NS_IMETHODIMP
AlertNotification::Init(const nsAString& aName, const nsAString& aImageURL,
                        const nsAString& aTitle, const nsAString& aText,
                        bool aTextClickable, const nsAString& aCookie,
                        const nsAString& aDir, const nsAString& aLang,
                        const nsAString& aData, nsIPrincipal* aPrincipal,
                        bool aInPrivateBrowsing, bool aRequireInteraction)
{
  mName = aName;
  mImageURL = aImageURL;
  mTitle = aTitle;
  mText = aText;
  mTextClickable = aTextClickable;
  mCookie = aCookie;
  mDir = aDir;
  mLang = aLang;
  mData = aData;
  mPrincipal = aPrincipal;
  mInPrivateBrowsing = aInPrivateBrowsing;
  mRequireInteraction = aRequireInteraction;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetName(nsAString& aName)
{
  aName = mName;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetImageURL(nsAString& aImageURL)
{
  aImageURL = mImageURL;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetTitle(nsAString& aTitle)
{
  aTitle = mTitle;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetText(nsAString& aText)
{
  aText = mText;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetTextClickable(bool* aTextClickable)
{
  *aTextClickable = mTextClickable;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetCookie(nsAString& aCookie)
{
  aCookie = mCookie;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetDir(nsAString& aDir)
{
  aDir = mDir;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetLang(nsAString& aLang)
{
  aLang = mLang;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetRequireInteraction(bool* aRequireInteraction)
{
  *aRequireInteraction = mRequireInteraction;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetData(nsAString& aData)
{
  aData = mData;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetPrincipal(nsIPrincipal** aPrincipal)
{
  NS_IF_ADDREF(*aPrincipal = mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetURI(nsIURI** aURI)
{
  if (!nsAlertsUtils::IsActionablePrincipal(mPrincipal)) {
    *aURI = nullptr;
    return NS_OK;
  }
  return mPrincipal->GetURI(aURI);
}

NS_IMETHODIMP
AlertNotification::GetInPrivateBrowsing(bool* aInPrivateBrowsing)
{
  *aInPrivateBrowsing = mInPrivateBrowsing;
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetActionable(bool* aActionable)
{
  *aActionable = nsAlertsUtils::IsActionablePrincipal(mPrincipal);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::GetSource(nsAString& aSource)
{
  nsAlertsUtils::GetSourceHostPort(mPrincipal, aSource);
  return NS_OK;
}

NS_IMETHODIMP
AlertNotification::LoadImage(uint32_t aTimeout,
                             nsIAlertNotificationImageListener* aListener,
                             nsISupports* aUserData,
                             nsICancelable** aRequest)
{
  NS_ENSURE_ARG(aListener);
  NS_ENSURE_ARG_POINTER(aRequest);
  *aRequest = nullptr;

  // Exit early if this alert doesn't have an image.
  if (mImageURL.IsEmpty()) {
    return aListener->OnImageMissing(aUserData);
  }
  nsCOMPtr<nsIURI> imageURI;
  NS_NewURI(getter_AddRefs(imageURI), mImageURL);
  if (!imageURI) {
    return aListener->OnImageMissing(aUserData);
  }

  RefPtr<AlertImageRequest> request = new AlertImageRequest(imageURI, mPrincipal,
                                                            mInPrivateBrowsing,
                                                            aTimeout, aListener,
                                                            aUserData);
  nsresult rv = request->Start();
  request.forget(aRequest);
  return rv;
}

NS_IMPL_CYCLE_COLLECTION(AlertImageRequest, mURI, mPrincipal, mListener,
                         mUserData)

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AlertImageRequest)
  NS_INTERFACE_MAP_ENTRY(imgINotificationObserver)
  NS_INTERFACE_MAP_ENTRY(nsICancelable)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsINamed)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, imgINotificationObserver)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(AlertImageRequest)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AlertImageRequest)

AlertImageRequest::AlertImageRequest(nsIURI* aURI, nsIPrincipal* aPrincipal,
                                     bool aInPrivateBrowsing, uint32_t aTimeout,
                                     nsIAlertNotificationImageListener* aListener,
                                     nsISupports* aUserData)
  : mURI(aURI)
  , mPrincipal(aPrincipal)
  , mInPrivateBrowsing(aInPrivateBrowsing)
  , mTimeout(aTimeout)
  , mListener(aListener)
  , mUserData(aUserData)
{}

AlertImageRequest::~AlertImageRequest()
{
  if (mRequest) {
    mRequest->CancelAndForgetObserver(NS_BINDING_ABORTED);
  }
}

NS_IMETHODIMP
AlertImageRequest::Notify(imgIRequest* aRequest, int32_t aType,
                          const nsIntRect* aData)
{
  MOZ_ASSERT(aRequest == mRequest);

  uint32_t imgStatus = imgIRequest::STATUS_ERROR;
  nsresult rv = aRequest->GetImageStatus(&imgStatus);
  if (NS_WARN_IF(NS_FAILED(rv)) ||
      (imgStatus & imgIRequest::STATUS_ERROR)) {
    return NotifyMissing();
  }

  // If the image is already decoded, `FRAME_COMPLETE` will fire before
  // `LOAD_COMPLETE`, so we can notify the listener immediately. Otherwise,
  // we'll need to request a decode when `LOAD_COMPLETE` fires, and wait
  // for the first frame.

  if (aType == imgINotificationObserver::LOAD_COMPLETE) {
    if (!(imgStatus & imgIRequest::STATUS_FRAME_COMPLETE)) {
      nsCOMPtr<imgIContainer> image;
      rv = aRequest->GetImage(getter_AddRefs(image));
      if (NS_WARN_IF(NS_FAILED(rv) || !image)) {
        return NotifyMissing();
      }

      // Ask the image to decode at its intrinsic size.
      int32_t width = 0, height = 0;
      image->GetWidth(&width);
      image->GetHeight(&height);
      image->RequestDecodeForSize(gfx::IntSize(width, height), imgIContainer::FLAG_NONE);
    }
    return NS_OK;
  }

  if (aType == imgINotificationObserver::FRAME_COMPLETE) {
    return NotifyComplete();
  }

  return NS_OK;
}

NS_IMETHODIMP
AlertImageRequest::Notify(nsITimer* aTimer)
{
  MOZ_ASSERT(aTimer == mTimer);
  return NotifyMissing();
}

NS_IMETHODIMP
AlertImageRequest::GetName(nsACString& aName)
{
  aName.AssignLiteral("AlertImageRequest");
  return NS_OK;
}

NS_IMETHODIMP
AlertImageRequest::Cancel(nsresult aReason)
{
  if (mRequest) {
    mRequest->Cancel(aReason);
  }
  // We call `NotifyMissing` here because we won't receive a `LOAD_COMPLETE`
  // notification if we cancel the request before it loads (bug 1233086,
  // comment 33). Once that's fixed, `nsIAlertNotification::loadImage` could
  // return the underlying `imgIRequest` instead of the wrapper.
  return NotifyMissing();
}

nsresult
AlertImageRequest::Start()
{
  // Keep the request alive until we notify the image listener.
  NS_ADDREF_THIS();

  nsresult rv;
  if (mTimeout > 0) {
    mTimer = do_CreateInstance(NS_TIMER_CONTRACTID);
    if (NS_WARN_IF(!mTimer)) {
      return NotifyMissing();
    }
    rv = mTimer->InitWithCallback(this, mTimeout,
                                  nsITimer::TYPE_ONE_SHOT);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return NotifyMissing();
    }
  }

  // Begin loading the image.
  imgLoader* il = imgLoader::NormalLoader();
  if (!il) {
    return NotifyMissing();
  }

  // Bug 1237405: `LOAD_ANONYMOUS` disables cookies, but we want to use a
  // temporary cookie jar instead. We should also use
  // `imgLoader::PrivateBrowsingLoader()` instead of the normal loader.
  // Unfortunately, the PB loader checks the load group, and asserts if its
  // load context's PB flag isn't set. The fix is to pass the load group to
  // `nsIAlertNotification::loadImage`.
  int32_t loadFlags = mInPrivateBrowsing ? nsIRequest::LOAD_ANONYMOUS :
                      nsIRequest::LOAD_NORMAL;

  rv = il->LoadImageXPCOM(mURI, nullptr, nullptr,
                          NS_LITERAL_STRING("default"), mPrincipal, nullptr,
                          this, nullptr, loadFlags, nullptr,
                          nsIContentPolicy::TYPE_INTERNAL_IMAGE,
                          getter_AddRefs(mRequest));
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return NotifyMissing();
  }

  return NS_OK;
}

nsresult
AlertImageRequest::NotifyMissing()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (nsCOMPtr<nsIAlertNotificationImageListener> listener = mListener.forget()) {
    nsresult rv = listener->OnImageMissing(mUserData);
    NS_RELEASE_THIS();
    return rv;
  }
  return NS_OK;
}

nsresult
AlertImageRequest::NotifyComplete()
{
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
  if (nsCOMPtr<nsIAlertNotificationImageListener> listener = mListener.forget()) {
    nsresult rv = listener->OnImageReady(mUserData, mRequest);
    NS_RELEASE_THIS();
    return rv;
  }
  return NS_OK;
}

} // namespace mozilla
