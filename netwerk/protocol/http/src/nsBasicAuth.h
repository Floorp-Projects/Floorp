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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Darin Fisher <darin@netscape.com>
 */

#ifndef nsBasicAuth_h__
#define nsBasicAuth_h__

#include "nsIAuthenticator.h"

class nsIURI;

/**
 * The nsBasicAuth class produces HTTP Basic-auth responses for a username/
 * (optional)password pair, BASE64("user:pass").
 */
class nsBasicAuth : public nsIAuthenticator
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIAUTHENTICATOR

	nsBasicAuth();
	virtual ~nsBasicAuth();
};

#endif
