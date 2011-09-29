/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Naoki Hotta <nhotta@netscape.com> (original author)
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
