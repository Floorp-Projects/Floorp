/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsXPFCObserverManager.h"
#include "nsIXPFCObserver.h"
#include "nsIXPFCSubject.h"
#include "nsIXPFCCommand.h"
#include "nsxpfcCIID.h"
#include "nsXPFCNotificationStateCommand.h"

static NS_DEFINE_IID(kISupportsIID,             NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCObserverManagerIID,   NS_IXPFC_OBSERVERMANAGER_IID);
static NS_DEFINE_IID(kCXPFCObserverIID,         NS_IXPFC_OBSERVER_IID);
static NS_DEFINE_IID(kCXPFCSubjectIID,          NS_IXPFC_SUBJECT_IID);

class ListEntry {
public:
  nsIXPFCSubject * subject;
  nsIXPFCObserver * observer;
  PRUint32 count;

  ListEntry(nsIXPFCSubject * aSubject, 
            nsIXPFCObserver * aObserver) { 
    subject = aSubject;
    observer = aObserver;
    count = 1;
  }
  ~ListEntry() {
  }
};

class StateEntry {
public:
  nsCommandState state;
  nsIXPFCObserver * observer;
  PRBool update_when_complete;

  StateEntry(nsCommandState aState, 
            nsIXPFCObserver * aObserver) { 
    state = aState;
    observer = aObserver;
    update_when_complete = PR_FALSE;
  }
  ~StateEntry() {
  }
};


nsXPFCObserverManager :: nsXPFCObserverManager()
{
  NS_INIT_REFCNT();

  mList = nsnull;
  mState = nsnull;
  monitor = nsnull;
  mNotificationCount = 0;
  mOriginalNotifier = nsnull;

  Init();
}

nsXPFCObserverManager :: ~nsXPFCObserverManager()  
{
  if (nsnull != mList)
  {
	  nsIIterator * iterator;

	  mList->CreateIterator(&iterator);
	  iterator->Init();

    ListEntry * item;

	  while(!(iterator->IsDone()))
	  {
		  item = (ListEntry *) iterator->CurrentItem();
		  delete item;
		  iterator->Next();
	  }
	  NS_RELEASE(iterator);

    mList->RemoveAll();
    NS_IF_RELEASE(mList);
  }

  if (nsnull != mState)
  {
	  nsIIterator * iterator;

	  mState->CreateIterator(&iterator);
	  iterator->Init();

    StateEntry * item;

	  while(!(iterator->IsDone()))
	  {
		  item = (StateEntry *) iterator->CurrentItem();
		  delete item;
		  iterator->Next();
	  }
	  NS_RELEASE(iterator);

    mState->RemoveAll();
    NS_IF_RELEASE(mState);
  }
  
  PR_DestroyMonitor(monitor);
}

NS_IMPL_ADDREF(nsXPFCObserverManager)
NS_IMPL_RELEASE(nsXPFCObserverManager)
NS_IMPL_QUERY_INTERFACE(nsXPFCObserverManager, kXPFCObserverManagerIID)

nsresult nsXPFCObserverManager::Init()
{
  if (mList == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mList);

    if (NS_OK != res)
      return res ;

    mList->Init();
  }

  if (mState == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mState);

    if (NS_OK != res)
      return res ;

    mState->Init();
  }

  if (monitor == nsnull) {
    monitor = PR_NewMonitor();
  }

  return NS_OK;
}


