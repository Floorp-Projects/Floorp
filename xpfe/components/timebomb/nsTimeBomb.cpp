/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTimeBomb.h"
#include "nsIServiceManager.h"
#include "nspr.h"
#include "plstr.h"

#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIIOService.h"
#include "nsNetUtil.h"

//Interfaces Needed
#include "nsIXULWindow.h"
#include "nsIWebBrowserChrome.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);


static nsresult DisplayURI(const char *urlStr, PRBool block)
{
    
    nsresult rv;
    nsCOMPtr<nsIURI> URL;
    
    rv = NS_NewURI(getter_AddRefs(URL), (const char *)urlStr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIAppShellService> appShell = 
             do_GetService(kAppShellServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIXULWindow>  window;
    rv = appShell->CreateTopLevelWindow(nsnull, 
                                URL,
                                PR_TRUE, 
                                PR_TRUE,
                                nsIWebBrowserChrome::CHROME_ALL, 
                                nsIAppShellService::SIZE_TO_CONTENT, // width 
                                nsIAppShellService::SIZE_TO_CONTENT, // height
                                getter_AddRefs(window));

    if (NS_FAILED(rv)) return rv;

    /*
     * Start up the main event loop...
     */
    if (block)
        rv = appShell->Run();
    
    return rv;
}

nsTimeBomb::nsTimeBomb()
{
    NS_INIT_REFCNT();
}

nsTimeBomb::~nsTimeBomb()
{
}

NS_IMPL_ISUPPORTS1(nsTimeBomb, nsITimeBomb)

NS_IMETHODIMP
nsTimeBomb::Init()
{
    nsresult rv;

    rv = nsServiceManager::GetService(kPrefCID, NS_GET_IID(nsIPref), getter_AddRefs(mPrefs));
    NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get prefs");
    if (NS_FAILED(rv))
        return rv;

    PRTime time = LL_Zero();
    rv = GetFirstLaunch(&time);
    if (NS_FAILED(rv))
    {
        time = PR_Now();
        char buffer[30];
        PR_snprintf(buffer, 30, "%lld", time);
        mPrefs->SetCharPref("timebomb.first_launch_time", buffer);
        rv = NS_OK;
    }
    return rv;
}

NS_IMETHODIMP
nsTimeBomb::CheckWithUI(PRBool *expired)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;

    *expired = PR_FALSE;

    PRBool val;
    nsresult rv = GetEnabled(&val);

    if (NS_FAILED(rv) || !val)
    {
        // was not set or not enabled.  
        // no problems.  just return okay.
        return NS_OK;
    }

    rv = GetExpired(&val);
    
    if (NS_SUCCEEDED(rv) && val)
    {
#ifdef DEBUG
        printf("********  Expired version  ********\n");
#endif
        DisplayURI("chrome://communicator/content/timebomb/expireText.xul", PR_FALSE);
        *expired = PR_TRUE;
        return NS_OK;
    }

    rv = GetWarned(&val);
    
    if (NS_SUCCEEDED(rv) && val)
    {
#ifdef DEBUG
        printf("********  ABOUT TO EXPIRE  ********\n");
#endif
        DisplayURI("chrome://communicator/content/timebomb/warnText.xul", PR_FALSE);
    }

    return NS_OK;
    
}

NS_IMETHODIMP
nsTimeBomb::LoadUpdateURL()
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;

    char* url;
    nsresult rv = GetTimebombURL(&url);
    if (NS_FAILED(rv)) return rv;
    rv = DisplayURI(url, PR_FALSE);
    nsMemory::Free(url);
    return rv;
}

