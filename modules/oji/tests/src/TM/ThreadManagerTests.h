/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifdef XP_WIN
#include <windows.h>
#endif
#include "ojiapitests.h"

#include <nsIThreadManager.h>
#include <nsIJVMManager.h>

static NS_DEFINE_IID(kIThreadManagerIID, NS_ITHREADMANAGER_IID);
static NS_DEFINE_IID(kJVMManagerCID, NS_JVMMANAGER_CID);

extern nsresult GetThreadManager(nsIThreadManager** tm);

class BaseDummyThread : public nsIRunnable {
protected:
	nsIThreadManager *tm;
public:
	nsresult rc;	
	NS_METHOD QueryInterface(const nsIID & uuid, void * *result) { return NS_OK; }
	NS_METHOD_(nsrefcnt) AddRef(void) { return NS_OK; }
	NS_METHOD_(nsrefcnt) Release(void) { return NS_OK; }
	NS_IMETHOD Run() = 0;
};
