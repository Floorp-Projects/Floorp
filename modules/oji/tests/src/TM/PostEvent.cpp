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

#include <nsIEventQueueService.h>
#include <nsIServiceManager.h>

#include <limits.h>


//PostEvent
extern nsIThreadManager* GetThreadManager();

TM_OJIAPITest(ThreadManager_PostEvent_1) {
	GET_TM_FOR_TEST
	PRUint32 *id = NULL; 
	nsresult rc = threadMgr->PostEvent(*id, NULL, (PRBool)0);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because thread's id is specified incorrectly (PRUint32 *id = NULL;)");
	return TestResult::FAIL("PostEvent", rc);

}

TM_OJIAPITest(ThreadManager_PostEvent_2) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:		
		DummyThread() {}
		NS_METHOD Run() { while(1); return NS_OK; }
	}; 
	PRUint32 id;
	DummyThread *dt = new DummyThread();
	nsresult rc = threadMgr->CreateThread(&id, dt);
		
	if (NS_SUCCEEDED(rc)) {
		nsresult rc = threadMgr->PostEvent(id, NULL, (PRBool)0);
		if (NS_FAILED(rc))
			return TestResult::PASS("Method should fail because no object to be invoked from thread is specified.");
		return TestResult::FAIL("PostEvent", rc);
	}
	return TestResult::FAIL("PostEvent", "Can't create new thread", rc);

}

TM_OJIAPITest(ThreadManager_PostEvent_3) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:		
		DummyThread() {}
		NS_METHOD Run() { while(1); return NS_OK; }
	}; 
	PRUint32 id = 0;
	DummyThread *dt = new DummyThread();
	nsresult rc = threadMgr->CreateThread(&id, dt);
		
	if (NS_SUCCEEDED(rc)) {
		nsresult rc = threadMgr->PostEvent(id, NULL, (PRBool)1);
		if (NS_FAILED(rc))
			return TestResult::PASS("Method should fail because no object to be invoked from thread is specified.");
		return TestResult::FAIL("PostEvent", rc);
	}
	return TestResult::FAIL("PostEvent", "Can't create new thread", rc);

}

TM_OJIAPITest(ThreadManager_PostEvent_5) {
	GET_TM_FOR_TEST
	class REvent : public BaseDummyThread {
	public:		
		REvent() {}
		NS_METHOD Run() { return rc = NS_OK; }
	}; 
	REvent *re = new REvent();
	PRThread *pt;
		
	nsresult rc = threadMgr->PostEvent((PRUint32)pt, re, (PRBool)0);
	if (NS_FAILED(rc))
		return TestResult::PASS("Method should fail because thtread doesn't exist (PRThread *pt;)");
	return TestResult::FAIL("PostEvent", rc);

}

TM_OJIAPITest(ThreadManager_PostEvent_6) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		PRUint32 id;
	public:		
		DummyThread(nsIThreadManager *atm, nsresult def_rc) : id(0) {
			rc = def_rc;
			tm = atm;
		}
		NS_METHOD Run() { 
			nsresult rv, lrc, lrc1;
			static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
			static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
			nsCOMPtr<nsIEventQueueService> eventService = 
			         do_GetService(kEventQueueServiceCID, &rv);
			if (NS_FAILED(rv)) nsresult rc = rv;
			nsIEventQueue *nq;

			nsCOMPtr<nsIEventQueue> eventQueue = NULL;
			
			printf("Waiting for thread ID (%p) ...", &id);
			tm->EnterMonitor(&id);
			tm->ExitMonitor(&id);
			printf("got : %d \n", id);
			lrc = eventService->GetThreadEventQueue((PRThread*)id, getter_AddRefs(eventQueue)); 

			if (lrc != NS_OK || !eventQueue) {
				printf("Failed to get event queue !\n");
				nsresult rc = NS_ERROR_FAILURE;
			}

			PRBool eventAvail = (PRBool)0;
			PLEvent *event = NULL;				
			printf("Try to get an event ... (%d =?)\n", id);
			while(eventQueue->EventAvailable(eventAvail) == NS_OK && eventAvail == PR_FALSE) { }; 
			printf("Event is available - let's handle it !\n");
			eventQueue->GetEvent(&event);
			eventQueue->HandleEvent(event);
			return rc = NS_OK;
		}
	};

	class REvent : public BaseDummyThread {
	private:
		nsresult *threadRc;
	public:
		REvent(nsresult *rc) : threadRc(rc) {}
		NS_METHOD Run() { 
			*threadRc = NS_OK;
			printf("Event was handled by thread !\n"); 
			return rc = NS_OK; 
		}
	};

	PRUint32 id = 0;
	DummyThread *dt = new DummyThread(threadMgr, NS_ERROR_FAILURE);
	REvent *rEvent = new REvent(&(dt->rc));
	threadMgr->EnterMonitor(&(dt->id));
	nsresult rc = threadMgr->CreateThread(&id, (nsIRunnable*)dt);

	dt->id = id;
	printf("Notifying that id is set (%p => %d) ...\n", &(dt->id), id);
	threadMgr->ExitMonitor(&(dt->id));

	if (NS_SUCCEEDED(rc)) {
		//let's give time to new thread to create event queue
		threadMgr->Sleep((PRUint32)500);
		//time to handle the evemt
		rc = threadMgr->PostEvent(id, rEvent, (PRBool)1);		
		threadMgr->Sleep((PRUint32)500);
		if (NS_SUCCEEDED(rc) && NS_SUCCEEDED(dt->rc))
			return TestResult::PASS("Method should work OK.");
		return TestResult::FAIL("PostEvent", rc);
	}
	//printf("Can't create new thread !\n");
	return TestResult::FAIL("PostEvent", "Can't create new thread", rc);
}                               


