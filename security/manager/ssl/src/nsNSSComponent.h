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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Hubbie Shaw
 *   Doug Turner <dougt@netscape.com>
 *   Brian Ryner <bryner@netscape.com>
 */

#ifndef _nsNSSComponent_h_
#define _nsNSSComponent_h_

#include "nsCOMPtr.h"
#include "nsISecurityManagerComponent.h"
#include "nsISignatureVerifier.h"
#include "nsIContentHandler.h"
#include "nsIEntropyCollector.h"
#include "nsString.h"
#include "nsIStringBundle.h"

#define SECURITY_STRING_BUNDLE_URL "chrome://communicator/locale/security.properties"

#define NS_NSSCOMPONENT_CID \
{0xa277189c, 0x1dd1, 0x11b2, {0xa8, 0xc9, 0xe4, 0xe8, 0xbf, 0xb1, 0x33, 0x8e}}

// Implementation of the PSM component interface.
class nsNSSComponent : public nsISecurityManagerComponent,
                       public nsIContentHandler,
                       public nsISignatureVerifier,
                       public nsIEntropyCollector
{
public:
  NS_DEFINE_STATIC_CID_ACCESSOR( NS_NSSCOMPONENT_CID );

  nsNSSComponent();
  virtual ~nsNSSComponent();

  NS_DECL_ISUPPORTS
  NS_DECL_NSISECURITYMANAGERCOMPONENT
  NS_DECL_NSICONTENTHANDLER
  NS_DECL_NSISIGNATUREVERIFIER
  NS_DECL_NSIENTROPYCOLLECTOR

  NS_METHOD Init();

  nsresult GetPIPNSSBundleString(const PRUnichar *name,
                                 nsString &outString);
  nsresult PIPBundleFormatStringFromName(const PRUnichar *name,
                                         const PRUnichar **params,
                                         PRUint32 numParams,
                                         PRUnichar **outString);
  nsresult InitializeNSS();

private:

  void InstallLoadableRoots();
  nsresult InitializePIPNSSBundle();

  nsCOMPtr<nsIStringBundle> mPIPNSSBundle;
  static PRBool mNSSInitialized;
};

#endif // _nsNSSComponent_h_

