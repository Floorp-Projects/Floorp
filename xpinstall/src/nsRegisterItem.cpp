/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

MOZ_DECL_CTOR_COUNTER(nsRegisterItem);

nsRegisterItem:: nsRegisterItem(  nsInstall* inInstall,
                                  nsIFile* chrome,
                                  PRUint32 chromeType )
: nsInstallObject(inInstall), mChrome(chrome), mChromeType(chromeType)
{
    MOZ_COUNT_CTOR(nsRegisterItem);
}


nsRegisterItem::~nsRegisterItem()
{
    MOZ_COUNT_DTOR(nsRegisterItem);
}

#if defined (XP_MAC)
static void SwapSlashColon(char * s)
{
	while (*s)
	{
		if (*s == '/')
			*s++ = ':';
		else if (*s == ':')
			*s++ = '/';
		else
			*s++;
	}
} 
#endif

#if defined(XP_PC) && !defined(XP_OS2)
#include <windows.h>
#endif

static nsresult 
hack_nsIFile2URL(nsIFile* file, char * *aURL)
{
    nsresult rv;
    char* ePath;
    rv = file->GetPath(&ePath);
    if (NS_FAILED(rv)) return rv;
#if defined (XP_PC)
    // Replace \ with / to convert to an url
    char* s = ePath;
    while (*s) {
#ifndef XP_OS2
        // We need to call IsDBCSLeadByte because
        // Japanese windows can have 0x5C in the sencond byte 
        // of a Japanese character, for example 0x8F 0x5C is
        // one Japanese character
        if(::IsDBCSLeadByte(*s) && *(s+1) != nsnull) {
            s++;
        } else 
#endif
            if (*s == '\\')
                *s = '/';
        s++;
    }
#endif
#if defined( XP_MAC )
    // Swap the / and colons to convert to an url
    SwapSlashColon(ePath);
#endif
    // Escape the path with the directory mask
    nsCAutoString tmp(ePath);
    tmp.ReplaceChar(":", '|');
    nsCAutoString escPath("file://");
	escPath += tmp;
//    rv = nsURLEscape(ePath,nsIIOService::url_Directory + nsIIOService::url_Forced, escPath);
//    if (NS_SUCCEEDED(rv)) {
        PRBool dir;
        rv = file->IsDirectory(&dir);
        if (NS_SUCCEEDED(rv) && dir && escPath[escPath.Length() - 1] != '/') {
            // make sure we have a trailing slash
            escPath += "/";
        }
        *aURL = escPath.ToNewCString();
        if (*aURL == nsnull) {
            nsMemory::Free(ePath);
            return NS_ERROR_OUT_OF_MEMORY;
        }
//    }
    nsMemory::Free(ePath);
    return rv;
}


PRInt32 nsRegisterItem::Prepare()
{
    // The chrome must exist
    PRBool exists;
    mChrome->Exists(&exists);
    if (!exists)
        return nsInstall::DOES_NOT_EXIST;

    // if we're registering now we need to make sure we can construct a URL
//    if ( mInstall->GetChromeRegistry() && !(mChromeType & CHROME_DELAYED) )
//    {
        // If not a directory then it's an archive and we need a jar: URL
        nsCOMPtr<nsIURI> pURL;
        PRBool isDir;

        mURL.SetCapacity(200);

        mChrome->IsDirectory(&isDir);
        if (!isDir)
            mURL = "jar:";

        nsXPIDLCString localURL;
        nsresult rv = hack_nsIFile2URL(mChrome, getter_Copies(localURL));
        mURL.Append(localURL);
#if 0
        nsresult rv = NS_NewURI(getter_AddRefs(pURL), "file:");
        if (NS_SUCCEEDED(rv)) 
        {
            nsCOMPtr<nsIFileURL> fileURL = do_QueryInterface(pURL);
            if (fileURL)
            {
                rv = fileURL->SetFile(mChrome);
                if (NS_SUCCEEDED(rv))
                {
                    rv = fileURL->GetSpec(getter_Copies(localURL));
                    mURL.Append(localURL);
                }
            }
        }
#endif

        if (!localURL)
            return nsInstall::UNEXPECTED_ERROR;

        if (!isDir)
            mURL.Append("!/");
//    }

    return nsInstall::SUCCESS;
}

