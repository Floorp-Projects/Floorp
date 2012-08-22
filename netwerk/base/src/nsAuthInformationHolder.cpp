/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAuthInformationHolder.h"

NS_IMPL_ISUPPORTS1(nsAuthInformationHolder, nsIAuthInformation)

NS_IMETHODIMP
nsAuthInformationHolder::GetFlags(uint32_t* aFlags)
{
    *aFlags = mFlags;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::GetRealm(nsAString& aRealm)
{
    aRealm = mRealm;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::GetAuthenticationScheme(nsACString& aScheme)
{
    aScheme = mAuthType;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::GetUsername(nsAString& aUserName)
{
    aUserName = mUser;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::SetUsername(const nsAString& aUserName)
{
    if (!(mFlags & ONLY_PASSWORD))
        mUser = aUserName;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::GetPassword(nsAString& aPassword)
{
    aPassword = mPassword;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::SetPassword(const nsAString& aPassword)
{
    mPassword = aPassword;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::GetDomain(nsAString& aDomain)
{
    aDomain = mDomain;
    return NS_OK;
}

NS_IMETHODIMP
nsAuthInformationHolder::SetDomain(const nsAString& aDomain)
{
    if (mFlags & NEED_DOMAIN)
        mDomain = aDomain;
    return NS_OK;
}


