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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@netscape.com> (original author)
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

#include "nsStreamListenerTee.h"

NS_IMPL_ISUPPORTS3(nsStreamListenerTee,
                   nsIStreamListener,
                   nsIRequestObserver,
                   nsIStreamListenerTee)

NS_IMETHODIMP
nsStreamListenerTee::OnStartRequest(nsIRequest *request,
                                    nsISupports *context)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_NOT_INITIALIZED);
    return mListener->OnStartRequest(request, context);
}

NS_IMETHODIMP
nsStreamListenerTee::OnStopRequest(nsIRequest *request,
                                   nsISupports *context,
                                   nsresult status)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_NOT_INITIALIZED);
    // it is critical that we close out the input stream tee
    if (mInputTee) {
        mInputTee->SetSink(nsnull);
        mInputTee = 0;
    }
    mSink = 0;
    return mListener->OnStopRequest(request, context, status);
}

NS_IMETHODIMP
nsStreamListenerTee::OnDataAvailable(nsIRequest *request,
                                     nsISupports *context,
                                     nsIInputStream *input,
                                     PRUint32 offset,
                                     PRUint32 count)
{
    NS_ENSURE_TRUE(mListener, NS_ERROR_NOT_INITIALIZED);
    NS_ENSURE_TRUE(mSink, NS_ERROR_NOT_INITIALIZED);

    nsCOMPtr<nsIInputStream> tee;
    nsresult rv;

    if (!mInputTee) {
        rv = NS_NewInputStreamTee(getter_AddRefs(tee), input, mSink);
        if (NS_FAILED(rv)) return rv;

        mInputTee = do_QueryInterface(tee, &rv);
        if (NS_FAILED(rv)) return rv;
    }
    else {
        // re-initialize the input tee since the input stream may have changed.
        rv = mInputTee->SetSource(input);
        if (NS_FAILED(rv)) return rv;

        tee = do_QueryInterface(mInputTee, &rv);
        if (NS_FAILED(rv)) return rv;
    }

    return mListener->OnDataAvailable(request, context, tee, offset, count);
}

NS_IMETHODIMP
nsStreamListenerTee::Init(nsIStreamListener *listener,
                          nsIOutputStream *sink)
{
    mListener = listener;
    mSink = sink;
    return NS_OK;
}
