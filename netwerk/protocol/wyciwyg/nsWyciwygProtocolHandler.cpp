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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Radha Kulkarni(radha@netscape.com)
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

#include "nsWyciwyg.h"
#include "nsWyciwygChannel.h"
#include "nsWyciwygProtocolHandler.h"
#include "nsIURL.h"
#include "nsIComponentManager.h"
#include "nsNetCID.h"

#ifdef MOZ_IPC
#include "mozilla/net/NeckoChild.h"
#endif

#ifdef MOZ_IPC
using namespace mozilla::net;
#include "mozilla/net/WyciwygChannelChild.h"
#endif

////////////////////////////////////////////////////////////////////////////////

nsWyciwygProtocolHandler::nsWyciwygProtocolHandler() 
{
#if defined(PR_LOGGING)
  if (!gWyciwygLog)
    gWyciwygLog = PR_NewLogModule("nsWyciwygChannel");
#endif

  LOG(("Creating nsWyciwygProtocolHandler [this=%x].\n", this));
}

nsWyciwygProtocolHandler::~nsWyciwygProtocolHandler() 
{
  LOG(("Deleting nsWyciwygProtocolHandler [this=%x]\n", this));
}

NS_IMPL_ISUPPORTS1(nsWyciwygProtocolHandler, nsIProtocolHandler)

////////////////////////////////////////////////////////////////////////////////
// nsIProtocolHandler methods:
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetScheme(nsACString &result)
{
  result = "wyciwyg";
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetDefaultPort(PRInt32 *result) 
{
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP 
nsWyciwygProtocolHandler::AllowPort(PRInt32 port, const char *scheme, PRBool *_retval)
{
  // don't override anything.  
  *_retval = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewURI(const nsACString &aSpec,
                                 const char *aCharset, // ignored
                                 nsIURI *aBaseURI,
                                 nsIURI **result) 
{
  nsresult rv;

  nsCOMPtr<nsIURI> url = do_CreateInstance(NS_SIMPLEURI_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = url->SetSpec(aSpec);
  NS_ENSURE_SUCCESS(rv, rv);

  *result = url;
  NS_ADDREF(*result);

  return rv;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::NewChannel(nsIURI* url, nsIChannel* *result)
{
#ifdef MOZ_IPC
  if (mozilla::net::IsNeckoChild())
    mozilla::net::NeckoChild::InitNeckoChild();
#endif // MOZ_IPC

  NS_ENSURE_ARG_POINTER(url);
  nsresult rv;

  nsCOMPtr<nsIWyciwygChannel> channel;
#ifdef MOZ_IPC
  if (IsNeckoChild()) {
    NS_ENSURE_TRUE(gNeckoChild != nsnull, NS_ERROR_FAILURE);

    WyciwygChannelChild *wcc = static_cast<WyciwygChannelChild *>(
                                 gNeckoChild->SendPWyciwygChannelConstructor());
    if (!wcc)
      return NS_ERROR_OUT_OF_MEMORY;

    channel = wcc;
    rv = wcc->Init(url);
    if (NS_FAILED(rv))
      PWyciwygChannelChild::Send__delete__(wcc);
  } else
#endif
  {
    nsWyciwygChannel *wc = new nsWyciwygChannel();
    if (!wc)
      return NS_ERROR_OUT_OF_MEMORY;
    channel = wc;
    rv = wc->Init(url);
  }

  *result = channel.forget().get();
  if (NS_FAILED(rv)) {
    NS_RELEASE(*result);
    return rv;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWyciwygProtocolHandler::GetProtocolFlags(PRUint32 *result) 
{
  // Should this be an an nsINestedURI?  We don't really want random webpages
  // loading these URIs...

  // Note that using URI_INHERITS_SECURITY_CONTEXT here is OK -- untrusted code
  // is not allowed to link to wyciwyg URIs and users shouldn't be able to get
  // at them, and nsDocShell::InternalLoad forbids non-history loads of these
  // URIs.  And when loading from history we end up using the principal from
  // the history entry, which we put there ourselves, so all is ok.
  *result = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD |
    URI_INHERITS_SECURITY_CONTEXT;
  return NS_OK;
}
