/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
                                                    nsIBrowsingProfile::GetIID(),
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

