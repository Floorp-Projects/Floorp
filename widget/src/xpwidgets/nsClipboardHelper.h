/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corp.  Portions created by Netscape are Copyright (C) 2001 Netscape
 * Communications Corp. All Rights Reserved.
 *
 * Original Author:
 *   Dan Rosen <dr@netscape.com>
 *
 * Contributor(s):
 *
 */

#ifndef nsClipboardHelper_h__
#define nsClipboardHelper_h__

// interfaces
#include "nsIClipboardHelper.h"

// basics
#include "nsString.h"

/**
 * impl class for nsIClipboardHelper, a helper for common uses of nsIClipboard.
 */

class nsClipboardHelper : public nsIClipboardHelper
{

public:

  NS_DECL_ISUPPORTS
  NS_DECL_NSICLIPBOARDHELPER

  nsClipboardHelper();
  virtual ~nsClipboardHelper();

};

#endif // nsClipboardHelper_h__
