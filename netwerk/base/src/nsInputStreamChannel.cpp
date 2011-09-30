/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Darin Fisher <darin@meer.net>
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

#include "nsInputStreamChannel.h"

//-----------------------------------------------------------------------------
// nsInputStreamChannel

nsresult
nsInputStreamChannel::OpenContentStream(bool async, nsIInputStream **result,
                                        nsIChannel** channel)
{
  NS_ENSURE_TRUE(mContentStream, NS_ERROR_NOT_INITIALIZED);

  // If content length is unknown, then we must guess.  In this case, we assume
  // the stream can tell us.  If the stream is a pipe, then this will not work.

  PRInt64 len = ContentLength64();
  if (len < 0) {
    PRUint32 avail;
    nsresult rv = mContentStream->Available(&avail);
    if (rv == NS_BASE_STREAM_CLOSED) {
      // This just means there's nothing in the stream
      avail = 0;
    } else if (NS_FAILED(rv)) {
      return rv;
    }
    SetContentLength64(avail);
  }

  EnableSynthesizedProgressEvents(PR_TRUE);
  
  NS_ADDREF(*result = mContentStream);
  return NS_OK;
}

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsISupports

NS_IMPL_ISUPPORTS_INHERITED1(nsInputStreamChannel,
                             nsBaseChannel,
                             nsIInputStreamChannel)

//-----------------------------------------------------------------------------
// nsInputStreamChannel::nsIInputStreamChannel

NS_IMETHODIMP
nsInputStreamChannel::SetURI(nsIURI *uri)
{
  NS_ENSURE_TRUE(!URI(), NS_ERROR_ALREADY_INITIALIZED);
  nsBaseChannel::SetURI(uri);
  return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::GetContentStream(nsIInputStream **stream)
{
  NS_IF_ADDREF(*stream = mContentStream);
  return NS_OK;
}

NS_IMETHODIMP
nsInputStreamChannel::SetContentStream(nsIInputStream *stream)
{
  NS_ENSURE_TRUE(!mContentStream, NS_ERROR_ALREADY_INITIALIZED);
  mContentStream = stream;
  return NS_OK;
}
