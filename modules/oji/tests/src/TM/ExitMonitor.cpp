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
//ExitMonitor
TM_OJIAPITest(ThreadManager_ExitMonitor_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->ExitMonitor(NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because adress is invalid (NULL).");
	return TestResult::FAIL("ExitMonitor", rc);
}

TM_OJIAPITest(ThreadManager_ExitMonitor_2) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->ExitMonitor(threadMgr);
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Thread can exit monitor even if it doesn't own it.");
	return TestResult::FAIL("ExitMonitor", rc);

}

TM_OJIAPITest(ThreadManager_ExitMonitor_3) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc)) {
		rc = threadMgr->ExitMonitor(threadMgr);
		if (NS_SUCCEEDED(rc))
			return TestResult::PASS("Method should work OK.");
		return TestResult::FAIL("ExitMonitor", rc);
	}
	return TestResult::FAIL("ExitMonitor", "Can't enter monitor", rc);

}

TM_OJIAPITest(ThreadManager_ExitMonitor_4) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		DummyThread(nsIThreadManager *threadMgr, nsresult def_rc) {
			tm = threadMgr;
			rc = def_rc;
		} 
		NS_METHOD Run() { 
			//printf("New thread enters monitor with adress %p ... ", tm); 
			tm->EnterMonitor(tm); 
			rc = NS_OK;
			//printf("OK - new thread should be able to enter monitor !\n"); 
			while(1); 
			return NS_OK;
		}
	};
	PRUint32 id = 0;
	DummyThread *newThread = new DummyThread(threadMgr, NS_ERROR_FAILURE);

	//printf("Our thread enters monitor with adress %p (%d) ...\n", threadMgr, id);
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc)) {
		rc = threadMgr->CreateThread(&id, (nsIRunnable*)newThread);
		if (NS_SUCCEEDED(rc))  {
			//let's give new thread a second to call EnterMonitor method
			threadMgr->Sleep(500);
			rc = threadMgr->ExitMonitor(threadMgr);
			if (NS_SUCCEEDED(rc)) {
				threadMgr->Sleep(500);
				rc = newThread->rc;
				if (NS_SUCCEEDED(rc))
					return TestResult::PASS("Another thread CAN enter monitor if the current thread unlock it.");
				return TestResult::FAIL("ExitMonitor", rc);
			}
			//printf("First thread can't exit monitor ...\n");
			return TestResult::FAIL("ExitMonitor", "First thread can't exit monitor", rc);
		}
		//printf("Can't create new thred !\n");		
		return TestResult::FAIL("ExitMonitor", "Can't create new thread", rc);

	}
	//printf("First thread can't enter monitor ...\n");
	return TestResult::FAIL("ExitMonitor", "First thread can't enter monitor", rc);
}



