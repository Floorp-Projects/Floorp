/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * Portions created by the Initial Developer are Copyright (C) 1999
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUnknownDecoder_h__
#define nsUnknownDecoder_h__

#include "nsIStreamConverter.h"
#include "nsIChannel.h"

#include "nsCOMPtr.h"
#include "nsString.h"

#define NS_UNKNOWNDECODER_CID                        \
{ /* 7d7008a0-c49a-11d3-9b22-0080c7cb1080 */         \
    0x7d7008a0,                                      \
    0xc49a,                                          \
    0x11d3,                                          \
    {0x9b, 0x22, 0x00, 0x80, 0xc7, 0xcb, 0x10, 0x80}       \
}


class nsUnknownDecoder : public nsIStreamConverter
{
public:
  // nsISupports methods
  NS_DECL_ISUPPORTS

  // nsIStreamConverter methods
  NS_DECL_NSISTREAMCONVERTER

  // nsIStreamListener methods
  NS_DECL_NSISTREAMLISTENER

  // nsIRequestObserver methods
  NS_DECL_NSIREQUESTOBSERVER

  nsUnknownDecoder();

protected:
  virtual ~nsUnknownDecoder();

  void DetermineContentType(nsIRequest* request);
  nsresult FireListenerNotifications(nsIRequest* request, nsISupports *aCtxt);

protected:
  nsCOMPtr<nsIStreamListener> mNextListener;
  void SniffForImageMimeType(const char *buf, PRUint32 len);

  char *mBuffer;
  PRUint32 mBufferLen;
  PRBool mRequireHTMLsuffix;

  nsCString mContentType;
};


#endif /* nsUnknownDecoder_h__ */

