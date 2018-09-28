/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MacPreferencesReader_h__
#define MacPreferencesReader_h__

//-----------------------------------------------------------------------------

#include "nsIMacPreferencesReader.h"

#define  NS_MACPREFERENCESREADER_CID \
{ 0x95790842, 0x75a0, 0x430d, \
  { 0x98, 0xbf, 0xf5, 0xce, 0x37, 0x88, 0xea, 0x6d } }
#define NS_MACPREFERENCESREADER_CONTRACTID \
  "@mozilla.org/mac-preferences-reader;1"

//-----------------------------------------------------------------------------

class nsMacPreferencesReader : public nsIMacPreferencesReader
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIMACPREFERENCESREADER

  nsMacPreferencesReader() {};

protected:
  virtual ~nsMacPreferencesReader() = default;
};

#endif  // MacPreferencesReader_h__