TM_OJIAPITest(ThreadManager_PostEvent_7) {
	GET_TM_FOR_TEST
	class DummyThread : public BaseDummyThread {
	public:
		PRUint32 id;
	public:		
		DummyThread(nsIThreadManager *atm, nsresult def_rc) : id(0) {
			rc = def_rc;
			tm = atm;
		}
		NS_METHOD Run() { 
			nsresult rv, lrc, lrc1;
			static NS_DEFINE_IID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);
			static NS_DEFINE_IID(kIEventQueueServiceIID, NS_IEVENTQUEUESERVICE_IID);
			nsCOMPtr<nsIEventQueueService> eventService = 
			         do_GetService(kEventQueueServiceCID, &rv);
			if (NS_FAILED(rv)) nsresult rc = rv;
			nsIEventQueue *nq;

			nsCOMPtr<nsIEventQueue> eventQueue = NULL;
			
			//tm->EnterMonitor(&id);
			printf("Waiting for thread ID (%p) ...", &id);
			while(!id);
			//tm->Wait(&id);
			printf("got : %d \n", id);
			lrc = eventService->GetThreadEventQueue((PRThread*)id, getter_AddRefs(eventQueue)); 

			if (lrc != NS_OK || !eventQueue) {
				printf("Failed to get event queue !\n");
				nsresult rc = NS_ERROR_FAILURE;
			}

			PRBool eventAvail = (PRBool)0;
			PLEvent *event = NULL;				
			printf("Try to get an event ... (%d =?)\n", id);
			while(eventQueue->EventAvailable(eventAvail) == NS_OK && eventAvail == PR_FALSE) { tm->Sleep(3000); } 
			printf("Event is available - let's handle it !\n");
			eventQueue->GetEvent(&event);
			eventQueue->HandleEvent(event);
			return rc = NS_OK;
		}
	};

	class REvent : public BaseDummyThread {
	private:
		nsresult *threadRc;
	public:
		REvent(nsresult *rc) : threadRc(rc) {}
		NS_METHOD Run() { 
			*threadRc = NS_OK;
			printf("Event was handled by thread !\n"); 
			return rc = NS_OK; 
		}
	};

	PRUint32 id = 0;
	DummyThread *dt = new DummyThread(threadMgr, NS_ERROR_FAILURE);
	REvent *rEvent = new REvent(&(dt->rc));
	threadMgr->EnterMonitor(&(dt->id));
	nsresult rc = threadMgr->CreateThread(&id, (nsIRunnable*)dt);

	dt->id = id;
	printf("Notifying that id is set (%p => %d) ...\n", &(dt->id), id);
	threadMgr->NotifyAll(&(dt->id));
	threadMgr->ExitMonitor(&(dt->id));

	if (NS_SUCCEEDED(rc)) {
		//let's give time to new thread to create event queue
		threadMgr->Sleep((PRUint32)500);
		//time to handle the evemt
		rc = threadMgr->PostEvent(id, rEvent, (PRBool)0);		
		threadMgr->Sleep((PRUint32)500);
		if (NS_SUCCEEDED(rc) && NS_SUCCEEDED(dt->rc))
			return TestResult::PASS("Method should work OK.");
		//printf("Event wasn't handled by thread (so we didn't wait for result) !\n");
		return TestResult::FAIL("PostEvent", "Event wasn't handled by thread (so we didn't wait for result)", rc);
	}
	printf("Can't create new thread !\n");
	return TestResult::FAIL("PostEvent", "Can't create new thread", rc);
	
}                               


 
