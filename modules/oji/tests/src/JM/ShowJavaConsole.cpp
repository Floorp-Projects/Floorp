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
#include "JVMManagerTests.h"


/*ShowJavaConsole*/

JM_OJIAPITest(JVMManager_ShowJavaConsole_1) {
	GET_JM_FOR_TEST
	JNIEnv *jniEnv;

	jvmMgr->CreateProxyJNI(nsnull, &jniEnv);	
	nsresult rc = jvmMgr->ShowJavaConsole();
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("CreateProxyJNI is called before ShowJavaConsole.");
	return TestResult::FAIL("ShowJavaConsole", rc);

}

JM_OJIAPITest(JVMManager_ShowJavaConsole_2) {
	GET_JM_FOR_TEST
	JNIEnv *jniEnv;

	nsresult rc = jvmMgr->ShowJavaConsole();
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should PASS nevertheless CreateProxyJNI wasn't call before.");
	return TestResult::FAIL("ShowJavaConsole", rc);

}




 
