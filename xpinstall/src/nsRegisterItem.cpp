/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "prprf.h"
#include "nsRegisterItem.h"
#include "nsInstallResources.h"
#include "nsNetUtil.h"
#include "nsXPIDLString.h"
#include "nsInstallTrigger.h"
#include "nsIChromeRegistry.h"

MOZ_DECL_CTOR_COUNTER(nsRegisterItem);

nsRegisterItem:: nsRegisterItem(  nsInstall* inInstall,
                                  nsIFile* chrome,
                                  PRUint32 chromeType )
: nsInstallObject(inInstall), mChromeType(chromeType)
{
    MOZ_COUNT_CTOR(nsRegisterItem);

    // construct an URL from the chrome argument. If it's not
    // a directory then it's an archive and we need a jar: URL
    nsCOMPtr<nsIURI> pURL;
    PRBool isDir;

    chrome->IsDirectory(&isDir);
    mURL.SetCapacity(200);

    if (!isDir)
        mURL.Append("jar:");

    nsresult rv = NS_NewURI(getter_AddRefs(pURL), "file:");
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(pURL);
        if (fileURL)
        {
            rv = fileURL->SetFile(chrome);
            if (NS_SUCCEEDED(rv))
            {
                nsXPIDLCString localURL;
                rv = fileURL->GetSpec(getter_Copies(localURL));
                int len = nsCRT::strlen(localURL);
                mURL.Append(localURL);
            }
        }
    }

    if (!isDir)
        mURL.Append("!/");
    else
        mURL.Append("/");
}


nsRegisterItem::~nsRegisterItem()
{
    MOZ_COUNT_DTOR(nsRegisterItem);
}


PRInt32 nsRegisterItem::Prepare()
{
    return nsInstall::SUCCESS;
}

PRInt32 nsRegisterItem::Complete()
{
    nsresult rv;
    PRInt32 result = nsInstall::SUCCESS;
    nsIChromeRegistry* reg = mInstall->GetChromeRegistry();

    if (reg)
    {
        if (mChromeType & CHROME_SKIN)
            rv = reg->InstallSkin(mURL.GetBuffer(), PR_FALSE);

        if (mChromeType & CHROME_LOCALE)
            rv = reg->InstallLocale(mURL.GetBuffer(), PR_FALSE);

        if (mChromeType & CHROME_CONTENT)
            rv = reg->InstallPackage(mURL.GetBuffer(), PR_FALSE);
    }
    else
    {
        // Couldn't get chrome registry, probably during the wizard.
        // XXX: Must save this for later processing somehow.
        result = nsInstall::REBOOT_NEEDED;
    }

    if (NS_FAILED(rv))
        result = nsInstall::UNEXPECTED_ERROR;

    return result;
}

void nsRegisterItem::Abort()
{
}

char* nsRegisterItem::toString()
{
    char* buffer = new char[1024];
    char* rsrcVal = nsnull;

    if (buffer == nsnull || !mInstall)
        return nsnull;

    switch (mChromeType)
    {
    case CHROME_SKIN:
        rsrcVal = mInstall->GetResourcedString(
                    NS_ConvertASCIItoUCS2("RegSkin"));
        break;
    case CHROME_LOCALE:
        rsrcVal = mInstall->GetResourcedString(
                    NS_ConvertASCIItoUCS2("RegLocale"));
        break;
    case CHROME_CONTENT:
        rsrcVal = mInstall->GetResourcedString(
                    NS_ConvertASCIItoUCS2("RegContent"));
        break;
    default:
        rsrcVal = mInstall->GetResourcedString(
                    NS_ConvertASCIItoUCS2("RegPackage"));
        break;
    }

    if (rsrcVal)
    {
        PR_snprintf(buffer, 1024, mURL.GetBuffer());
        nsCRT::free(rsrcVal);
    }

    return buffer;
}


PRBool
nsRegisterItem::CanUninstall()
{
    return PR_FALSE;
}

PRBool
nsRegisterItem::RegisterPackageNode()
{
    return PR_FALSE;
}

