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

//Wait

TM_OJIAPITest(ThreadManager_Wait_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->Wait(NULL, (PRUint32)100);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because invalid adress (NULL) is specified.");
	return TestResult::FAIL("Wait", rc);

}

TM_OJIAPITest(ThreadManager_Wait_2) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		DummyThread(nsIThreadManager *aTM) { rc = NS_OK; tm = aTM; }
		NS_METHOD Run() {
			tm->EnterMonitor(tm);
			tm->Wait(tm, (PRUint32)UINT_MAX);
			rc = NS_ERROR_FAILURE; //we shoudn't ever get here
			tm->ExitMonitor(tm);
			return NS_OK; 
		}
	};
	PRUint32 id = 0;
	nsresult rc;

	DummyThread *dt = new DummyThread(threadMgr);
	rc = threadMgr->CreateThread(&id, (nsIRunnable*)dt);
	threadMgr->Sleep(500); 
	rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_FAILED(rc))
		return TestResult::FAIL("Wait","Can't eneter monitor", rc);
	threadMgr->Sleep(500);
	if (NS_SUCCEEDED(dt->rc))
		return TestResult::PASS("Method should work OK.");
	rc = threadMgr->ExitMonitor(threadMgr);
	if (NS_FAILED(rc))
		return TestResult::FAIL("Wait","Can't exit monitor", rc);
	return TestResult::FAIL("Wait", dt->rc);
}

TM_OJIAPITest(ThreadManager_Wait_3) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		DummyThread(nsIThreadManager *aTM) { rc = NS_OK; tm = aTM; }
		NS_METHOD Run() {
			tm->EnterMonitor(tm);
			tm->Wait(tm, (PRUint32)0);
			rc = NS_ERROR_FAILURE; //we shoudn't ever get here
			tm->ExitMonitor(tm);
			return NS_OK; 
		}
	};
	PRUint32 id = 0;
	nsresult rc;

	DummyThread *dt = new DummyThread(threadMgr);
	rc = threadMgr->CreateThread(&id, (nsIRunnable*)dt);
	threadMgr->Sleep(500); 
	rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_FAILED(rc))
		return TestResult::FAIL("Wait","Can't eneter monitor", rc);
	threadMgr->Sleep(500);
	if (NS_SUCCEEDED(dt->rc))
		return TestResult::PASS("Method should work OK.");
	rc = threadMgr->ExitMonitor(threadMgr);
	if (NS_FAILED(rc))
		return TestResult::FAIL("Wait","Can't exit monitor", rc);
	return TestResult::FAIL("Wait", dt->rc);
}

TM_OJIAPITest(ThreadManager_Wait_4) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->Wait(threadMgr, (PRUint32)100);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because current thread doesn't own monitor.");
	return TestResult::FAIL("Wait", rc);

}



TM_OJIAPITest(ThreadManager_Wait_5) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		int notified;
		DummyThread(nsIThreadManager *threadMgr, nsresult def_rc) : notified(0) {
			tm = threadMgr;
			rc = def_rc; 
		}
		NS_METHOD Run() { 
			nsresult lrc = tm->EnterMonitor(tm);
			if (NS_SUCCEEDED(lrc))  {
				lrc = tm->Wait(tm);
				//if first thread notified it - nsresult rc =ing OK
				this->rc = NS_ERROR_FAILURE;
			}
			while(1);
			return NS_OK;
		}
	};
	PRUint32 id = 0;
	DummyThread *newThread = new DummyThread(threadMgr, NS_OK);

	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc)) {
		rc  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread);
		if (NS_SUCCEEDED(rc)) {
			threadMgr->Sleep((PRUint32)500);
			if (NS_SUCCEEDED(newThread->rc))
				return TestResult::PASS("Method should work OK.");
			return TestResult::FAIL("Wait", rc);
		} else {
			return TestResult::FAIL("Wait", "Can't create new thread", rc);
		}
	}
	return TestResult::FAIL("Wait", "Can't enter moniotor", rc);
}
