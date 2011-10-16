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

#include "mozilla/Util.h"

#include "nsIconURI.h"
#include "nsNetUtil.h"
#include "nsIIOService.h"
#include "nsIURL.h"
#include "prprf.h"
#include "plstr.h"
#include <stdlib.h>

using namespace mozilla;

#define DEFAULT_IMAGE_SIZE 16

#if defined(MAX_PATH)
#define SANE_FILE_NAME_LEN MAX_PATH
#elif defined(PATH_MAX)
#define SANE_FILE_NAME_LEN PATH_MAX
#else
#define SANE_FILE_NAME_LEN 1024
#endif

// helper function for parsing out attributes like size, and contentType
// from the icon url.
static void extractAttributeValue(const char * searchString, const char * attributeName, nsCString& aResult);
 
static const char *kSizeStrings[] =
{
  "button",
  "toolbar",
  "toolbarsmall",
  "menu",
  "dnd",
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

#define MOZICON_SCHEME "moz-icon:"
#define MOZICON_SCHEME_LEN (sizeof(MOZICON_SCHEME) - 1)

////////////////////////////////////////////////////////////////////////////////
// nsURI methods:

NS_IMETHODIMP
nsMozIconURI::GetSpec(nsACString &aSpec)
{
  aSpec = MOZICON_SCHEME;

  if (mIconURL)
  {
    nsCAutoString fileIconSpec;
    nsresult rv = mIconURL->GetSpec(fileIconSpec);
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
    aSpec += mFileName;
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

NS_IMETHODIMP
nsMozIconURI::GetSpecIgnoringRef(nsACString &result)
{
  return GetSpec(result);
}

NS_IMETHODIMP
nsMozIconURI::GetHasRef(bool *result)
{
  *result = PR_FALSE;
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
  // Reset everything to default values.
  mIconURL = nsnull;
  mSize = DEFAULT_IMAGE_SIZE;
  mContentType.Truncate();
  mFileName.Truncate();
  mStockIcon.Truncate();
  mIconSize = -1;
  mIconState = -1;

  nsCAutoString iconSpec(aSpec);
  if (!Substring(iconSpec, 0, MOZICON_SCHEME_LEN).EqualsLiteral(MOZICON_SCHEME))
    return NS_ERROR_MALFORMED_URI;

  PRInt32 questionMarkPos = iconSpec.Find("?");
  if (questionMarkPos != -1 && static_cast<PRInt32>(iconSpec.Length()) > (questionMarkPos + 1))
  {
    extractAttributeValue(iconSpec.get(), "contentType=", mContentType);

    nsCAutoString sizeString;
    extractAttributeValue(iconSpec.get(), "size=", sizeString);
    if (!sizeString.IsEmpty())
    {      
      const char *sizeStr = sizeString.get();
      for (PRUint32 i = 0; i < ArrayLength(kSizeStrings); i++)
      {
        if (PL_strcasecmp(sizeStr, kSizeStrings[i]) == 0)
        {
          mIconSize = i;
          break;
        }
      }

      PRInt32 sizeValue = atoi(sizeString.get());
      if (sizeValue)
        mSize = sizeValue;
    }

    nsCAutoString stateString;
    extractAttributeValue(iconSpec.get(), "state=", stateString);
    if (!stateString.IsEmpty())
    {
      const char *stateStr = stateString.get();
      for (PRUint32 i = 0; i < ArrayLength(kStateStrings); i++)
      {
        if (PL_strcasecmp(stateStr, kStateStrings[i]) == 0)
        {
          mIconState = i;
          break;
        }
      }
    }
  }

  PRInt32 pathLength = iconSpec.Length() - MOZICON_SCHEME_LEN;
  if (questionMarkPos != -1)
    pathLength = questionMarkPos - MOZICON_SCHEME_LEN;
  if (pathLength < 3)
    return NS_ERROR_MALFORMED_URI;

  nsCAutoString iconPath(Substring(iconSpec, MOZICON_SCHEME_LEN, pathLength));

  // Icon URI path can have three forms:
  // (1) //stock/<icon-identifier>
  // (2) //<some dummy file with an extension>
  // (3) a valid URL

  if (!strncmp("//stock/", iconPath.get(), 8))
  {
    mStockIcon.Assign(Substring(iconPath, 8));
    // An icon identifier must always be specified.
    if (mStockIcon.IsEmpty())
      return NS_ERROR_MALFORMED_URI;
    return NS_OK;
  }

  if (StringBeginsWith(iconPath, NS_LITERAL_CSTRING("//")))
  {
    // Sanity check this supposed dummy file name.
    if (iconPath.Length() > SANE_FILE_NAME_LEN)
      return NS_ERROR_MALFORMED_URI;
    iconPath.Cut(0, 2);
    mFileName.Assign(iconPath);
  }

  nsresult rv;
  nsCOMPtr<nsIIOService> ioService(do_GetService(NS_IOSERVICE_CONTRACTID, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIURI> uri;
  ioService->NewURI(iconPath, nsnull, nsnull, getter_AddRefs(uri));
  mIconURL = do_QueryInterface(uri);
  if (mIconURL)
    mFileName.Truncate();
  else if (mFileName.IsEmpty())
    return NS_ERROR_MALFORMED_URI;

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::GetPrePath(nsACString &prePath)
{
  prePath = MOZICON_SCHEME;
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
nsMozIconURI::GetRef(nsACString &aRef)
{
  aRef.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetRef(const nsACString &aRef)
{
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsMozIconURI::Equals(nsIURI *other, bool *result)
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
nsMozIconURI::EqualsExceptRef(nsIURI *other, bool *result)
{
  // GetRef/SetRef not supported by nsMozIconURI, so
  // EqualsExceptRef() is the same as Equals().
  return Equals(other, result);
}

NS_IMETHODIMP
nsMozIconURI::SchemeIs(const char *i_Scheme, bool *o_Equals)
{
  NS_ENSURE_ARG_POINTER(o_Equals);
  if (!i_Scheme) return NS_ERROR_INVALID_ARG;
  
  *o_Equals = PL_strcasecmp("moz-icon", i_Scheme) ? PR_FALSE : PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::Clone(nsIURI **result)
{
  nsCOMPtr<nsIURL> newIconURL;
  if (mIconURL)
  {
    nsCOMPtr<nsIURI> newURI;
    nsresult rv = mIconURL->Clone(getter_AddRefs(newURI));
    if (NS_FAILED(rv))
      return rv;
    newIconURL = do_QueryInterface(newURI, &rv);
    if (NS_FAILED(rv))
      return rv;
  }

  nsMozIconURI *uri = new nsMozIconURI();
  newIconURL.swap(uri->mIconURL);
  uri->mSize = mSize;
  uri->mContentType = mContentType;
  uri->mFileName = mFileName;
  uri->mStockIcon = mStockIcon;
  uri->mIconSize = mIconSize;
  uri->mIconState = mIconState;
  NS_ADDREF(*result = uri);

  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::CloneIgnoringRef(nsIURI **result)
{
  // GetRef/SetRef not supported by nsMozIconURI, so
  // CloneIgnoringRef() is the same as Clone().
  return Clone(result);
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
nsMozIconURI::GetIconURL(nsIURL* * aFileUrl)
{
  *aFileUrl = mIconURL;
  NS_IF_ADDREF(*aFileUrl);
  return NS_OK;
}

NS_IMETHODIMP
nsMozIconURI::SetIconURL(nsIURL* aFileUrl)
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
  // First, try to get the extension from mIconURL if we have one
  if (mIconURL)
  {
    nsCAutoString fileExt;
    if (NS_SUCCEEDED(mIconURL->GetFileExtension(fileExt)))
    {
      if (!fileExt.IsEmpty())
      {
        // unfortunately, this code doesn't give us the required '.' in front of the extension
        // so we have to do it ourselves..
        aFileExtension.Assign('.');
        aFileExtension.Append(fileExt);
      }
    }
    return NS_OK;
  }

  if (!mFileName.IsEmpty())
  {
    // truncate the extension out of the file path...
    const char * chFileName = mFileName.get(); // get the underlying buffer
    const char * fileExt = strrchr(chFileName, '.');
    if (!fileExt)
      return NS_OK;
    aFileExtension = fileExt;
  }

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
  if (mIconState >= 0)
    aState = kStateStrings[mIconState];
  else
    aState.Truncate();
  return NS_OK;
}
////////////////////////////////////////////////////////////////////////////////
