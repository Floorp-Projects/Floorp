/*
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

#pragma once

#include "nsISecurityContext.h"

class nsIURI;
class nsILiveconnect;

class MRJSecurityContext : public nsISecurityContext {
public:
	MRJSecurityContext(const char* location);
	~MRJSecurityContext();

	NS_DECL_ISUPPORTS
    
	NS_IMETHOD Implies(const char* target, const char* action, PRBool *bAllowedAccess);
    NS_IMETHOD GetOrigin(char* buf, int len);
    NS_IMETHOD GetCertificateID(char* buf, int len);

    nsILiveconnect* getConnection() { return mConnection; }

private:
    nsIURI* mLocation;
    nsILiveconnect* mConnection;
};
