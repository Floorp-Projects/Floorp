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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2007
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsUrlClassifierUtils_h_
#define nsUrlClassifierUtils_h_

#include "nsAutoPtr.h"
#include "nsIUrlClassifierUtils.h"

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
    PRBool Contains(unsigned char c) const
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

  nsresult Init();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIURLCLASSIFIERUTILS

  // This function will encode all "special" characters in typical url encoding,
  // that is %hh where h is a valid hex digit.  The characters which are encoded
  // by this function are any ascii characters under 32(control characters and
  // space), 37(%), and anything 127 or above (special characters).  Url is the
  // string to encode, ret is the encoded string.  Function returns true if
  // ret != url.
  PRBool SpecialEncode(const nsACString & url, nsACString & _retval);

private:
  // Disallow copy constructor
  nsUrlClassifierUtils(const nsUrlClassifierUtils&);

  // Function to tell if we should encode a character.
  PRBool ShouldURLEscape(const unsigned char c) const;

  nsAutoPtr<Charmap> mEscapeCharmap;
};

#endif // nsUrlClassifierUtils_h_
