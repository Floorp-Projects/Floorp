/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications.  Portions created by Netscape Communications are
 * Copyright (C) 2001 by Netscape Communications.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com> (original author)
 */

#include "nsResURL.h"
#include "nsResProtocolHandler.h"
#include "nsNetCID.h"
#include "nsILocalFile.h"
#include "nsXPIDLString.h"

static NS_DEFINE_CID(kStandardURLCID, NS_STANDARDURL_CID);

//-----------------------------------------------------------------------------
// nsResURL <public>
//-----------------------------------------------------------------------------

nsresult
nsResURL::Init(nsIURI *uri)
{
    nsresult rv;
    mStdURL = do_QueryInterface(uri, &rv);
    return rv;
}

//-----------------------------------------------------------------------------
// nsResURL::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ISUPPORTS3(nsResURL,
                   nsIURI,
                   nsIURL,
                   nsIFileURL)

//-----------------------------------------------------------------------------
// nsResURL::nsIFileURL
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsResURL::GetFile(nsIFile **result)
{
    nsresult rv;

    NS_ENSURE_TRUE(nsResProtocolHandler::get(), NS_ERROR_NOT_AVAILABLE);
    NS_ENSURE_TRUE(mStdURL, NS_ERROR_NOT_INITIALIZED);

    nsXPIDLCString spec;
    rv = nsResProtocolHandler::get()->ResolveURI(mStdURL, getter_Copies(spec));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsILocalFile> localFile =
            do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return rv;

    rv = localFile->SetURL(spec);
    if (NS_FAILED(rv)) return rv;

    return CallQueryInterface(localFile, result);
}

NS_IMETHODIMP
nsResURL::SetFile(nsIFile *file)
{
    NS_NOTREACHED("nsResURL::SetFile not implemented\n");
    return NS_ERROR_NOT_IMPLEMENTED;
}
