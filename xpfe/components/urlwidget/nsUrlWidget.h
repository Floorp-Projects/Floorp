/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsurlwidget_h___
#define nsurlwidget_h___

#include "nsIUrlWidget.h"

// {1802EE82-34A1-11d4-82EE-0050DA2DA771}
#define NS_IURLWIDGET_CID { 0x1802EE82, 0x34A1, 0x11d4, { 0x82, 0xEE, 0x00, 0x50, 0xDA, 0x2D, 0xA7, 0x71 } }

// nsUrlWidget declaration
class nsUrlWidget : public nsIUrlWidget {
public:
    nsUrlWidget();
    virtual ~nsUrlWidget();
    nsresult Init();
	
    // Declare all interface methods we must implement.
    NS_DECL_ISUPPORTS
    NS_DECL_NSIURLWIDGET
};
#endif // nsurlwidget_h___
