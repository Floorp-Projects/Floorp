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
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *   Stuart Parmenter <pavlov@netscape.com>
 *   Mike Pinkerton   <pinkerton@netscape.com>
 */

#ifndef nsBaseFilePicker_h__
#define nsBaseFilePicker_h__

#include "nsIFilePicker.h"
#include "nsIWidget.h"
#include "nsISimpleEnumerator.h"

class nsBaseFilePicker : public nsIFilePicker
{
public:
  nsBaseFilePicker(); 
  virtual ~nsBaseFilePicker();

  NS_IMETHOD Init(nsIDOMWindowInternal *aParent,
                  const PRUnichar *aTitle,
                  PRInt16 aMode);

  NS_IMETHOD AppendFilters(PRInt32 filterMask);
  NS_IMETHOD GetFilterIndex(PRInt32 *aFilterIndex);
  NS_IMETHOD SetFilterIndex(PRInt32 aFilterIndex);
  NS_IMETHOD GetFiles(nsISimpleEnumerator **aFiles);

protected:

  NS_IMETHOD InitNative(nsIWidget *aParent, const PRUnichar *aTitle, PRInt16 aMode) = 0;

private:

  NS_IMETHOD DOMWindowToWidget(nsIDOMWindowInternal *dw, nsIWidget **aResult);
};

#endif // nsBaseFilePicker_h__
