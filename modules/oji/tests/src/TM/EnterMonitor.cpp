/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include <ThreadManagerTests.h>
/*EnterMonitor*/

TM_OJIAPITest(ThreadManager_EnterMonitor_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->EnterMonitor(NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method shoulf fail because invalid (NULL) adress is specified.");
	return TestResult::FAIL("EnterMonitor", rc);

}

TM_OJIAPITest(ThreadManager_EnterMonitor_2) {
	GET_TM_FOR_TEST
	//printf("Entering monitor with adress %p\n", threadMgr);
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Current thread can enter monitor even if no other thread we created.");
	return TestResult::FAIL("EnterMonitor", rc);

}

TM_OJIAPITest(ThreadManager_EnterMonitor_3) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	printf("Entering monitor with adress %p\n", threadMgr);
	if (NS_SUCCEEDED(rc)) {
		printf("Entering monitor again with adress %p\n", threadMgr);
		nsresult rc = threadMgr->EnterMonitor(threadMgr);
		if (NS_SUCCEEDED(rc))
			return TestResult::PASS("One thread can enter monitor twice.");
		return TestResult::FAIL("EnterMonitor", rc);
	}
	return TestResult::FAIL("EnterMonitor", "Can't enter monitor at all", rc);

}


TM_OJIAPITest(ThreadManager_EnterMonitor_4) {
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
			rc = NS_ERROR_FAILURE;
			//printf("Error - new thread should wait while first thread exit monitor !\n"); 
			while(1); 
			return NS_OK;
		}
	};
	PRUint32 id = 0;
	DummyThread *newThread = new DummyThread(threadMgr, NS_OK);

	printf("Our thread enters monitor with adress %p (%d) ...\n", threadMgr, id);
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc)) {
		rc = threadMgr->CreateThread(&id, (nsIRunnable*)newThread);
		if (NS_SUCCEEDED(rc))  {
			//let's give new thread a second to call EnterMonitor method
			threadMgr->Sleep(1000);
			rc = newThread->rc;
			if (NS_SUCCEEDED(rc))
				return TestResult::PASS("Another thread can't enter monitor locked by current thread.");
			return TestResult::FAIL("EnterMonitor", rc);

		}
		return TestResult::FAIL("EnterMonitor", "Can't create new thread", rc);

	}
	printf("First thread can't enter monitor ...\n");
	return TestResult::FAIL("EnterMonitor", "First thread can't enter monitor", rc);

}

