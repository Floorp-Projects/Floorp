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
#include "nsILinkHandler.h"
#include "nsIDocument.h"
#include "nsString.h"
#include "nsCRT.h"
#include "prthread.h"
#include "plevent.h"

static NS_DEFINE_IID(kILinkHandlerIID, NS_ILINKHANDLER_IID);

class LinkHandlerImpl : public nsILinkHandler {
public:
  LinkHandlerImpl();
  ~LinkHandlerImpl();

  void* operator new(size_t sz) {
    void* rv = new char[sz];
    nsCRT::zero(rv, sz);
    return rv;
  }

  // nsISupports
  NS_DECL_ISUPPORTS;

  // nsILinkHandler
  NS_IMETHOD Init(nsIWebWidget* aWidget);
  NS_IMETHOD GetWebWidget(nsIWebWidget** aResult);
  NS_IMETHOD OnLinkClick(nsIFrame* aFrame, 
                         const nsString& aURLSpec,
                         const nsString& aTargetSpec,
                         nsIPostData* aPostData = 0);
  NS_IMETHOD OnOverLink(nsIFrame* aFrame, 
                        const nsString& aURLSpec,
                        const nsString& aTargetSpec);
  NS_IMETHOD GetLinkState(const nsString& aURLSpec, nsLinkState& aState);

  void HandleLinkClickEvent(const nsString& aURLSpec,
                            const nsString& aTargetSpec,
                            nsIPostData* aPostDat = 0);

  nsIWebWidget* mWidget;
  nsIPostData* mPostData;
};

//----------------------------------------------------------------------

struct OnLinkClickEvent : public PLEvent {
  OnLinkClickEvent(LinkHandlerImpl* aHandler, const nsString& aURLSpec,
                   const nsString& aTargetSpec, nsIPostData* aPostData = 0);
  ~OnLinkClickEvent();

  void HandleEvent();

  LinkHandlerImpl* mHandler;
  nsString*    mURLSpec;
  nsString*    mTargetSpec;
  nsIPostData *mPostData;
};

static void PR_CALLBACK HandleEvent(OnLinkClickEvent* aEvent)
{
  aEvent->HandleEvent();
}

static void PR_CALLBACK DestroyEvent(OnLinkClickEvent* aEvent)
{
  delete aEvent;
}

OnLinkClickEvent::OnLinkClickEvent(LinkHandlerImpl* aHandler,
                                   const nsString& aURLSpec,
                                   const nsString& aTargetSpec,
                                   nsIPostData* aPostData)
{
  mHandler = aHandler;
  NS_ADDREF(aHandler);
  mURLSpec = new nsString(aURLSpec);
  mTargetSpec = new nsString(aTargetSpec);
  mPostData = nsnull;
  if (aPostData) {
    NS_NewPostData(aPostData, &mPostData);
  }

  PL_InitEvent(this, nsnull,
               (PLHandleEventProc) ::HandleEvent,
               (PLDestroyEventProc) ::DestroyEvent);

#ifdef XP_PC
  PLEventQueue* eventQueue = PL_GetMainEventQueue();
#endif
  PL_PostEvent(eventQueue, this);
}

OnLinkClickEvent::~OnLinkClickEvent()
{
  NS_IF_RELEASE(mHandler);
  if (nsnull != mURLSpec) delete mURLSpec;
  if (nsnull != mTargetSpec) delete mTargetSpec;
  if (nsnull != mPostData) delete mPostData;
}

void OnLinkClickEvent::HandleEvent()
{
  mHandler->HandleLinkClickEvent(*mURLSpec, *mTargetSpec, mPostData);
}

//----------------------------------------------------------------------

LinkHandlerImpl::LinkHandlerImpl()
{
}

LinkHandlerImpl::~LinkHandlerImpl()
{
}

NS_IMPL_ISUPPORTS(LinkHandlerImpl, kILinkHandlerIID);

// this Init method assumes that the nsIWebWidget passed in owns this
// LinkHandlerImpl also, so it wont refcount the aWidget.  
NS_IMETHODIMP
LinkHandlerImpl::Init(nsIWebWidget* aWidget)
{
  NS_PRECONDITION(nsnull != aWidget, "null ptr");
  NS_PRECONDITION(nsnull == mWidget, "init twice");
  if (nsnull == aWidget) {
    return NS_ERROR_NULL_POINTER;
  }
  if (nsnull != mWidget) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  mWidget = aWidget;
  //NS_ADDREF(mWidget);  // this widget is owned by the same owner of the LinkHandler, no circular ref.
  return NS_OK;
}

NS_IMETHODIMP
LinkHandlerImpl::GetWebWidget(nsIWebWidget** aResult)
{
  NS_PRECONDITION(nsnull != aResult, "null ptr");
  if (nsnull == aResult) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = mWidget;
  NS_IF_ADDREF(mWidget);
  return NS_OK;
}

NS_IMETHODIMP
LinkHandlerImpl::OnLinkClick(nsIFrame* aFrame,
                             const nsString& aURLSpec,
                             const nsString& aTargetSpec,
                             nsIPostData* aPostData)

{
  new OnLinkClickEvent(this, aURLSpec, aTargetSpec, aPostData);
  return NS_OK;
}

NS_IMETHODIMP
LinkHandlerImpl::OnOverLink(nsIFrame* aFrame,
                            const nsString& aURLSpec,
                            const nsString& aTargetSpec)
{
  return NS_OK;
}

NS_IMETHODIMP
LinkHandlerImpl::GetLinkState(const nsString& aURLSpec, nsLinkState& aState)
{
  // XXX not yet implemented
  aState = eLinkState_Unvisited;
  return NS_OK;
}

void
LinkHandlerImpl::HandleLinkClickEvent(const nsString& aURLSpec,
                                      const nsString& aTargetSpec,
                                      nsIPostData* aPostData)
{
  if (nsnull != mWidget) {
    mWidget->LoadURL(aURLSpec, nsnull, aPostData);
  }
}

NS_WEB nsresult NS_NewLinkHandler(nsILinkHandler** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  LinkHandlerImpl* it = new LinkHandlerImpl();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kILinkHandlerIID, (void**) aInstancePtrResult);
}
