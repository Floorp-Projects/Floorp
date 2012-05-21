/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIDNService_h__
#define nsIDNService_h__

#include "nsIIDNService.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsWeakReference.h"
#include "nsIUnicodeNormalizer.h"
#include "nsIDNKitInterface.h"
#include "nsString.h"

class nsIPrefBranch;

//-----------------------------------------------------------------------------
// nsIDNService
//-----------------------------------------------------------------------------

#define kACEPrefixLen 4 

class nsIDNService : public nsIIDNService,
                     public nsIObserver,
                     public nsSupportsWeakReference
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIIDNSERVICE
  NS_DECL_NSIOBSERVER

  nsIDNService();
  virtual ~nsIDNService();

  nsresult Init();

private:
  void normalizeFullStops(nsAString& s);
  nsresult stringPrepAndACE(const nsAString& in, nsACString& out,
                            bool allowUnassigned);
  nsresult encodeToACE(const nsAString& in, nsACString& out);
  nsresult stringPrep(const nsAString& in, nsAString& out,
                      bool allowUnassigned);
  nsresult decodeACE(const nsACString& in, nsACString& out,
                     bool allowUnassigned);
  nsresult UTF8toACE(const nsACString& in, nsACString& out,
                     bool allowUnassigned);
  nsresult ACEtoUTF8(const nsACString& in, nsACString& out,
                     bool allowUnassigned);
  bool isInWhitelist(const nsACString &host);
  void prefsChanged(nsIPrefBranch *prefBranch, const PRUnichar *pref);

  bool mMultilingualTestBed;  // if true generates extra node for multilingual testbed 
  idn_nameprep_t mNamePrepHandle;
  nsCOMPtr<nsIUnicodeNormalizer> mNormalizer;
  char mACEPrefix[kACEPrefixLen+1];
  nsXPIDLString mIDNBlacklist;
  bool mShowPunycode;
  nsCOMPtr<nsIPrefBranch> mIDNWhitelistPrefBranch;
};

#endif  // nsIDNService_h__
