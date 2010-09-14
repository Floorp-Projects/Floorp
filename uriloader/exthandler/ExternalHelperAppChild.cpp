/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/****** BEGIN LICENSE BLOCK *****
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
 * The Original Code is IPC External Helper App module.
 *
 * The Initial Developer of the Original Code is
 * Brian Crowder <crowderbt@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#include "ExternalHelperAppChild.h"
#include "nsIInputStream.h"
#include "nsIRequest.h"
#include "nsIResumableChannel.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS2(ExternalHelperAppChild,
                   nsIStreamListener,
                   nsIRequestObserver)

ExternalHelperAppChild::ExternalHelperAppChild()
  : mStatus(NS_OK)
{
}

ExternalHelperAppChild::~ExternalHelperAppChild()
{
}

//-----------------------------------------------------------------------------
// nsIStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
ExternalHelperAppChild::OnDataAvailable(nsIRequest *request,
                                        nsISupports *ctx,
                                        nsIInputStream *input,
                                        PRUint32 offset,
                                        PRUint32 count)
{
  if (NS_FAILED(mStatus))
    return mStatus;

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(input, data, count);
  if (NS_FAILED(rv))
    return rv;

  if (!SendOnDataAvailable(data, offset, count))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
ExternalHelperAppChild::OnStartRequest(nsIRequest *request, nsISupports *ctx)
{
  nsresult rv = mHandler->OnStartRequest(request, ctx);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  nsCString entityID;
  nsCOMPtr<nsIResumableChannel> resumable(do_QueryInterface(request));
  if (resumable)
    resumable->GetEntityID(entityID);
  SendOnStartRequest(entityID);
  return NS_OK;
}


NS_IMETHODIMP
ExternalHelperAppChild::OnStopRequest(nsIRequest *request,
                                      nsISupports *ctx,
                                      nsresult status)
{
  nsresult rv = mHandler->OnStopRequest(request, ctx, status);
  SendOnStopRequest(status);

  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);
  return NS_OK;
}


bool
ExternalHelperAppChild::RecvCancel(const nsresult& aStatus)
{
  mStatus = aStatus;
  return true;
}

} // namespace dom
} // namespace mozilla
