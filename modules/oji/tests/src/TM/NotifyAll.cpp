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
//NotifyAll
TM_OJIAPITest(ThreadManager_NotifyAll_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->NotifyAll(NULL);
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should fail because adress is invalid (NULL)");
	return TestResult::FAIL("NotifyAll", rc);
}

TM_OJIAPITest(ThreadManager_NotifyAll_2) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->NotifyAll(threadMgr);
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should fail because it can call Notify only being monitor's owner.");
	return TestResult::FAIL("NotifyAll", rc);

}


TM_OJIAPITest(ThreadManager_NotifyAll_3) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		int notified;
		DummyThread *otherThread;
		DummyThread(nsIThreadManager *threadMgr, nsresult def_rc) : notified(0) {
			tm = threadMgr;
			rc = def_rc; 
		}
		NS_METHOD Run() { 
			nsresult lrc = tm->EnterMonitor(tm);
			printf("Thread %p waiting for monitor %p\n", this, tm);
			lrc = tm->Wait(tm);
			if (NS_SUCCEEDED(lrc))  {
				printf("Thread %p: after wait -> notified: %d\n", this, notified);
				//if first thread notified it - nsresult rc =ing OK
				if (notified == 1) {  
					this->rc = NS_OK;
				}
				tm->ExitMonitor(tm);
				//other thread should get here without any notifications
			} else {
				printf("ERROR: thread %p: Wait method returned error (%d)\n", this, lrc);
			}
			while(1);
			return NS_OK;
		}
	};

	PRUint32 id = 0;
	DummyThread *newThread1 = new DummyThread(threadMgr, NS_ERROR_FAILURE);
	DummyThread *newThread2 = new DummyThread(threadMgr, NS_ERROR_FAILURE);
	nsresult rc1  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread1);
	nsresult rc2  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread2);

	newThread1->otherThread = newThread2;
	newThread2->otherThread = newThread1;
	//time to enter monitor and say wait
	threadMgr->Sleep((PRUint32)500);
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	if (NS_SUCCEEDED(rc)) {
		if (NS_SUCCEEDED(rc)) {
			newThread1->notified = 1;
			newThread2->notified = 1;		
			rc = threadMgr->NotifyAll(threadMgr);
			threadMgr->ExitMonitor(threadMgr);
			if (NS_SUCCEEDED(rc)) {
				//time to check notified flag and set rc to NS_OK
				threadMgr->Sleep((PRUint32)500);
				if (NS_SUCCEEDED(newThread1->rc) && NS_SUCCEEDED(newThread2->rc))
					return TestResult::PASS("Method should work OK.");
				return TestResult::FAIL("Not all threads were notified.");
			} else {
				//can't call Notify method
				return TestResult::FAIL("NotifyAll", rc);
			}
		} else {
			return TestResult::FAIL("NotifyAll", "Main thread can't exit monitor", rc);
		}
	} else {
		//can't create new threds
		return TestResult::FAIL("NotifyAll", "Can't create new threds", rc);
	}
	//can't enter monitor
	return TestResult::FAIL("NotifyAll", "Can't enter monitor", rc);
	
}

TM_OJIAPITest(ThreadManager_NotifyAll_4) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	
	if (NS_SUCCEEDED(rc)) {
		nsresult rc = threadMgr->NotifyAll(threadMgr);
		if (NS_SUCCEEDED(rc))
			return TestResult::PASS("Method should work even if there are no other threads waiting on monitor.");
		return TestResult::FAIL("NotifyAll", rc);
	}
	return TestResult::FAIL("NotifyAll", "Can't enter monitor", rc);

}



