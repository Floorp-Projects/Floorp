/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

/*

  This is the hook code which puts together the browsing profile
  cookie.

 */

#include "nsIServiceManager.h"
#include "nsIBrowsingProfile.h"
#include "plstr.h"

static NS_DEFINE_CID(kBrowsingProfileCID, NS_BROWSINGPROFILE_CID);

extern "C"
PRBool BP_GetProfile(char** aProfileStringCopy)
{
    nsresult rv;
    nsIBrowsingProfile* profile;
    if (NS_FAILED(rv = nsServiceManager::GetService(kBrowsingProfileCID,
                                                    NS_GET_IID(nsIBrowsingProfile),
                                                    (nsISupports**) &profile)))
        // browsing profile not available
        return PR_FALSE;

    char buf[kBrowsingProfileCookieSize];
    if (NS_FAILED(rv = profile->GetCookieString(buf))) {
        NS_ERROR("unable to get cookie string");
        goto done;
    }

    if (! (*aProfileStringCopy = PL_strdup(buf)))
        rv = NS_ERROR_OUT_OF_MEMORY;

done:
    nsServiceManager::ReleaseService(kBrowsingProfileCID, profile);
    return NS_SUCCEEDED(rv);
}