NS_IMETHODIMP
nsTimeBomb::GetExpired(PRBool *expired)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;

    *expired = PR_FALSE;

    PRTime bombTime    = LL_Zero();
    PRTime currentTime = PR_Now();
    
    // check absolute expiration;

    nsresult rv = GetExpirationTime(&bombTime);

    if (NS_FAILED(rv)) return NS_OK;
    
    if (LL_CMP(bombTime, <, currentTime)) 
    {    
        *expired = PR_TRUE;       
        return NS_OK;
    }
    
    // try relative expiration;

    PRTime offsetTime   = LL_Zero();
    PRTime offset       = LL_Zero();
    
    rv = GetBuildTime(&bombTime);
    if (NS_FAILED(rv)) return NS_OK;
    rv = GetExpirationOffset(&offset);
    if (NS_FAILED(rv)) return NS_OK;

    LL_ADD(offsetTime, bombTime, offset);
    if (LL_CMP(offsetTime, <, currentTime)) 
    {    
        *expired = PR_FALSE;
        return NS_OK;
    }

    rv = GetFirstLaunch(&bombTime);
    if (NS_FAILED(rv)) return NS_OK;
    
    LL_ADD(offsetTime, bombTime, offset);
    if (LL_CMP(offsetTime, <, currentTime)) 
    {    
        *expired = PR_FALSE;
        return NS_OK;
    }


    return NS_OK;
}

NS_IMETHODIMP
nsTimeBomb::GetWarned(PRBool *warn)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    *warn = PR_FALSE;

    PRTime bombTime    = LL_Zero();
    PRTime currentTime = PR_Now();
    
    // check absolute expiration;

    nsresult rv = GetWarningTime(&bombTime);

    if (NS_FAILED(rv)) return NS_OK;
    
    if (LL_CMP(bombTime, <, currentTime)) 
    {    
        *warn = PR_TRUE;       
        return NS_OK;
    }
    
    // try relative expiration;

    PRTime offsetTime   = LL_Zero();
    PRTime offset       = LL_Zero();
    
    rv = GetBuildTime(&bombTime);
    if (NS_FAILED(rv)) return NS_OK;
    rv = GetWarningOffset(&offset);
    if (NS_FAILED(rv)) return NS_OK;

    LL_ADD(offsetTime, bombTime, offset);
    if (LL_CMP(offsetTime, <, currentTime)) 
    {    
        *warn = PR_FALSE;
        return NS_OK;
    }

    rv = GetFirstLaunch(&bombTime);
    if (NS_FAILED(rv)) return NS_OK;
    
    LL_ADD(offsetTime, bombTime, offset);
    if (LL_CMP(offsetTime, <, currentTime)) 
    {    
        *warn = PR_FALSE;
        return NS_OK;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsTimeBomb::GetEnabled(PRBool *enabled)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return mPrefs->GetBoolPref("timebomb.enabled",enabled);
}


NS_IMETHODIMP
nsTimeBomb::GetExpirationTime(PRTime *time)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return GetInt64ForPref("timebomb.expiration_time", time);
}


NS_IMETHODIMP
nsTimeBomb::GetWarningTime(PRTime *time)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return GetInt64ForPref("timebomb.warning_time", time);
}


NS_IMETHODIMP
nsTimeBomb::GetBuildTime(PRTime *time)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return GetInt64ForPref("timebomb.build_time", time);
}

NS_IMETHODIMP
nsTimeBomb::GetFirstLaunch(PRTime *time)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return GetInt64ForPref("timebomb.first_launch_time", time);
}


NS_IMETHODIMP
nsTimeBomb::GetWarningOffset(PRInt64 *offset)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    return GetInt64ForPref("timebomb.warning_offset", offset);
}

NS_IMETHODIMP
nsTimeBomb::GetExpirationOffset(PRInt64 *offset)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;

    return GetInt64ForPref("timebomb.expiration_offset", offset);
}



NS_IMETHODIMP
nsTimeBomb::GetTimebombURL(char* *url)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    char* string;
    nsresult rv = mPrefs->CopyCharPref("timebomb.timebombURL", &string);
    if (NS_SUCCEEDED(rv))
    {
        *url = (char*)nsMemory::Clone(string, (strlen(string)+1)*sizeof(char));
        
        PL_strfree(string);

        if(*url == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        return NS_OK;
    }
   
    string = "http://www.mozilla.org/projects/seamonkey/";
    *url = (char*)nsMemory::Clone(string, (strlen(string)+1)*sizeof(char));
    
    if(!*url)
        return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}



nsresult 
nsTimeBomb::GetInt64ForPref(const char* pref, PRInt64* time)
{
    if (!mPrefs) return NS_ERROR_NULL_POINTER;
    
    char* string;
    nsresult rv = mPrefs->CopyCharPref(pref, &string);
    if (NS_SUCCEEDED(rv))
    {
        PR_sscanf(string, "%lld", time);
        PL_strfree(string);
    }
    return rv;
}
