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

/*Sleep*/

TM_OJIAPITest(ThreadManager_Sleep_1) {
	GET_TM_FOR_TEST
	PRUint32 id = 0;
	class DummyThread : public BaseDummyThread {
	private:
		nsIThreadManager *tm;
	public:
		int awoken;
		DummyThread(nsIThreadManager *aTm) : tm(aTm), awoken(0) {}
		NS_METHOD Run() {
			tm->Sleep(0);
			awoken = 1;
			return NS_OK;
		}
	};

	DummyThread *newThread = new DummyThread(threadMgr);
	nsresult rc  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread);
	if (NS_SUCCEEDED(rc)) {
		rc = threadMgr->Sleep(500);        
		if (NS_SUCCEEDED(rc) && newThread->awoken) //if it waked up -> pass
			return TestResult::PASS("Method should work OK.");
		return TestResult::FAIL("Sleep", rc);
	} 
	return TestResult::FAIL("Sleep", "Can't create new thread", rc);

}



TM_OJIAPITest(ThreadManager_Sleep_2) {
	GET_TM_FOR_TEST
	PRUint32 id = 0;
	class DummyThread : public BaseDummyThread {
	private:
		nsIThreadManager *tm;
	public:
		int isSleeping;
		DummyThread(nsIThreadManager *aTm) : tm(aTm), isSleeping(1) {} //UINT_MAX = 32-bit uint max value 
		NS_METHOD Run() {
			tm->Sleep(UINT_MAX);
			isSleeping = 0;
			return NS_OK;
		}
	};

	DummyThread *newThread = new DummyThread(threadMgr);
	nsresult rc  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread);
	if (NS_SUCCEEDED(rc)) {
		rc = threadMgr->Sleep(500);        
		if (NS_SUCCEEDED(rc) && newThread->isSleeping)
			return TestResult::PASS("Method should work OK.");
		return TestResult::FAIL("Sleep", rc);
	} 
	return TestResult::FAIL("Sleep", "Can't create new thread", rc);

}