PRInt32 nsRegisterItem::Complete()
{
    nsresult rv;
    PRInt32 result = nsInstall::SUCCESS;
    PRBool  isProfile = mChromeType & CHROME_PROFILE;
    nsIChromeRegistry* reg = mInstall->GetChromeRegistry();

    if ( reg && !(mChromeType & CHROME_DELAYED) )
    {
        if (mChromeType & CHROME_SKIN)
            rv = reg->InstallSkin(mURL.GetBuffer(), isProfile, PR_TRUE);

        if (mChromeType & CHROME_LOCALE)
            rv = reg->InstallLocale(mURL.GetBuffer(), isProfile);

        if (mChromeType & CHROME_CONTENT)
            rv = reg->InstallPackage(mURL.GetBuffer(), isProfile);
    }
    else
    {
        // Unless the script explicitly told us to register later
        // return the REBOOT_NEEDED status. If the script requested
        // it then we assume it knows what it's doing.
        if (!(mChromeType & CHROME_DELAYED))
            result = nsInstall::REBOOT_NEEDED;

        // First find the "bin" diretory of the install
        PRFileDesc* fd = nsnull;
        nsCOMPtr<nsILocalFile> startupFile;
        nsCOMPtr<nsIFile> progDir = nsSoftwareUpdate::GetProgramDirectory();
        if (!progDir)
        {
            // not in the wizard, so ask the directory service where it is
            NS_WITH_SERVICE(nsIProperties, directoryService,
                            NS_DIRECTORY_SERVICE_PROGID, &rv);
            if(NS_SUCCEEDED(rv) && directoryService)
            {
                directoryService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                            NS_GET_IID(nsIFile), getter_AddRefs(progDir));
            }
        }

        if (progDir)
        {
            // we got one... construct a reference to the magic file
            nsCOMPtr<nsIFile> tmp;
            PRBool bExists = PR_FALSE;
            rv = progDir->Clone(getter_AddRefs(tmp));
            if (NS_SUCCEEDED(rv))
                startupFile = do_QueryInterface(tmp, &rv);

            if (NS_SUCCEEDED(rv))
            {
                rv = startupFile->Append("chrome");
                if (NS_SUCCEEDED(rv))
                {
                    rv = startupFile->Exists(&bExists);
                    if (NS_SUCCEEDED(rv) && !bExists)
                        rv = startupFile->Create(nsIFile::DIRECTORY_TYPE, 0755);
                    if (NS_SUCCEEDED(rv))
                    {
                        rv = startupFile->Append("installed-chrome.txt");
                        if (NS_SUCCEEDED(rv))
                        {
                            rv = startupFile->OpenNSPRFileDesc(
                                            PR_CREATE_FILE | PR_WRONLY,
                                            0744,
                                            &fd);
                        }
                    }
                }
            }
        }

        if ( NS_SUCCEEDED(rv) && fd )
        {
            PR_Seek(fd, 0, PR_SEEK_END);
            const char* location = (mChromeType & CHROME_PROFILE) ? "profile" : "install";

//            nsXPIDLCString path;
//            rv = mChrome->GetPath(getter_Copies(path));
            if (NS_SUCCEEDED(rv)/* && path*/)
            {
                PRInt32 written, actual;
                char* installStr = nsnull;
       
                // this looks redundant, but a single registerChrome()
                // call can register all three types.
                if (mChromeType & CHROME_SKIN)
                {
                    installStr = PR_smprintf("skin,%s,url,%s\n",
                                             location, (const char*)mURL);
                    if (installStr)
                    {
                        actual = strlen(installStr);
                        written = PR_Write(fd, installStr, actual);
                        if ( written != actual ) 
                        {
                            result = nsInstall::CHROME_REGISTRY_ERROR;
                        }
                        PR_smprintf_free(installStr);
                    }
                    else 
                        result = nsInstall::OUT_OF_MEMORY;
                }

                if (mChromeType & CHROME_LOCALE)
                {
                    installStr = PR_smprintf("locale,%s,url,%s\n",
                                             location, (const char*)mURL);
                    if (installStr)
                    {
                        actual = strlen(installStr);
                        written = PR_Write(fd, installStr, actual);
                        if ( written != actual ) 
                        {
                            result = nsInstall::CHROME_REGISTRY_ERROR;
                        }
                        PR_smprintf_free(installStr);
                    }
                    else
                        result = nsInstall::OUT_OF_MEMORY;
                }

                if (mChromeType & CHROME_CONTENT)
                {
                    installStr = PR_smprintf("content,%s,url,%s\n",
                                             location, (const char*)mURL);
                    if (installStr)
                    {
                        actual = strlen(installStr);
                        written = PR_Write(fd, installStr, actual);
                        if ( written != actual ) 
                        {
                            result = nsInstall::CHROME_REGISTRY_ERROR;
                        }
                        PR_smprintf_free(installStr);
                    }
                    else
                        result = nsInstall::OUT_OF_MEMORY;
                }
            }
            PR_Close(fd);
        }
    }

    if (NS_FAILED(rv))
        result = nsInstall::CHROME_REGISTRY_ERROR;

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

    buffer[0] = '\0';
    switch (mChromeType & CHROME_ALL)
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
        if (mInstall->GetChromeRegistry() && !(mChromeType & CHROME_DELAYED))
            PR_snprintf(buffer, 1024, rsrcVal, mURL.GetBuffer());
        else
        {
            nsXPIDLCString path;
            nsresult rv = mChrome->GetPath(getter_Copies(path));
            if (NS_SUCCEEDED(rv) && path)
            {
                PR_snprintf(buffer, 1024, rsrcVal, (const char*)path);
            }
        }
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

