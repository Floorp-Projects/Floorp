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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 */

#include "XRemoteContentListener.h"

XRemoteContentListener::XRemoteContentListener()
{
  NS_INIT_ISUPPORTS();
}

XRemoteContentListener::~XRemoteContentListener()
{
}

NS_IMPL_ISUPPORTS2(XRemoteContentListener,
		   nsIURIContentListener,
		   nsIInterfaceRequestor)

// nsIURIContentListener

NS_IMETHODIMP
XRemoteContentListener::OnStartURIOpen(nsIURI *aURI, PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
XRemoteContentListener::DoContent(const char *aContentType,
				  PRBool aIsContentPreferred,
				  nsIRequest *request,
				  nsIStreamListener **aContentHandler,
				  PRBool *_retval)
{
  NS_NOTREACHED("XRemoteContentListener::DoContent");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XRemoteContentListener::IsPreferred(const char *aContentType,
				    char **aDesiredContentType,
				    PRBool *_retval)
{
  return NS_OK;
}

NS_IMETHODIMP
XRemoteContentListener::CanHandleContent(const char *aContentType,
					 PRBool aIsContentPreferred,
					 char **aDesiredContentType,
					 PRBool *_retval)
{
  NS_NOTREACHED("XRemoteContentListener::CanHandleContent");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
XRemoteContentListener::GetLoadCookie(nsISupports * *aLoadCookie)
{
  *aLoadCookie = mLoadCookie;
  NS_IF_ADDREF(*aLoadCookie);
  return NS_OK;
}

NS_IMETHODIMP
XRemoteContentListener::SetLoadCookie(nsISupports * aLoadCookie)
{
  mLoadCookie = aLoadCookie;
  return NS_OK;
}

NS_IMETHODIMP
XRemoteContentListener::GetParentContentListener(nsIURIContentListener * *aParentContentListener)
{
  *aParentContentListener = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
XRemoteContentListener::SetParentContentListener(nsIURIContentListener * aParentContentListener)
{
  return NS_OK;
}

// nsIInterfaceRequestor
NS_IMETHODIMP
XRemoteContentListener::GetInterface(const nsIID & uuid, void * *result)
{
  NS_ENSURE_ARG_POINTER(result);
  return QueryInterface(uuid, result);
}
