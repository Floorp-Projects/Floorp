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
#include <LiveConnectManagerTests.h>
/*ShutdownLiveConnect*/

LCM_OJIAPITest(LCM_ShutdownLiveConnect_1) { 
	GET_LCM_FOR_TEST
	PRBool *out = nsnull;
	nsresult rc = lcMgr->ShutdownLiveConnect(nsnull, *out);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no memory is allocated for the result value (PRBool *out = nsnull;)");
	return TestResult::FAIL("ShutdownLiveConnect", rc);

}

LCM_OJIAPITest( LCM_ShutdownLiveConnect_2) { 
	GET_LCM_FOR_TEST
	PRBool out;
	nsresult rc = lcMgr->ShutdownLiveConnect(nsnull, out);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because first parameter is incorrect (NULL).");
	return TestResult::FAIL("ShutdownLiveConnect", rc);

}

