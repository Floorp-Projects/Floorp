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

#include "nsXPFCCanvasManager.h"
#include "nsIXPFCCanvas.h"
#include "nsIWidget.h"
#include "nsxpfcCIID.h"

static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCanvasManagerIID, NS_IXPFC_CANVAS_MANAGER_IID);

static NS_DEFINE_IID(kCXPFCCanvasIID, NS_IXPFC_CANVAS_IID);

class ListEntry {
public:
  nsIWidget * widget;
  nsIXPFCCanvas * canvas;

  ListEntry(nsIWidget * aWidget, 
            nsIXPFCCanvas * aCanvas) { 
    widget = aWidget;
    canvas = aCanvas;
  }
  ~ListEntry() {
  }
};


nsXPFCCanvasManager :: nsXPFCCanvasManager()
{
  NS_INIT_REFCNT();
  mList = nsnull;
  monitor = nsnull;
  mRootCanvas = nsnull;
  mFocusedCanvas = nsnull;
}

nsXPFCCanvasManager :: ~nsXPFCCanvasManager()  
{
  PR_DestroyMonitor(monitor);
  NS_IF_RELEASE(mRootCanvas);
  NS_IF_RELEASE(mList);
}

NS_IMPL_ADDREF(nsXPFCCanvasManager)
NS_IMPL_RELEASE(nsXPFCCanvasManager)
NS_IMPL_QUERY_INTERFACE(nsXPFCCanvasManager, kXPFCCanvasManagerIID)

nsresult nsXPFCCanvasManager::Init()
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

  if (monitor == nsnull) {
    monitor = PR_NewMonitor();
  }

  return NS_OK;
}

nsIXPFCCanvas * nsXPFCCanvasManager::CanvasFromWidget(nsIWidget * aWidget)
{
  nsIXPFCCanvas * canvas = nsnull;

  PR_EnterMonitor(monitor);

  nsIIterator * iterator;

  mList->CreateIterator(&iterator);

  iterator->Init();

  ListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ListEntry *) iterator->CurrentItem();

    if (item->widget == aWidget)
    {
      canvas = item->canvas;
      break;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);

  PR_ExitMonitor(monitor);

  return (canvas);
}

nsresult nsXPFCCanvasManager::Register(nsIXPFCCanvas * aCanvas, nsIWidget * aWidget)
{
  PR_EnterMonitor(monitor);

  mList->Append(new ListEntry(aWidget, aCanvas));

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult nsXPFCCanvasManager::Unregister(nsIXPFCCanvas * aCanvas)
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

    if (item->canvas == aCanvas)
    {
      mList->Remove((nsComponent)item);
      break;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);

  PR_ExitMonitor(monitor);

  return NS_OK;
}

nsresult nsXPFCCanvasManager::GetRootCanvas(nsIXPFCCanvas ** aCanvas)
{
  *aCanvas = mRootCanvas;
  NS_ADDREF(mRootCanvas);
  return NS_OK;
}

nsresult nsXPFCCanvasManager::SetRootCanvas(nsIXPFCCanvas * aCanvas)
{
  NS_IF_RELEASE(mRootCanvas);
  mRootCanvas = aCanvas;
  NS_ADDREF(mRootCanvas);
  return NS_OK;
}

nsresult nsXPFCCanvasManager::SetFocusedCanvas(nsIXPFCCanvas * aCanvas)
{
  mFocusedCanvas = aCanvas;

  mFocusedCanvas->SetFocus();

  return NS_OK;
}

nsIXPFCCanvas * nsXPFCCanvasManager::GetFocusedCanvas()
{
  return(mFocusedCanvas);
}
