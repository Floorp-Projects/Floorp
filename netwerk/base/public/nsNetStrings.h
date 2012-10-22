/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsNetStrings_h__
#define nsNetStrings_h__

#include "nsLiteralString.h"

/**
 * Class on which wide strings are available, to avoid constructing strings
 * wherever these strings are used.
 */
class nsNetStrings {
public:
  nsNetStrings();

  const nsLiteralString kChannelPolicy;
};

extern NS_HIDDEN_(nsNetStrings*) gNetStrings;


#endif
