/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIconURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "prprf.h"
#include "plstr.h"
#include <stdlib.h>

#define DEFAULT_IMAGE_SIZE          16

// helper function for parsing out attributes like size, and contentType
// from the icon url.
static void extractAttributeValue(const char * searchString, const char * attributeName, nsCString& aResult);
 
static const char *kSizeStrings[] =
{
  "button",
  "toolbar",
  "toolbarsmall",
  "menu",
  "dialog"
};

static const char *kStateStrings[] =
{
  "normal",
  "disabled"
};

////////////////////////////////////////////////////////////////////////////////
 
nsMozIconURI::nsMozIconURI()
  : mSize(DEFAULT_IMAGE_SIZE),
    mIconSize(-1),
    mIconState(-1)
{
}
 
nsMozIconURI::~nsMozIconURI()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS2(nsMozIconURI, nsIMozIconURI, nsIURI)

#define NS_MOZICON_SCHEME           "moz-icon:"
#define NS_MOZ_ICON_DELIMITER        '?'


////////////////////////////////////////////////////////////////////////////////
// nsURI methods:

NS_IMETHODIMP
nsMozIconURI::GetSpec(nsACString &aSpec)
{
  aSpec = NS_MOZICON_SCHEME;

  if (mFileIcon)
  {
    nsCAutoString fileIconSpec;
    nsresult rv = mFileIcon->GetSpec(fileIconSpec);
    NS_ENSURE_SUCCESS(rv, rv);
    aSpec += fileIconSpec;
  }
  else if (!mStockIcon.IsEmpty())
  {
    aSpec += "//stock/";
    aSpec += mStockIcon;
  }
  else
  {
    aSpec += "//";
    aSpec += mDummyFilePath;
  }

  aSpec += "?size=";
  if (mIconSize >= 0)
  {
    aSpec += kSizeStrings[mIconSize];
  }
  else
  {
    char buf[20];
    PR_snprintf(buf, sizeof(buf), "%d", mSize);
    aSpec.Append(buf);
  }

  if (mIconState >= 0) {
    aSpec += "&state=";
    aSpec += kStateStrings[mIconState];
  }

  if (!mContentType.IsEmpty())
  {
    aSpec += "&contentType=";
    aSpec += mContentType.get();
  }
  
  return NS_OK;
}

// takes a string like ?size=32&contentType=text/html and returns a new string 
// containing just the attribute value. i.e you could pass in this string with
// an attribute name of 'size=', this will return 32
// Assumption: attribute pairs in the string are separated by '&'.
void extractAttributeValue(const char * searchString, const char * attributeName, nsCString& result)
{
  //NS_ENSURE_ARG_POINTER(extractAttributeValue);

  result.Truncate();

  if (searchString && attributeName)
  {
    // search the string for attributeName
    PRUint32 attributeNameSize = strlen(attributeName);
    const char * startOfAttribute = PL_strcasestr(searchString, attributeName);
    if (startOfAttribute &&
       ( *(startOfAttribute-1) == '?' || *(startOfAttribute-1) == '&') )
    {
      startOfAttribute += attributeNameSize; // skip over the attributeName
      if (*startOfAttribute) // is there something after the attribute name
      {
        const char * endofAttribute = strchr(startOfAttribute, '&');
        if (endofAttribute)
          result.Assign(Substring(startOfAttribute, endofAttribute));
        else
          result.Assign(startOfAttribute);
      } // if we have a attribute value
    } // if we have a attribute name
  } // if we got non-null search string and attribute name values
}

