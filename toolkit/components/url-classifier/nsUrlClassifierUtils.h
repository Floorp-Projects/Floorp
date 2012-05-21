/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsUrlClassifierUtils_h_
#define nsUrlClassifierUtils_h_

#include "nsAutoPtr.h"
#include "nsIUrlClassifierUtils.h"
#include "nsTArray.h"
#include "nsDataHashtable.h"

class nsUrlClassifierUtils : public nsIUrlClassifierUtils
{
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
    Charmap(PRUint32 b0, PRUint32 b1, PRUint32 b2, PRUint32 b3,
            PRUint32 b4, PRUint32 b5, PRUint32 b6, PRUint32 b7)
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
    PRUint32 mMap[8];
  };


public:
  nsUrlClassifierUtils();
  ~nsUrlClassifierUtils() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUTILS

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
                    PRUint32 bytes,
                    bool allowOctal,
                    nsACString & _retval);

  // Convert an urlsafe base64 string to a normal base64 string.
  // This method will leave an already-normal base64 string alone.
  static void UnUrlsafeBase64(nsACString & str);

  // Takes an urlsafe-base64 encoded client key and gives back binary
  // key data
  static nsresult DecodeClientKey(const nsACString & clientKey,
                                  nsACString & _retval);
private:
  // Disallow copy constructor
  nsUrlClassifierUtils(const nsUrlClassifierUtils&);

  // Function to tell if we should encode a character.
  bool ShouldURLEscape(const unsigned char c) const;

  void CleanupHostname(const nsACString & host, nsACString & _retval);

  nsAutoPtr<Charmap> mEscapeCharmap;
};

#endif // nsUrlClassifierUtils_h_
