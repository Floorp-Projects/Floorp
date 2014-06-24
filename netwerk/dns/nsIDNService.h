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
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIIDNSERVICE
  NS_DECL_NSIOBSERVER

  nsIDNService();

  nsresult Init();

protected:
  virtual ~nsIDNService();

private:
  void normalizeFullStops(nsAString& s);
  nsresult stringPrepAndACE(const nsAString& in, nsACString& out,
                            bool allowUnassigned, bool convertAllLabels);
  nsresult encodeToACE(const nsAString& in, nsACString& out);
  nsresult stringPrep(const nsAString& in, nsAString& out,
                      bool allowUnassigned);
  nsresult decodeACE(const nsACString& in, nsACString& out,
                     bool allowUnassigned, bool convertAllLabels);
  nsresult SelectiveUTF8toACE(const nsACString& input, nsACString& ace);
  nsresult SelectiveACEtoUTF8(const nsACString& input, nsACString& _retval);
  nsresult UTF8toACE(const nsACString& input, nsACString& ace,
                     bool allowUnassigned, bool convertAllLabels);
  nsresult ACEtoUTF8(const nsACString& input, nsACString& _retval,
                     bool allowUnassigned, bool convertAllLabels);
  bool isInWhitelist(const nsACString &host);
  void prefsChanged(nsIPrefBranch *prefBranch, const char16_t *pref);
  bool isLabelSafe(const nsAString &label);
  bool illegalScriptCombo(int32_t script, int32_t& savedScript);

  bool mMultilingualTestBed;  // if true generates extra node for multilingual testbed 
  idn_nameprep_t mNamePrepHandle;
  nsCOMPtr<nsIUnicodeNormalizer> mNormalizer;
  char mACEPrefix[kACEPrefixLen+1];
  nsXPIDLString mIDNBlacklist;
  bool mShowPunycode;
  enum restrictionProfile {
    eASCIIOnlyProfile,
    eHighlyRestrictiveProfile,
    eModeratelyRestrictiveProfile
  };
  restrictionProfile mRestrictionProfile;
  nsCOMPtr<nsIPrefBranch> mIDNWhitelistPrefBranch;
  bool mIDNUseWhitelist;
};

#endif  // nsIDNService_h__