NS_IMETHODIMP
nsMozIconURI::SetSpec(const nsACString &aSpec)
{
  nsresult rv;
  nsCOMPtr<nsIIOService> ioService (do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCAutoString scheme;
  rv = ioService->ExtractScheme(aSpec, scheme);
  NS_ENSURE_SUCCESS(rv, rv);

  if (strcmp("moz-icon", scheme.get()) != 0) 
    return NS_ERROR_MALFORMED_URI;

  nsCAutoString sizeString;
  nsCAutoString stateString;
  nsCAutoString mozIconPath(aSpec);

  // guaranteed to exist!
  const char *path = strchr(mozIconPath.get(), ':') + 1;
  const char *question = strchr(mozIconPath.get(), NS_MOZ_ICON_DELIMITER);

  if (!question) // no size or content type specified
  {
    mDummyFilePath.Assign(path);
  }
  else
  {
    mDummyFilePath.Assign(Substring(path, question));

    // fill in any size and content type values...
    extractAttributeValue(question, "size=", sizeString);
    extractAttributeValue(question, "state=", stateString);
    extractAttributeValue(question, "contentType=", mContentType);
  }

  if (!sizeString.IsEmpty())
  {
    const char *sizeStr = sizeString.get();
    for (PRInt32 i = 0; i < NS_ARRAY_LENGTH(kSizeStrings); i++)
    {
      if (PL_strcasecmp(sizeStr, kSizeStrings[i]) == 0)
      {
        mIconSize = i;
        break;
      }
    }
  }

  if (!stateString.IsEmpty())
  {
    const char *stateStr = stateString.get();
    for (PRInt32 i = 0; i < NS_ARRAY_LENGTH(kStateStrings); i++)
    {
      if (PL_strcasecmp(stateStr, kStateStrings[i]) == 0)
      {
        mIconState = i;
        break;
      }
    }
  }

  // Okay now we have a bit of a hack here...filePath can have three forms:
  // (1) file://<some valid platform specific file url>
  // (2) //<some dummy file with an extension>
  // (3) stock/<icon-identifier>
  // We need to determine which case we are and behave accordingly...
  if (mDummyFilePath.Length() > 2)
  {
    if (!strncmp("//stock/", mDummyFilePath.get(), 8))
    {
      // we have a stock icon
      mStockIcon = Substring(mDummyFilePath, 8);
    }
    else
    {
      if (!strncmp("//", mDummyFilePath.get(), 2))// must not have a url here..
      {
        // in this case the string looks like //somefile.html or // somefile.extension. So throw away the "//" part
        // and remember the rest in mDummyFilePath
        mDummyFilePath.Cut(0, 2); // cut the first 2 bytes....
      }
      if (!strncmp("file://", mDummyFilePath.get(), 7))
      { 
        // we have a file url, let the IOService normalize it
        nsCOMPtr<nsIURI> tmpURI;
        rv = ioService->NewURI(mDummyFilePath, nsnull, nsnull, getter_AddRefs(tmpURI));
        if (NS_SUCCEEDED(rv) && tmpURI)
        {
          mFileIcon = tmpURI;
        }
      }
      if (!sizeString.IsEmpty())
      {
        PRInt32 sizeValue = atoi(sizeString.get());
        // if the size value we got back is > 0 then use it
        if (sizeValue)
          mSize = sizeValue;
      }
    }
  }
  else
    rv = NS_ERROR_MALFORMED_URI; // they didn't include a file path...
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
  NS_ENSURE_ARG_POINTER(other);
  NS_PRECONDITION(result, "null pointer");

  nsCAutoString spec1;
  nsCAutoString spec2;

  other->GetSpec(spec2);
  GetSpec(spec1);
  if (!PL_strcasecmp(spec1.get(), spec2.get()))
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
  nsCOMPtr<nsIURI> newFileIcon;
  if (mFileIcon)
  {
    nsresult rv = mFileIcon->Clone(getter_AddRefs(newFileIcon));
    if (NS_FAILED(rv)) 
      return rv;
  }

  nsMozIconURI *uri = new nsMozIconURI();
  if (!uri)
    return NS_ERROR_OUT_OF_MEMORY;
 
  newFileIcon.swap(uri->mFileIcon);
  uri->mSize = mSize;
  uri->mContentType = mContentType;
  uri->mDummyFilePath = mDummyFilePath;
  uri->mStockIcon = mStockIcon;
  uri->mIconSize = mIconSize;
  uri->mIconState = mIconState;
  NS_ADDREF(*result = uri);

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::Resolve(const nsACString &relativePath, nsACString &result)
{
  return NS_ERROR_NOT_IMPLEMENTED;
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
  // this isn't called anywhere, needs to go through SetSpec parsing
  return NS_ERROR_NOT_IMPLEMENTED;
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
        aFileExtension.Assign('.');
        aFileExtension.Append(fileExt);
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

  aFileExtension = fileExt;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetStockIcon(nsACString &aStockIcon)
{
  aStockIcon = mStockIcon;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetIconSize(nsACString &aSize)
{
  if (mIconSize >= 0)
    aSize = kSizeStrings[mIconSize];
  else
    aSize.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetIconState(nsACString &aState)
{
  if (mIconState)
    aState = kStateStrings[mIconState];
  else
    aState.Truncate();
  return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
