/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Terry Hayes <thayes@netscape.com>
*/

#ifndef __NS_NSSDIALOGS_H__
#define __NS_NSSDIALOGS_H__

#include "nsINSSDialogs.h"
#include "nsIBadCertListener.h"

#include "nsCOMPtr.h"
#include "nsIStringBundle.h"
#include "nsIPref.h"

#define NS_NSSDIALOGS_CID \
  { 0x518e071f, 0x1dd2, 0x11b2, \
    { 0x93, 0x7e, 0xc4, 0x5f, 0x14, 0xde, 0xf7, 0x78 }}

class nsNSSDialogs
: public nsINSSDialogs,
  public nsITokenPasswordDialogs,
  public nsIBadCertListener,
  public nsISecurityWarningDialogs,
  public nsICertificateDialogs,
  public nsIClientAuthDialogs,
  public nsICertPickDialogs,
  public nsITokenDialogs,
  public nsIDOMCryptoDialogs,
  public nsIGeneratingKeypairInfoDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSINSSDIALOGS
  NS_DECL_NSITOKENPASSWORDDIALOGS
  NS_DECL_NSIBADCERTLISTENER
  NS_DECL_NSISECURITYWARNINGDIALOGS
  NS_DECL_NSICERTIFICATEDIALOGS
  NS_DECL_NSICLIENTAUTHDIALOGS
  NS_DECL_NSICERTPICKDIALOGS
  NS_DECL_NSITOKENDIALOGS
  NS_DECL_NSIDOMCRYPTODIALOGS
  NS_DECL_NSIGENERATINGKEYPAIRINFODIALOGS
  nsNSSDialogs();
  virtual ~nsNSSDialogs();

  nsresult Init();

protected:
  nsresult AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                   const PRUnichar *messageName,
                   const PRUnichar *showAgainName);
  nsresult ConfirmDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                   const PRUnichar *messageName, 
                   const PRUnichar *showAgainName, PRBool* _result);
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsIStringBundle> mPIPStringBundle;
  nsCOMPtr<nsIPref> mPref;
};

#endif
