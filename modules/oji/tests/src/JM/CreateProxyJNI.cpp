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

#include "CreateProxyJNI.h"
/*CreateProxyJNI*/

//1-st parameter is NULL
JM_OJIAPITest(JVMManager_CreateProxyJNI_1) {
	GET_JM_FOR_TEST
	JNIEnv *jniEnv;

	nsresult rc = jvmMgr->CreateProxyJNI(nsnull, &jniEnv);	
	if (NS_SUCCEEDED(rc) && jniEnv)
		return TestResult::PASS("First paramenter CAN be NULL.");
	return TestResult::FAIL("CreateProxyJNI", rc);
}

//both parameters are NULL
JM_OJIAPITest(JVMManager_CreateProxyJNI_2) {
	GET_JM_FOR_TEST
	//JNIEnv **jniEnv = nsnull;

	nsresult rc = jvmMgr->CreateProxyJNI(nsnull, nsnull);	
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no space is allocated for the result pointer.");
	return TestResult::FAIL("CreateProxyJNI", rc);

}


//2-nd parameter is NULL
JM_OJIAPITest(JVMManager_CreateProxyJNI_3) {
	GET_JM_FOR_TEST
	//JNIEnv **jniEnv = nsnull;
	nsISecureEnv *secureEnv = new nsDummySecureEnv();

	nsresult rc = jvmMgr->CreateProxyJNI(secureEnv, nsnull);	
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no space is allocated for the result pointer.");
	return TestResult::FAIL("CreateProxyJNI", rc);
}


//both parameters !=NULL
JM_OJIAPITest(JVMManager_CreateProxyJNI_4) {
	GET_JM_FOR_TEST
	JNIEnv *jniEnv;
	nsISecureEnv *secureEnv = new nsDummySecureEnv();

	nsresult rc = jvmMgr->CreateProxyJNI(secureEnv, &jniEnv);	
	if (NS_SUCCEEDED(rc) && jniEnv)
		return TestResult::PASS("Method should work OK.");
	return TestResult::FAIL("CreateProxyJNI", rc);
}



 
