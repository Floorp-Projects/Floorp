/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierUtils_h_
#define nsUrlClassifierUtils_h_

#include "nsAutoPtr.h"
#include "nsIUrlClassifierUtils.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"

class nsUrlClassifierUtils final : public nsIUrlClassifierUtils,
                                   public nsIObserver
{
public:
  typedef nsClassHashtable<nsCStringHashKey, nsCString> ProviderDictType;

private:
  /**
   * A fast, bit-vector map for ascii characters.
   *
   * Internally stores 256 bits in an array of 8 ints.
   * Does quick bit-flicking to lookup needed characters.
   */
  class Charmap
  {
  public:
    Charmap(uint32_t b0, uint32_t b1, uint32_t b2, uint32_t b3,
            uint32_t b4, uint32_t b5, uint32_t b6, uint32_t b7)
    {
      mMap[0] = b0; mMap[1] = b1; mMap[2] = b2; mMap[3] = b3;
      mMap[4] = b4; mMap[5] = b5; mMap[6] = b6; mMap[7] = b7;
    }

    /**
     * Do a quick lookup to see if the letter is in the map.
     */
    bool Contains(unsigned char c) const
    {
      return mMap[c >> 5] & (1 << (c & 31));
    }

  private:
    // Store the 256 bits in an 8 byte array.
    uint32_t mMap[8];
  };


public:
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

  nsAutoPtr<Charmap> mEscapeCharmap;

  // The provider lookup table and its mutex.
  ProviderDictType mProviderDict;
  mozilla::Mutex mProviderDictLock;
};

#endif // nsUrlClassifierUtils_h_