nsresult nsXPFCObserverManager::Register(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver)
{
  PR_EnterMonitor(monitor);

  /*
   * Check to see if this registration already exists and if so, simply bump up the ref count
   */

  nsIIterator * iterator;
  PRBool bFound = PR_FALSE;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->subject == aSubject && item->observer == aObserver)
    {
      item->count++;
      bFound = PR_TRUE;
      break;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);
  

  if (PR_FALSE == bFound)
    mList->Append(new ListEntry(aSubject, aObserver));

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult nsXPFCObserverManager::Unregister(nsIXPFCSubject * aSubject, nsIXPFCObserver * aObserver)
{
  PR_EnterMonitor(monitor);

  /*
   * We need to loop through looking for a match of both and then remove them
   */
listunregister:

  nsIIterator * iterator;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->subject == aSubject && item->observer == aObserver)
    {
      mList->Remove((nsComponent)item);
      delete item;
      NS_RELEASE(iterator);
      goto listunregister;
      break;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult nsXPFCObserverManager::UnregisterSubject(nsIXPFCSubject * aSubject)
{
  return (Unregister((nsISupports*)aSubject));
}

nsresult nsXPFCObserverManager::UnregisterObserver(nsIXPFCObserver * aObserver)
{
  return (Unregister((nsISupports*)aObserver));
}

/*
 * If an arbitrary object is passed in to unregister, we want to
 * remove it from all lists containing it as either a subject OR
 * observer
 */

nsresult nsXPFCObserverManager::Unregister(nsISupports * aSubjectObserver)
{
  PR_EnterMonitor(monitor);

  /*
   * See which interfaces the passed in object supports
   */

//XXX We really want to QueryInterface here ...

  nsIXPFCObserver * observer = (nsIXPFCObserver *)aSubjectObserver;
  nsIXPFCSubject * subject = (nsIXPFCSubject *)aSubjectObserver;


#if 0
  nsresult res = aSubjectObserver->QueryInterface(kCXPFCObserverIID, (void**)observer);

  if (NS_OK != res)
    observer = nsnull;

  res = aSubjectObserver->QueryInterface(kCXPFCSubjectIID, (void**)subject);

  if (NS_OK != res)
    subject = nsnull;

  /*
   * If this object does not support either, it's not doing anything
   */

  if ((observer == nsnull) && (subject == nsnull)) {
      NS_IF_RELEASE(subject);
      NS_IF_RELEASE(observer);
      return NS_OK;
  }
#endif
  /*
   * We need to loop through looking for a match of both and then remove them
   */
listdelete:

  nsIIterator * iterator;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->subject == subject || item->observer == observer)
    {
      mList->Remove((nsComponent)item);
      delete item;
      NS_RELEASE(iterator);
      goto listdelete;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);

  PR_ExitMonitor(monitor);

#if 0
  NS_IF_RELEASE(subject);
  NS_IF_RELEASE(observer);
#endif

  return NS_OK;
}


nsresult nsXPFCObserverManager::Notify(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{
  PR_EnterMonitor(monitor);

  if (0 == mNotificationCount)
  {
    mOriginalNotifier = aSubject;
    NS_ADDREF(aSubject);
  }

  mNotificationCount++;

  nsIIterator * iterator;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->subject == aSubject || aSubject == nsnull)
    {
      item->observer->Update(item->subject, aCommand);

      /*
       * Check to see if this observer has registered for 
       * Command State Notifications and if so, update it
       */

      CheckForCommandStateNotification(item->observer);
    }

    iterator->Next();
  }

  NS_RELEASE(iterator);

  mNotificationCount--;

  /*
   * If this set of Notifications has completed, check to see
   * if anyone has registered to be notified of command complete
   * notifications, and tell them if their state was indeed updated
   * as a result of the previous notification round
   */

  if (0 == mNotificationCount)
  {
    SendCommandStateNotifications(nsCommandState_eComplete);
    mOriginalNotifier = nsnull;
    NS_RELEASE(aSubject);
  }

  PR_ExitMonitor(monitor);
  
  return NS_OK;
}

nsresult nsXPFCObserverManager::RegisterForCommandState(nsIXPFCObserver * aObserver, nsCommandState aCommandState)
{
  PR_EnterMonitor(monitor);

  mState->Append(new StateEntry(aCommandState, aObserver));

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult nsXPFCObserverManager::CheckForCommandStateNotification(nsIXPFCObserver * aObserver)
{
  nsIIterator * iterator;

  mState->CreateIterator(&iterator);

  iterator->Init();

  StateEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (StateEntry *) iterator->CurrentItem();

    if (item->observer == aObserver)
      item->update_when_complete = PR_TRUE;

    iterator->Next();
  }

  NS_RELEASE(iterator);

  return NS_OK;
}

nsresult nsXPFCObserverManager::SendCommandStateNotifications(nsCommandState aCommandState)
{
  nsIIterator * iterator;
  nsXPFCNotificationStateCommand * command = nsnull;
  nsresult res = NS_OK;

  mState->CreateIterator(&iterator);

  iterator->Init();

  StateEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (StateEntry *) iterator->CurrentItem();

    if (PR_TRUE == item->update_when_complete)
    {
      /*
       * Create the CommandState Event with attribute eComplete
       */
      if (nsnull == command)
      {
        static NS_DEFINE_IID(kCXPFCNotificationStateCommandCID, NS_XPFC_NOTIFICATIONSTATE_COMMAND_CID);
        static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

        res = nsRepository::CreateInstance(kCXPFCNotificationStateCommandCID, 
                                           nsnull, 
                                           kXPFCCommandIID, 
                                           (void **)&command);

        if (NS_OK != res)
          break ;

        command->Init();

        command->mCommandState = aCommandState;
      }

      item->update_when_complete = PR_FALSE;
      item->observer->Update(mOriginalNotifier, command);
    }

    iterator->Next();
  }

  NS_RELEASE(iterator);
  NS_IF_RELEASE(command);

  return NS_OK;
}
