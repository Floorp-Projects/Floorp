/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Christopher Blizzard
 * <blizzard@mozilla.org>.  Portions created by Christopher Blizzard
 * are Copyright (C) 1998 Christopher Blizzard. All Rights Reserved.
 *
 * Contributor(s):
 * Christopher Blizzard <blizzard@mozilla.org>
 *   Peter Hartshorn <peter@igelaus.com.au>
*/

#ifndef nsIDragSessionXLIB_h_
#define nsIDragSessionXLIB_h_

#include "nsISupports.h"
#include "nsIWidget.h"

#define NS_IDRAGSESSIONXLIB_IID \
{ 0xa6b49c42, 0x1dd1, 0x11b2, { 0xb2, 0xdf, 0xc1, 0xd6, 0x1d, 0x67, 0x45, 0xcf } };

class nsIDragSessionXlib : public nsISupports {
 public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDRAGSESSIONXLIB_IID)

  NS_IMETHOD IsDragging(PRBool *result) = 0;
  NS_IMETHOD UpdatePosition(PRInt32 x, PRInt32 y) = 0;
};

#endif /* nsIDragSessionXLIB_h_ */
