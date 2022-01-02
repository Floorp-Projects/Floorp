/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacPreferencesReader_h__
#define MacPreferencesReader_h__

//-----------------------------------------------------------------------------

#include "nsIMacPreferencesReader.h"

#define NS_MACPREFERENCESREADER_CID                  \
  {                                                  \
    0xb0f20595, 0x88ce, 0x4738, {                    \
      0xa1, 0xa4, 0x24, 0xde, 0x78, 0xeb, 0x80, 0x51 \
    }                                                \
  }
#define NS_MACPREFERENCESREADER_CONTRACTID \
  "@mozilla.org/mac-preferences-reader;1"

//-----------------------------------------------------------------------------

class nsMacPreferencesReader : public nsIMacPreferencesReader {
 public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACPREFERENCESREADER

  nsMacPreferencesReader(){};

 protected:
  virtual ~nsMacPreferencesReader() = default;
};

#endif  // MacPreferencesReader_h__
