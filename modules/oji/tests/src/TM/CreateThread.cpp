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
#include <ThreadManagerTests.h>
//CreateThread

TM_OJIAPITest(ThreadManager_CreateThread_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->CreateThread(NULL, NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no nsIRunnable object is specified.");
	return TestResult::FAIL("CreateThread", rc);

}

TM_OJIAPITest(ThreadManager_CreateThread_2) {
	GET_TM_FOR_TEST
	PRUint32 id;

	nsresult rc = threadMgr->CreateThread(&id, NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no nsIRunnable object is specified.");
	return TestResult::FAIL("CreateThread", rc);

}

TM_OJIAPITest(ThreadManager_CreateThread_3) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		DummyThread() {}
		NS_METHOD Run() { while(1); return NS_OK; }
	};

	DummyThread *dt = new DummyThread();
	nsresult rc = threadMgr->CreateThread(NULL, (nsIRunnable*)dt);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because no space is allocated for thread id.");
	return TestResult::FAIL("CreateThread", rc);
	
}

TM_OJIAPITest(ThreadManager_CreateThread_4) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		DummyThread() {}
		NS_METHOD Run() { while(1); return NS_OK; }
	};
	PRUint32 id = 0;
	DummyThread *dt = new DummyThread();
	nsresult rc = threadMgr->CreateThread(&id, (nsIRunnable*)dt);	
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should work OK.");
	return TestResult::FAIL("");

}
