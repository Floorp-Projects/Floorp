/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierUtils_h_
#define nsUrlClassifierUtils_h_

#include "nsIUrlClassifierUtils.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"

#define TESTING_TABLE_PROVIDER_NAME "test"

class nsUrlClassifierUtils final : public nsIUrlClassifierUtils,
                                   public nsIObserver
{
public:
  typedef nsClassHashtable<nsCStringHashKey, nsCString> ProviderDictType;

  nsUrlClassifierUtils();

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUTILS
  NS_DECL_NSIOBSERVER

  nsresult Init();

  nsresult CanonicalizeHostname(const nsACString & hostname,
                                nsACString & _retval);
  nsresult CanonicalizePath(const nsACString & url, nsACString & _retval);

  // This function will encode all "special" characters in typical url encoding,
  // that is %hh where h is a valid hex digit.  The characters which are encoded
  // by this function are any ascii characters under 32(control characters and
  // space), 37(%), and anything 127 or above (special characters).  Url is the
  // string to encode, ret is the encoded string.  Function returns true if
  // ret != url.
  bool SpecialEncode(const nsACString & url,
                       bool foldSlashes,
                       nsACString & _retval);

  void ParseIPAddress(const nsACString & host, nsACString & _retval);
  void CanonicalNum(const nsACString & num,
                    uint32_t bytes,
                    bool allowOctal,
                    nsACString & _retval);

private:
  ~nsUrlClassifierUtils() {}

  // Disallow copy constructor
  nsUrlClassifierUtils(const nsUrlClassifierUtils&);

  // Function to tell if we should encode a character.
  bool ShouldURLEscape(const unsigned char c) const;

  void CleanupHostname(const nsACString & host, nsACString & _retval);

  nsresult ReadProvidersFromPrefs(ProviderDictType& aDict);

  // The provider lookup table and its mutex.
  ProviderDictType mProviderDict;
  mozilla::Mutex mProviderDictLock;
};

#endif // nsUrlClassifierUtils_h_
