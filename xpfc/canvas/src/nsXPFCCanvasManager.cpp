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
#include "nsIView.h"
#include "nsxpfcCIID.h"
#include "nsIViewObserver.h"

static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCCanvasManagerIID, NS_IXPFC_CANVAS_MANAGER_IID);
static NS_DEFINE_IID(kCXPFCCanvasIID,       NS_IXPFC_CANVAS_IID);
static NS_DEFINE_IID(kIViewObserverIID,     NS_IVIEWOBSERVER_IID);

class ViewListEntry {
public:
  nsIView * view;
  nsIXPFCCanvas * canvas;

  ViewListEntry(nsIView * aView, 
                nsIXPFCCanvas * aCanvas) { 
    view = aView;
    canvas = aCanvas;
  }
  ~ViewListEntry() {
  }
};

class WidgetListEntry {
public:
  nsIWidget * widget;
  nsIXPFCCanvas * canvas;

  WidgetListEntry(nsIWidget * aWidget, 
                  nsIXPFCCanvas * aCanvas) { 
    widget = aWidget;
    canvas = aCanvas;
  }
  ~WidgetListEntry() {
  }
};


nsXPFCCanvasManager :: nsXPFCCanvasManager()
{
  NS_INIT_REFCNT();
  mViewList = nsnull;
  mWidgetList = nsnull;
  monitor = nsnull;
  mRootCanvas = nsnull;
  mFocusedCanvas = nsnull;
  mMouseOverCanvas = nsnull;
  mPressedCanvas = nsnull;
  mViewManager = nsnull;
}

nsXPFCCanvasManager :: ~nsXPFCCanvasManager()  
{

  nsIIterator * iterator;
  mViewList->CreateIterator(&iterator);
  iterator->Init();

  ViewListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ViewListEntry *) iterator->CurrentItem();
    delete item;
    iterator->Next();
  }
  NS_RELEASE(iterator);

  mViewList->RemoveAll();

  PR_DestroyMonitor(monitor);
  NS_IF_RELEASE(mRootCanvas);
  NS_IF_RELEASE(mViewList);
  NS_IF_RELEASE(mWidgetList);
}

NS_IMPL_ADDREF(nsXPFCCanvasManager)
NS_IMPL_RELEASE(nsXPFCCanvasManager)

nsresult nsXPFCCanvasManager::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFCCanvasManagerIID);
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCCanvasManager *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kIViewObserverIID)) {
      *aInstancePtr = (void*)((nsIViewObserver*)this);
      AddRef();
      return NS_OK;
  }
  return (NS_ERROR_NO_INTERFACE);
}



nsresult nsXPFCCanvasManager::Init()
{
  if (mViewList == nsnull) 
  {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mViewList);

    if (NS_OK != res)
      return res ;

    mViewList->Init();
  }

  if (mWidgetList == nsnull) 
  {

    static NS_DEFINE_IID(kCVectorIteratorCID, NS_ARRAY_ITERATOR_CID);
    static NS_DEFINE_IID(kCVectorCID, NS_ARRAY_CID);

    nsresult res = nsRepository::CreateInstance(kCVectorCID, 
                                       nsnull, 
                                       kCVectorCID, 
                                       (void **)&mWidgetList);

    if (NS_OK != res)
      return res ;

    mWidgetList->Init();
  }

  if (monitor == nsnull) {
    monitor = PR_NewMonitor();
  }

  return NS_OK;
}

nsIXPFCCanvas * nsXPFCCanvasManager::CanvasFromView(nsIView * aView)
{
  nsIXPFCCanvas * canvas = nsnull;

  PR_EnterMonitor(monitor);

  nsIIterator * iterator;

  mViewList->CreateIterator(&iterator);

  iterator->Init();

  ViewListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ViewListEntry *) iterator->CurrentItem();

    if (item->view == aView)
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

