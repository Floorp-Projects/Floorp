/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsParserBase_h_
#define nsParserBase_h_

#include "nsIChannel.h"

class nsParserBase : public nsISupports {
 public:
  NS_IMETHOD_(bool) IsParserEnabled() { return true; }
  NS_IMETHOD_(bool) IsParserClosed() { return false; }
};

#endif  // nsParserBase_h_
