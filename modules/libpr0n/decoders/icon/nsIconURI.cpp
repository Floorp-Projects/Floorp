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
 *
 * Contributors:
 *  Scott MacGregor <mscott@netscape.com>
 */

#include "nsIconURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
#define DEFAULT_IMAGE_SIZE          16
 
////////////////////////////////////////////////////////////////////////////////
 
nsMozIconURI::nsMozIconURI()
{
  NS_INIT_REFCNT();
  mSize = DEFAULT_IMAGE_SIZE;
}
 
nsMozIconURI::~nsMozIconURI()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMozIconURI, nsIMozIconURI, nsIURI)

#define NS_MOZICON_SCHEME           "moz-icon:"
#define NS_MOZ_ICON_DELIMITER        '?'


nsresult
nsMozIconURI::FormatSpec(char* *result)
{
  nsresult rv;
  nsXPIDLCString fileIconSpec;
  rv = mFileIcon->GetSpec(getter_Copies(fileIconSpec));
  if (NS_FAILED(rv)) return rv;

  nsCString spec(NS_MOZICON_SCHEME);
  spec += fileIconSpec;
  spec += NS_MOZ_ICON_DELIMITER;
  spec += "size=";
  spec.AppendInt(mSize);
  if (!mContentType.IsEmpty())
  {
    spec += "&contentType=";
    spec += mContentType.get();
  }
  
  *result = nsCRT::strdup(spec);
  return *result ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

////////////////////////////////////////////////////////////////////////////////
// nsURI methods:

NS_IMETHODIMP
nsMozIconURI::GetSpec(char* *aSpec)
{
  return FormatSpec(aSpec);
}

NS_IMETHODIMP
nsMozIconURI::SetSpec(const char * aSpec)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService (do_GetService(kIOServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRUint32 startPos, endPos;
  rv = ioService->ExtractScheme(aSpec, &startPos, &endPos, nsnull);
  NS_ENSURE_SUCCESS(rv, rv);

  if (nsCRT::strncmp("moz-icon", &aSpec[startPos], endPos - startPos - 1) != 0)
    return NS_ERROR_MALFORMED_URI;

  nsCAutoString mozIconPath(aSpec);
  nsCAutoString filePath;
  PRInt32 pos = mozIconPath.FindChar(NS_MOZ_ICON_DELIMITER);

  if (pos == -1) // no size or content type specified
  {
    mozIconPath.Right(filePath, mozIconPath.Length() - endPos);
  }
  else
  {
    mozIconPath.Mid(filePath, endPos, pos);
    // fill in any size and content type values...

  }

  rv = ioService->NewURI(filePath, nsnull, getter_AddRefs(mFileIcon));
  if (NS_FAILED(rv)) return rv;
  return rv;
}

NS_IMETHODIMP
nsMozIconURI::GetPrePath(char* *prePath)
{
  *prePath = nsCRT::strdup(NS_MOZICON_SCHEME);
  return *prePath ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMozIconURI::SetPrePath(const char* prePath)
{
  NS_NOTREACHED("nsMozIconURI::SetPrePath");
  return NS_ERROR_NOT_IMPLEMENTED; 
}

NS_IMETHODIMP
nsMozIconURI::GetScheme(char * *aScheme)
{
  *aScheme = nsCRT::strdup("moz-icon");
  return *aScheme ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsMozIconURI::SetScheme(const char * aScheme)
{
  // doesn't make sense to set the scheme of a moz-icon URL
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetUsername(char * *aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetUsername(const char * aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPassword(char * *aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetPassword(const char * aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPreHost(char * *aPreHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetPreHost(const char * aPreHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetHost(char * *aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetHost(const char * aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPort(PRInt32 *aPort)
{
  return NS_ERROR_FAILURE;
}
 
NS_IMETHODIMP
nsMozIconURI::SetPort(PRInt32 aPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPath(char * *aPath)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetPath(const char * aPath)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::Equals(nsIURI *other, PRBool *result)
{
  nsXPIDLCString spec1;
  nsXPIDLCString spec2;

  other->GetSpec(getter_Copies(spec2));
  GetSpec(getter_Copies(spec1));
  if (!nsCRT::strcasecmp(spec1, spec2))
    *result = PR_TRUE;
  else
    *result = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SchemeIs(const char *i_Scheme, PRBool *o_Equals)
{
  NS_ENSURE_ARG_POINTER(o_Equals);
  if (!i_Scheme) return NS_ERROR_INVALID_ARG;
  
  *o_Equals = PL_strcasecmp("moz-icon", i_Scheme) ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::Clone(nsIURI **result)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::Resolve(const char *relativePath, char **result)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIJARUri methods:

NS_IMETHODIMP
nsMozIconURI::GetIconFile(nsIURI* * aFileUrl)
{
  *aFileUrl = mFileIcon;
  NS_ADDREF(*aFileUrl);
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetIconFile(nsIURI* aFileUrl)
{
  mFileIcon = aFileUrl;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetImageSize(PRUint32 * aImageSize)  // measured by # of pixels in a row. defaults to 16.
{
  *aImageSize = mSize;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetImageSize(PRUint32 aImageSize)  // measured by # of pixels in a row. defaults to 16.
{
  mSize = aImageSize;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetContentType(char ** aContentType)  
{
  *aContentType = mContentType.ToNewCString();
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetContentType(const char * aContentType) 
{
  mContentType = aContentType;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
