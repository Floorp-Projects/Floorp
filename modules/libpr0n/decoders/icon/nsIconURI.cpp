/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIURL.h"
#include "nsCRT.h"
#include "nsReadableUtils.h"
#include "nsPrintfCString.h"

static NS_DEFINE_CID(kIOServiceCID,     NS_IOSERVICE_CID);
#define DEFAULT_IMAGE_SIZE          16

// helper function for parsing out attributes like size, and contentType
// from the icon url.
static void extractAttributeValue(const char * searchString, const char * attributeName, char ** result);
 
////////////////////////////////////////////////////////////////////////////////
 
nsMozIconURI::nsMozIconURI()
{
  mSize = DEFAULT_IMAGE_SIZE;
}
 
nsMozIconURI::~nsMozIconURI()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMozIconURI, nsIMozIconURI, nsIURI)

#define NS_MOZICON_SCHEME           "moz-icon:"
#define NS_MOZ_ICON_DELIMITER        '?'


nsresult
nsMozIconURI::FormatSpec(nsACString &spec)
{
  nsresult rv = NS_OK;
  spec = NS_MOZICON_SCHEME;

  if (mFileIcon)
  {
    nsCAutoString fileIconSpec;
    rv = mFileIcon->GetSpec(fileIconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    spec += fileIconSpec;
  }
  else
  {
    spec += "//";
    spec += mDummyFilePath;
  }

  spec += NS_MOZ_ICON_DELIMITER;
  spec += "size=";
  spec.Append(nsPrintfCString("%d", mSize));
  if (!mContentType.IsEmpty())
  {
    spec += "&contentType=";
    spec += mContentType.get();
  }
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsURI methods:

NS_IMETHODIMP
nsMozIconURI::GetSpec(nsACString &aSpec)
{
  return FormatSpec(aSpec);
}

// takes a string like ?size=32&contentType=text/html and returns a new string 
// containing just the attribute value. i.e you could pass in this string with
// an attribute name of size, this will return 32
// Assumption: attribute pairs in the string are separated by '&'.
void extractAttributeValue(const char * searchString, const char * attributeName, char ** result)
{
  //NS_ENSURE_ARG_POINTER(extractAttributeValue);

	char * attributeValue = nsnull;
	if (searchString && attributeName)
	{
		// search the string for attributeName
		PRUint32 attributeNameSize = PL_strlen(attributeName);
		char * startOfAttribute = PL_strcasestr(searchString, attributeName);
		if (startOfAttribute)
		{
			startOfAttribute += attributeNameSize; // skip over the attributeName
			if (startOfAttribute) // is there something after the attribute name
			{
				char * endofAttribute = startOfAttribute ? PL_strchr(startOfAttribute, '&') : nsnull;
				if (startOfAttribute && endofAttribute) // is there text after attribute value
					attributeValue = PL_strndup(startOfAttribute, endofAttribute - startOfAttribute);
				else // there is nothing left so eat up rest of line.
					attributeValue = PL_strdup(startOfAttribute);
			} // if we have a attribute value

		} // if we have a attribute name
	} // if we got non-null search string and attribute name values

  *result = attributeValue; // passing ownership of attributeValue into result...no need to 
	return;
}

NS_IMETHODIMP
nsMozIconURI::SetSpec(const nsACString &aSpec)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService (do_GetService(kIOServiceCID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString scheme;
  rv = ioService->ExtractScheme(aSpec, scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (strcmp("moz-icon", scheme.get()) != 0) 
    return NS_ERROR_MALFORMED_URI;

  nsCAutoString mozIconPath(aSpec);
  PRInt32 endPos = mozIconPath.FindChar(':') + 1; // guaranteed to exist!
  PRInt32 pos = mozIconPath.FindChar(NS_MOZ_ICON_DELIMITER);

  if (pos == -1) // no size or content type specified
  {
    mozIconPath.Right(mDummyFilePath, mozIconPath.Length() - endPos);
  }
  else
  {
    mozIconPath.Mid(mDummyFilePath, endPos, pos - endPos);
    // fill in any size and content type values...
    nsXPIDLCString sizeString;
    nsXPIDLCString contentTypeString;
    extractAttributeValue(mozIconPath.get() + pos, "size=", getter_Copies(sizeString));
    extractAttributeValue(mozIconPath.get() + pos, "contentType=", getter_Copies(contentTypeString));
    mContentType = contentTypeString;

    if (sizeString.get())
    {
      PRInt32 sizeValue = atoi(sizeString);
      // if the size value we got back is > 0 then use it
      if (sizeValue)
        mSize = sizeValue;
    }
  }

  // Okay now we have a bit of a hack here...filePath can have two forms:
  // (1) file://<some valid platform specific file url>
  // (2) //<some dummy file with an extension>
  // We need to determine which case we are and behave accordingly...
  if (mDummyFilePath.Length() > 2) // we should at least have two forward slashes followed by a file or a file://
  {
    if (!nsCRT::strncmp("//", mDummyFilePath.get(), 2))// must not have a url here..
    {
      // in this case the string looks like //somefile.html or // somefile.extension. So throw away the "//" part
      // and remember the rest in mDummyFilePath
      mDummyFilePath.Cut(0, 2); // cut the first 2 bytes....
    }
    else // we must have a url
    {
      // we have a file url.....so store it...
      rv = ioService->NewURI(mDummyFilePath, nsnull, nsnull, getter_AddRefs(mFileIcon));
      if (NS_FAILED(rv)) return NS_ERROR_MALFORMED_URI;
    }
  }
  else
    return NS_ERROR_MALFORMED_URI; // they didn't include a file path...
  return rv;
}

NS_IMETHODIMP
nsMozIconURI::GetPrePath(nsACString &prePath)
{
  prePath = NS_MOZICON_SCHEME;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetScheme(nsACString &aScheme)
{
  aScheme = "moz-icon";
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetScheme(const nsACString &aScheme)
{
  // doesn't make sense to set the scheme of a moz-icon URL
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetUsername(nsACString &aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetUsername(const nsACString &aUsername)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetPassword(nsACString &aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetPassword(const nsACString &aPassword)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetUserPass(nsACString &aUserPass)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetUserPass(const nsACString &aUserPass)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetHostPort(nsACString &aHostPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetHostPort(const nsACString &aHostPort)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::GetHost(nsACString &aHost)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::SetHost(const nsACString &aHost)
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
nsMozIconURI::GetPath(nsACString &aPath)
{
  aPath.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetPath(const nsACString &aPath)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::Equals(nsIURI *other, PRBool *result)
{
  nsCAutoString spec1;
  nsCAutoString spec2;

  other->GetSpec(spec2);
  GetSpec(spec1);
  if (!nsCRT::strcasecmp(spec1.get(), spec2.get()))
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
nsMozIconURI::Resolve(const nsACString &relativePath, nsACString &result)
{
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetAsciiSpec(nsACString &aSpecA)
{
  return GetSpec(aSpecA);
}

NS_IMETHODIMP
nsMozIconURI::GetAsciiHost(nsACString &aHostA)
{
  return GetHost(aHostA);
}

NS_IMETHODIMP
nsMozIconURI::GetOriginCharset(nsACString &result)
{
  result.Truncate();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsIIconUri methods:

NS_IMETHODIMP
nsMozIconURI::GetIconFile(nsIURI* * aFileUrl)
{
  *aFileUrl = mFileIcon;
  NS_IF_ADDREF(*aFileUrl);
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
nsMozIconURI::GetContentType(nsACString &aContentType)  
{
  aContentType = mContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetContentType(const nsACString &aContentType) 
{
  mContentType = aContentType;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetFileExtension(nsACString &aFileExtension)  
{
  nsCAutoString fileExtension;
  nsresult rv = NS_OK;

  // First, try to get the extension from mFileIcon if we have one
  if (mFileIcon)
  {
    nsCAutoString fileExt;
    nsCOMPtr<nsIURL> url (do_QueryInterface(mFileIcon, &rv));
    if (NS_SUCCEEDED(rv) && url)
    {
      rv = url->GetFileExtension(fileExt);
      if (NS_SUCCEEDED(rv))
      {
        // unfortunately, this code doesn't give us the required '.' in front of the extension
        // so we have to do it ourselves..
        aFileExtension = NS_LITERAL_CSTRING(".") + fileExt;
        return NS_OK;
      }
    }
    
    mFileIcon->GetSpec(fileExt);
    fileExtension = fileExt;
  }
  else
  {
    fileExtension = mDummyFilePath;
  }

  // truncate the extension out of the file path...
  const char * chFileName = fileExtension.get(); // get the underlying buffer
  const char * fileExt = strrchr(chFileName, '.');
  if (!fileExt) return NS_ERROR_FAILURE; // no file extension to work from.
  else
    aFileExtension = nsDependentCString(fileExt);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
