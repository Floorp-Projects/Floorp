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
#include "nsIXPFCCommandStateObserver.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCObserverManagerIID, NS_IXPFC_OBSERVERMANAGER_IID);
static NS_DEFINE_IID(kCXPFCObserverIID, NS_IXPFC_OBSERVER_IID);
static NS_DEFINE_IID(kCXPFCSubjectIID, NS_IXPFC_SUBJECT_IID);

class ListEntry {
public:
  nsIXPFCSubject * subject;
  nsIXPFCObserver * observer;

  ListEntry(nsIXPFCSubject * aSubject, 
            nsIXPFCObserver * aObserver) { 
    subject = aSubject;
    observer = aObserver;
  }
  ~ListEntry() {
  }
};

class StateEntry {
public:
  nsCommandState state;
  nsIXPFCCommandStateObserver * observer;

  StateEntry(nsCommandState aState, 
            nsIXPFCCommandStateObserver * aObserver) { 
    state = aState;
    observer = aObserver;
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

  Init();
}

nsXPFCObserverManager :: ~nsXPFCObserverManager()  
{
  NS_IF_RELEASE(mList);
  NS_IF_RELEASE(mState);
  PR_DestroyMonitor(monitor);
}

NS_IMPL_ADDREF(nsXPFCObserverManager)
NS_IMPL_RELEASE(nsXPFCObserverManager)
NS_IMPL_QUERY_INTERFACE(nsXPFCObserverManager, kXPFCObserverManagerIID)

nsresult nsXPFCObserverManager::Init()
{
  if (mList == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mList);

    if (NS_OK != res)
      return res ;

    mList->Init();
  }

  if (mState == nsnull) {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_VECTOR_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_VECTOR_CID);

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
      break;
    }  

    iterator->Next();
  }

  PR_ExitMonitor(monitor);

  return NS_OK;
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

  nsIXPFCObserver * observer;
  nsIXPFCSubject * subject;

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

  /*
   * We need to loop through looking for a match of both and then remove them
   */
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
      break;
    }  

    iterator->Next();
  }

  PR_ExitMonitor(monitor);

  NS_IF_RELEASE(subject);
  NS_IF_RELEASE(observer);

  return NS_OK;
}


nsresult nsXPFCObserverManager::Notify(nsIXPFCSubject * aSubject, nsIXPFCCommand * aCommand)
{
  PR_EnterMonitor(monitor);

  nsIIterator * iterator;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->subject == aSubject || aSubject == nsnull)
      item->observer->Update(item->subject, aCommand);

    iterator->Next();
  }

  PR_ExitMonitor(monitor);
  
  return NS_OK;
}

nsresult nsXPFCObserverManager::RegisterForCommandState(nsIXPFCCommandStateObserver * aCommandStateObserver, nsCommandState aCommandState)
{
  PR_EnterMonitor(monitor);

  mState->Append(new StateEntry(aCommandState, aCommandStateObserver));

  PR_ExitMonitor(monitor);

  return NS_OK;
}
