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
#include "nsIFileChannel.h"
#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsInstallTrigger.h"
#include "nsIChromeRegistry.h"
#include "nsIDirectoryService.h"
#include "nsDirectoryServiceDefs.h"

MOZ_DECL_CTOR_COUNTER(nsRegisterItem)

nsRegisterItem:: nsRegisterItem(  nsInstall* inInstall,
                                  nsIFile* chrome,
                                  PRUint32 chromeType,
                                  const char* path )
: nsInstallObject(inInstall), mChrome(chrome), mChromeType(chromeType), mPath(path)
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
#ifdef XP_MAC
    nsCAutoString escPath("file:///");
#else
    nsCAutoString escPath("file://");
#endif
	escPath += tmp;
//    rv = nsURLEscape(ePath,nsIIOService::url_Directory + nsIIOService::url_Forced, escPath);
//    if (NS_SUCCEEDED(rv)) {
        PRBool dir;
        rv = file->IsDirectory(&dir);
        if (NS_SUCCEEDED(rv) && dir && escPath[escPath.Length() - 1] != '/') {
            // make sure we have a trailing slash
            escPath += "/";
        }
        *aURL = ToNewCString(escPath);
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
    nsresult rv = mChrome->Exists(&exists);
    if (NS_FAILED(rv))
        return nsInstall::UNEXPECTED_ERROR;
    if (!exists)
        return nsInstall::DOES_NOT_EXIST;


    // Are we dealing with a directory (flat chrome) or an archive?
    PRBool isDir;
    rv = mChrome->IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return nsInstall::UNEXPECTED_ERROR;


    // Can we construct a resource: URL or do we need a file: URL instead?
    // find the xpcom directory and see if mChrome is a child
    PRBool isChild = PR_FALSE;
    mProgDir = nsSoftwareUpdate::GetProgramDirectory();
    if (!mProgDir)
    {
        // not in the wizard, so ask the directory service where it is
        nsCOMPtr<nsIProperties> dirService(
                do_GetService(NS_DIRECTORY_SERVICE_CONTRACTID, &rv));
        if(NS_SUCCEEDED(rv))
        {
            NS_ASSERTION(dirService,"directory service lied to us");
            rv = dirService->Get(NS_XPCOM_CURRENT_PROCESS_DIR,
                        NS_GET_IID(nsIFile), getter_AddRefs(mProgDir));
        }
    }
    if (NS_SUCCEEDED(rv))
    {
        NS_ASSERTION(mProgDir,"NS_SUCCESS but no mProgDir");
        rv = mProgDir->Contains(mChrome, PR_TRUE, &isChild);
        if (NS_FAILED(rv)) 
            return nsInstall::UNEXPECTED_ERROR;
    }
    else
        return nsInstall::UNEXPECTED_ERROR;


    // Either way we need the file: URL to the chrome
    nsXPIDLCString localURL;
    rv = GetURLFromIFile( mChrome, getter_Copies(localURL) );
    if (NS_FAILED(rv))
        return nsInstall::UNEXPECTED_ERROR;

    // see what kind of URL we have to construct
    if (!isChild)
    {
        // Not relative so use the file:// URL we got above
        PRInt32 urlLen = nsCRT::strlen(localURL) + mPath.Length();

        if (isDir)
        {
            // "flat" chrome, urlLen is suffient
            mURL.SetCapacity( urlLen );
        }
        else
        {
            // archive, add room for jar: syntax (over by one, but harmless)
            mURL.SetCapacity( urlLen + sizeof("jar:") + sizeof('!') );
            mURL = "jar:";
        }
        mURL.Append(localURL);
    }
    else
    {
        // we can construct a resource: URL to chrome in a subdir
        nsXPIDLCString binURL;
        rv = GetURLFromIFile( mProgDir, getter_Copies(binURL) );
        if (NS_FAILED(rv))
            return nsInstall::UNEXPECTED_ERROR;

        PRInt32 binLen = nsCRT::strlen(binURL);
        const char *subURL = localURL + binLen;
        PRInt32 padding = sizeof("resource:/") + sizeof("jar:!/");

        mURL.SetCapacity( nsCRT::strlen(subURL) + mPath.Length() + padding );

        if (!isDir)
            mURL = "jar:";

        mURL.Append("resource:/");
        mURL.Append(subURL);
    }

        
    if (!isDir)
    {
        // need jar: URL closing bang-slash
        mURL.Append("!/");
    }
    else
    {
        // Necko should already slash-terminate directory file:// URLs
        NS_ASSERTION(mURL[mURL.Length()-1] == '/', "Necko changed the rules");
    }

    // add on "extra" subpath to new content.rdf
    mURL.Append(mPath);

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
        // We can register right away
        if (mChromeType & CHROME_SKIN)
            rv = reg->InstallSkin(mURL.get(), isProfile, PR_TRUE);

        if (NS_SUCCEEDED(rv) && (mChromeType & CHROME_LOCALE))
            rv = reg->InstallLocale(mURL.get(), isProfile);

        if (NS_SUCCEEDED(rv) && (mChromeType & CHROME_CONTENT))
            rv = reg->InstallPackage(mURL.get(), isProfile);
    }
    else
    {
        // Either script asked for delayed chrome or we can't find
        // the chrome registry to do it now.
        NS_ASSERTION(mProgDir, "this.Prepare() failed to set mProgDir");

        // construct a reference to the magic file
        PRFileDesc* fd = nsnull;
        nsCOMPtr<nsIFile> tmp;
        PRBool bExists = PR_FALSE;
        rv = mProgDir->Clone(getter_AddRefs(tmp));
        if (NS_SUCCEEDED(rv))
        {
            nsCOMPtr<nsILocalFile> startupFile( do_QueryInterface(tmp, &rv) );

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

            if (NS_SUCCEEDED(rv)/* && path*/)
            {
                PRInt32 written, actual;
                char* installStr = nsnull;
       
                // this looks redundant, but a single registerChrome()
                // call can register all three types.
                if (mChromeType & CHROME_SKIN)
                {
                    installStr = PR_smprintf("skin,%s,url,%s\n",
                                             location, mURL.get());
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
                                             location, mURL.get());
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
                                             location, mURL.get());
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
        else
        {
            result = nsInstall::CHROME_REGISTRY_ERROR;
        }
    }

    if (NS_FAILED(rv))
        result = nsInstall::CHROME_REGISTRY_ERROR;

    return result;
}

