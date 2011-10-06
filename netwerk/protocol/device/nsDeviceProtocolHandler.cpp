/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Camera.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Nino D'Aversa <ninodaversa@gmail.com>
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

#include "nsDeviceProtocolHandler.h"
#include "nsDeviceChannel.h"
#include "nsNetCID.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsSimpleURI.h"

//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS1(nsDeviceProtocolHandler,
                              nsIProtocolHandler)

nsresult
nsDeviceProtocolHandler::Init(){
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceProtocolHandler::GetScheme(nsACString &aResult)
{
  aResult.AssignLiteral("moz-device");
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceProtocolHandler::GetDefaultPort(PRInt32 *aResult)
{
  *aResult = -1;        // no port for moz_device: URLs
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceProtocolHandler::GetProtocolFlags(PRUint32 *aResult)
{
  *aResult = URI_NORELATIVE | URI_NOAUTH | URI_DANGEROUS_TO_LOAD;
  return NS_OK;
}

NS_IMETHODIMP
nsDeviceProtocolHandler::NewURI(const nsACString &spec,
                                const char *originCharset,
                                nsIURI *baseURI,
                                nsIURI **result)
{
  nsRefPtr<nsSimpleURI> uri = new nsSimpleURI();

  nsresult rv = uri->SetSpec(spec);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(uri, result);
}

NS_IMETHODIMP
nsDeviceProtocolHandler::NewChannel(nsIURI* aURI, nsIChannel **aResult)
{
  nsRefPtr<nsDeviceChannel> channel = new nsDeviceChannel();
  nsresult rv = channel->Init(aURI);
  NS_ENSURE_SUCCESS(rv, rv);

  return CallQueryInterface(channel, aResult);
}

NS_IMETHODIMP 
nsDeviceProtocolHandler::AllowPort(PRInt32 port,
                                   const char *scheme,
                                   bool *aResult)
{
  // don't override anything.  
  *aResult = PR_FALSE;
  return NS_OK;
}
