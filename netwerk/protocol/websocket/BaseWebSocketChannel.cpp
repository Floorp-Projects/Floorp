/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Josh Matthews <josh@joshmatthews.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "WebSocketLog.h"
#include "BaseWebSocketChannel.h"
#include "nsILoadGroup.h"
#include "nsIInterfaceRequestor.h"
#include "nsIURI.h"
#include "nsAutoPtr.h"
#include "nsStandardURL.h"

#if defined(PR_LOGGING)
PRLogModuleInfo *webSocketLog = nsnull;
#endif

namespace mozilla {
namespace net {

BaseWebSocketChannel::BaseWebSocketChannel()
  : mEncrypted(false)
{
#if defined(PR_LOGGING)
  if (!webSocketLog)
    webSocketLog = PR_NewLogModule("nsWebSocket");
#endif
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIWebSocketChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
BaseWebSocketChannel::GetOriginalURI(nsIURI **aOriginalURI)
{
  LOG(("BaseWebSocketChannel::GetOriginalURI() %p\n", this));

  if (!mOriginalURI)
    return NS_ERROR_NOT_INITIALIZED;
  NS_ADDREF(*aOriginalURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetURI(nsIURI **aURI)
{
  LOG(("BaseWebSocketChannel::GetURI() %p\n", this));

  if (!mOriginalURI)
    return NS_ERROR_NOT_INITIALIZED;
  if (mURI)
    NS_ADDREF(*aURI = mURI);
  else
    NS_ADDREF(*aURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::
GetNotificationCallbacks(nsIInterfaceRequestor **aNotificationCallbacks)
{
  LOG(("BaseWebSocketChannel::GetNotificationCallbacks() %p\n", this));
  NS_IF_ADDREF(*aNotificationCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::
SetNotificationCallbacks(nsIInterfaceRequestor *aNotificationCallbacks)
{
  LOG(("BaseWebSocketChannel::SetNotificationCallbacks() %p\n", this));
  mCallbacks = aNotificationCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetLoadGroup(nsILoadGroup **aLoadGroup)
{
  LOG(("BaseWebSocketChannel::GetLoadGroup() %p\n", this));
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetLoadGroup(nsILoadGroup *aLoadGroup)
{
  LOG(("BaseWebSocketChannel::SetLoadGroup() %p\n", this));
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetExtensions(nsACString &aExtensions)
{
  LOG(("BaseWebSocketChannel::GetExtensions() %p\n", this));
  aExtensions = mNegotiatedExtensions;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocol(nsACString &aProtocol)
{
  LOG(("BaseWebSocketChannel::GetProtocol() %p\n", this));
  aProtocol = mProtocol;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::SetProtocol(const nsACString &aProtocol)
{
  LOG(("BaseWebSocketChannel::SetProtocol() %p\n", this));
  mProtocol = aProtocol;                        /* the sub protocol */
  return NS_OK;
}

//-----------------------------------------------------------------------------
// BaseWebSocketChannel::nsIProtocolHandler
//-----------------------------------------------------------------------------


NS_IMETHODIMP
BaseWebSocketChannel::GetScheme(nsACString &aScheme)
{
  LOG(("BaseWebSocketChannel::GetScheme() %p\n", this));

  if (mEncrypted)
    aScheme.AssignLiteral("wss");
  else
    aScheme.AssignLiteral("ws");
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetDefaultPort(PRInt32 *aDefaultPort)
{
  LOG(("BaseWebSocketChannel::GetDefaultPort() %p\n", this));

  if (mEncrypted)
    *aDefaultPort = kDefaultWSSPort;
  else
    *aDefaultPort = kDefaultWSPort;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::GetProtocolFlags(PRUint32 *aProtocolFlags)
{
  LOG(("BaseWebSocketChannel::GetProtocolFlags() %p\n", this));

  *aProtocolFlags = URI_NORELATIVE | URI_NON_PERSISTABLE | ALLOWS_PROXY | 
      ALLOWS_PROXY_HTTP | URI_DOES_NOT_RETURN_DATA | URI_DANGEROUS_TO_LOAD;
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewURI(const nsACString & aSpec, const char *aOriginCharset,
                             nsIURI *aBaseURI, nsIURI **_retval NS_OUTPARAM)
{
  LOG(("BaseWebSocketChannel::NewURI() %p\n", this));

  PRInt32 port;
  nsresult rv = GetDefaultPort(&port);
  if (NS_FAILED(rv))
    return rv;

  nsRefPtr<nsStandardURL> url = new nsStandardURL();
  rv = url->Init(nsIStandardURL::URLTYPE_AUTHORITY, port, aSpec,
                aOriginCharset, aBaseURI);
  if (NS_FAILED(rv))
    return rv;
  NS_ADDREF(*_retval = url);
  return NS_OK;
}

NS_IMETHODIMP
BaseWebSocketChannel::NewChannel(nsIURI *aURI, nsIChannel **_retval NS_OUTPARAM)
{
  LOG(("BaseWebSocketChannel::NewChannel() %p\n", this));
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
BaseWebSocketChannel::AllowPort(PRInt32 port, const char *scheme,
                                PRBool *_retval NS_OUTPARAM)
{
  LOG(("BaseWebSocketChannel::AllowPort() %p\n", this));

  // do not override any blacklisted ports
  *_retval = PR_FALSE;
  return NS_OK;
}

} // namespace net
} // namespace mozilla