void nsRegisterItem::Abort()
{
    // nothing to undo
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
                    NS_LITERAL_STRING("RegSkin"));
        break;
    case CHROME_LOCALE:
        rsrcVal = mInstall->GetResourcedString(
                    NS_LITERAL_STRING("RegLocale"));
        break;
    case CHROME_CONTENT:
        rsrcVal = mInstall->GetResourcedString(
                    NS_LITERAL_STRING("RegContent"));
        break;
    default:
        rsrcVal = mInstall->GetResourcedString(
                    NS_LITERAL_STRING("RegPackage"));
        break;
    }

    if (rsrcVal)
    {
        PR_snprintf(buffer, 1024, rsrcVal, mURL.get());
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

nsresult
nsRegisterItem::GetURLFromIFile(nsIFile* aFile, char** aOutURL)
{
    if (!aFile || !aOutURL)
    {
        NS_WARNING("bogus arg passed to nsRegisterItem::GetURLFromIFile()");
        return NS_ERROR_NULL_POINTER;
    }
    *aOutURL = nsnull;

    // try to use Necko to create the URL; if that fails (as
    // it will for the install wizards which don't have Necko)
    // then use warren's local hack.

    nsCOMPtr<nsIURI> pURL;
    nsresult rv = NS_NewURI(getter_AddRefs(pURL), "file:");
    if (NS_SUCCEEDED(rv)) 
    {
        nsCOMPtr<nsIFileURL> fileURL( do_QueryInterface(pURL) );
        if (fileURL)
        {
            rv = fileURL->SetFile(aFile);
            if (NS_SUCCEEDED(rv))
            {
                rv = fileURL->GetSpec(aOutURL);
            }
        }
    }

    if ( NS_FAILED(rv))
    {
        // Necko couldn't do it (wasn't present?), try the hack
        rv = hack_nsIFile2URL(aFile, aOutURL);
    }

    return rv;
}