nsIXPFCCanvas * nsXPFCCanvasManager::CanvasFromWidget(nsIWidget * aWidget)
{
  nsIXPFCCanvas * canvas = nsnull;

  PR_EnterMonitor(monitor);

  nsIIterator * iterator;

  mWidgetList->CreateIterator(&iterator);

  iterator->Init();

  WidgetListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (WidgetListEntry *) iterator->CurrentItem();

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

nsresult nsXPFCCanvasManager::RegisterView(nsIXPFCCanvas * aCanvas, nsIView * aView)
{
  PR_EnterMonitor(monitor);

  mViewList->Append(new ViewListEntry(aView, aCanvas));

  PR_ExitMonitor(monitor);

  aView->SetClientData((nsIViewObserver*)this);

  return NS_OK;
}

nsresult nsXPFCCanvasManager::RegisterWidget(nsIXPFCCanvas * aCanvas, nsIWidget * aWidget)
{
  PR_EnterMonitor(monitor);

  mWidgetList->Append(new WidgetListEntry(aWidget, aCanvas));

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

  mViewList->CreateIterator(&iterator);

  iterator->Init();

  ViewListEntry * item ;

  while(!(iterator->IsDone()))
  {
    item = (ViewListEntry *) iterator->CurrentItem();

    if (item->canvas == aCanvas)
    {
      delete item;
      mViewList->Remove((nsComponent)item);
      break;
    }  

    iterator->Next();
  }

  NS_RELEASE(iterator);


  mWidgetList->CreateIterator(&iterator);

  iterator->Init();

  WidgetListEntry * witem ;

  while(!(iterator->IsDone()))
  {
    witem = (WidgetListEntry *) iterator->CurrentItem();

    if (witem->canvas == aCanvas)
    {
      mWidgetList->Remove((nsComponent)witem);
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

nsresult nsXPFCCanvasManager::SetPressedCanvas(nsIXPFCCanvas * aCanvas)
{
  mPressedCanvas = aCanvas;
  return NS_OK;
}

nsIXPFCCanvas * nsXPFCCanvasManager::GetPressedCanvas()
{
  return(mPressedCanvas);
}

nsresult nsXPFCCanvasManager::SetMouseOverCanvas(nsIXPFCCanvas * aCanvas)
{
  mMouseOverCanvas = aCanvas;
  return NS_OK;
}

nsIXPFCCanvas * nsXPFCCanvasManager::GetMouseOverCanvas()
{
  return(mMouseOverCanvas);
}

nsresult nsXPFCCanvasManager::Paint(nsIView * aView,
                                    nsIRenderingContext& aRenderingContext,
                                    const nsRect& aDirtyRect)
{
  nsIXPFCCanvas * canvas = CanvasFromView(aView);

  if (canvas == nsnull)
    return NS_OK;

  canvas->OnPaint(aRenderingContext, aDirtyRect);
  
  return NS_OK;
}

nsresult nsXPFCCanvasManager::HandleEvent(nsIView * aView,
                                          nsGUIEvent*     aEvent,
                                          nsEventStatus&  aEventStatus)
{
  nsIXPFCCanvas * canvas = CanvasFromView(aView);

  if (canvas == nsnull)
    return NS_OK;

  aEventStatus = canvas->HandleEvent(aEvent);

  return NS_OK;
}

nsresult nsXPFCCanvasManager::Scrolled(nsIView * aView)
{
  nsIXPFCCanvas * canvas = CanvasFromView(aView);

  if (canvas == nsnull)
    return NS_OK;

  return NS_OK;
}

nsresult nsXPFCCanvasManager::ResizeReflow(nsIView * aView, 
                                           nscoord aWidth, 
                                           nscoord aHeight)
{
  nsIXPFCCanvas * canvas = CanvasFromView(aView);

  if (canvas == nsnull)
    return NS_OK;

  canvas->OnResize(0,0,aWidth,aHeight);

  return NS_OK;
}



nsresult nsXPFCCanvasManager::SetWebViewerContainer(nsIWebViewerContainer * aWebViewerContainer)
{
  mWebViewerContainer = aWebViewerContainer;
  return NS_OK;
}

nsIWebViewerContainer * nsXPFCCanvasManager::GetWebViewerContainer()
{
  return(mWebViewerContainer);
}

nsresult nsXPFCCanvasManager::SetViewManager(nsIViewManager * aViewManager)
{
  mViewManager = aViewManager;
  mViewManager->SetViewObserver((nsIViewObserver*)this);
  return NS_OK;
}

nsIViewManager * nsXPFCCanvasManager::GetViewManager()
{
  return(mViewManager);
}

