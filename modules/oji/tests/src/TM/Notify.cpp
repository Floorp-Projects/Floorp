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

//NULL adress
TM_OJIAPITest(ThreadManager_Notify_1) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->Notify(NULL);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because adress is invalid (NULL)");
	return TestResult::FAIL("Notify", rc);

}

//monitor isn't entered by this thread
TM_OJIAPITest(ThreadManager_Notify_2) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->Notify(threadMgr);
	if (NS_SUCCEEDED(rc))
		return TestResult::PASS("Method should fail because it can call Notify only being monitor's owner.");
	return TestResult::FAIL("Notify", rc);

}

//monitor is entered by this thread and there are waiting threads
TM_OJIAPITest(ThreadManager_Notify_3) {
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
				//printf("Thread %p is waiting for notify ...\n", this);
				lrc = tm->Wait(tm);
				//if first thread notified it - nsresult rc =ing OK
				if (NS_SUCCEEDED(lrc) && notified)
					this->rc = NS_OK;
				tm->ExitMonitor(tm);
			} else {
				fprintf(stderr, "ERROR: new thread %p can't create monitor for adress %p\n", this, tm);
			}
			while(1);
			return NS_OK;
		}
	};
	PRUint32 id = 0;
	DummyThread *newThread1 = new DummyThread(threadMgr, NS_ERROR_FAILURE);

	nsresult rc  = threadMgr->CreateThread(&id, (nsIRunnable*)newThread1);
	if (NS_SUCCEEDED(rc)) {
		//give time to new thread to enter monitor and say wait
		threadMgr->Sleep((PRUint32)500);
		rc = threadMgr->EnterMonitor(threadMgr);
		if (NS_SUCCEEDED(rc)) {
			newThread1->notified = 1;
			threadMgr->Notify(threadMgr);
			if (NS_SUCCEEDED(rc)) {
				rc = threadMgr->ExitMonitor(threadMgr);
				if (NS_SUCCEEDED(rc)) {
					//time to check notified flag and set rc to NS_OK
					threadMgr->Sleep((PRUint32)500);
					if (NS_SUCCEEDED(newThread1->rc))
						return TestResult::PASS("Method should work OK.");
					return TestResult::FAIL("Notify", rc);		
				} else {
					return TestResult::FAIL("Notify", "Can't exit monitor", rc);
				}
			} else {
				//can't call Notify method
				return TestResult::FAIL("Notify", rc);
			}
		} else {
			return TestResult::FAIL("Notify", "Can't enter monitor", rc);
		}
	}
	//can't create new threds
	return TestResult::FAIL("Notify", "Can't create new threads", rc);
}


//thread entered monitor but no waiting threads
TM_OJIAPITest(ThreadManager_Notify_4) {
	GET_TM_FOR_TEST
	nsresult rc = threadMgr->EnterMonitor(threadMgr);
	
	if (NS_SUCCEEDED(rc)) {
		nsresult rc = threadMgr->Notify(threadMgr);
		if (NS_SUCCEEDED(rc))
			return TestResult::PASS("Method should work even if there are no other threads waiting on monitor.");
		return TestResult::FAIL("Notify", rc);
	}
	return TestResult::FAIL("Notify", "Can't enter monitor", rc);
}




