/* This Source Code Form is subject to the terms of the Mozilla Pub
 * License, v. 2.0. If a copy of the MPL was not distributed with t
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/AlertNotification.h"

#include "nsAlertsUtils.h"

namespace mozilla {

NS_IMPL_CYCLE_COLLECTION(AlertNotification, mPrincipal)
NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(AlertNotification)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIAlertNotification)
  NS_INTERFACE_MAP_ENTRY(nsIAlertNotification)
NS_INTERFACE_MAP_END
NS_IMPL_CYCLE_COLLECTING_ADDREF(AlertNotification)
NS_IMPL_CYCLE_COLLECTING_RELEASE(AlertNotification)

AlertNotification::AlertNotification()
  : mTextClickable(false)
  , mPrincipal(nullptr)
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
                        bool aInPrivateBrowsing)
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

} // namespace mozilla
