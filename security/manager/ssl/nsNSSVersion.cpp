/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsNSSVersion.h"
#include "nsString.h"
#include "prinit.h"
#include "nss.h"
#include "nssutil.h"
#include "ssl.h"
#include "smime.h"

NS_IMPL_ISUPPORTS(nsNSSVersion, nsINSSVersion)

nsNSSVersion::nsNSSVersion()
{
}

nsNSSVersion::~nsNSSVersion()
{
}

NS_IMETHODIMP
nsNSSVersion::GetNSPR_Version(nsAString & v)
{
    CopyUTF8toUTF16(PR_GetVersion(), v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSS_Version(nsAString & v)
{
    CopyUTF8toUTF16(NSS_GetVersion(), v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSUTIL_Version(nsAString & v)
{
    CopyUTF8toUTF16(NSSUTIL_GetVersion(), v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSSSL_Version(nsAString & v)
{
    CopyUTF8toUTF16(NSSSSL_GetVersion(), v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSSMIME_Version(nsAString & v)
{
    CopyUTF8toUTF16(NSSSMIME_GetVersion(), v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSPR_MinVersion(nsAString & v)
{
    CopyUTF8toUTF16(PR_VERSION, v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSS_MinVersion(nsAString & v)
{
    CopyUTF8toUTF16(NSS_VERSION, v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSUTIL_MinVersion(nsAString & v)
{
    CopyUTF8toUTF16(NSSUTIL_VERSION, v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSSSL_MinVersion(nsAString & v)
{
    CopyUTF8toUTF16(NSS_VERSION, v);
    return NS_OK;
}

NS_IMETHODIMP
nsNSSVersion::GetNSSSMIME_MinVersion(nsAString & v)
{
    CopyUTF8toUTF16(NSS_VERSION, v);
    return NS_OK;
}
