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

#ifndef nsSecurityWarningDialogs_h
#define nsSecurityWarningDialogs_h

#include "nsISecurityWarningDialogs.h"
#include "nsIPref.h"
#include "nsIStringBundle.h"
#include "nsCOMPtr.h"

class nsSecurityWarningDialogs : public nsISecurityWarningDialogs
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYWARNINGDIALOGS

  nsSecurityWarningDialogs();
  virtual ~nsSecurityWarningDialogs();

  nsresult Init();

protected:
  nsresult AlertDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                   const PRUnichar *messageName,
                   const PRUnichar *showAgainName);
  nsresult ConfirmDialog(nsIInterfaceRequestor *ctx, const char *prefName,
                   const PRUnichar *messageName, 
                   const PRUnichar *showAgainName, PRBool* _result);
  nsCOMPtr<nsIStringBundle> mStringBundle;
  nsCOMPtr<nsIPref> mPref;
};

#define NS_SECURITYWARNINGDIALOGS_CID \
 { /* 8d995d4f-adcc-4159-b7f1-e94af72eeb88 */       \
  0x8d995d4f, 0xadcc, 0x4159,                       \
 {0xb7, 0xf1, 0xe9, 0x4a, 0xf7, 0x2e, 0xeb, 0x88} }

#endif
