/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIThrobber.h"
#include "nsIFactory.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsITimer.h"
#include "nsIImageGroup.h"
#include "nsIImageObserver.h"
#include "nsIImageRequest.h"
#include "nsFont.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "prprf.h"
#include "nsIDeviceContext.h"


static NS_DEFINE_IID(kChildCID, NS_CHILD_CID);
static NS_DEFINE_IID(kThrobberCID, NS_THROBBER_CID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kIFactoryIID,       NS_IFACTORY_IID);
static NS_DEFINE_IID(kIImageObserverIID, NS_IIMAGEREQUESTOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIThrobberIID,      NS_ITHROBBER_IID);


#define THROB_NUM 14
#define THROBBER_AT "resource:/res/throbber/anims%02d.gif"

static nsVoidArray gThrobbers;

#define INNER_OUTER \
  ((nsThrobber*)((char*)this - offsetof(nsThrobber, mInner)))

class nsThrobber : public nsIThrobber,
                   public nsIImageRequestObserver
{
public:
  nsThrobber(nsISupports* aOuter);

  NS_DECL_AND_IMPL_ZEROING_OPERATOR_NEW

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIThrobber
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds, const nsString& aFileNameMask, PRInt32 aNumImages);
  NS_IMETHOD Init(nsIWidget* aParent, const nsRect& aBounds);
  NS_IMETHOD MoveTo(PRInt32 aX, PRInt32 aY);
  NS_IMETHOD Show();
  NS_IMETHOD Hide();
  NS_IMETHOD Start();
  NS_IMETHOD Stop();

  // nsIImageRequestObserver
  virtual void Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3);

  virtual void NotifyError(nsIImageRequest *aImageRequest,
                           nsImageError aErrorType);

  void Tick();
  
  virtual ~nsThrobber();

  nsresult LoadThrobberImages(const nsString& aFileNameMask, PRInt32 aNumImages);

  void DestroyThrobberImages();


  PRInt32        mWidth;
  PRInt32        mHeight;
  nsIWidget*     mWidget;
  nsVoidArray*   mImages;
  PRInt32        mNumImages;
  PRInt32        mIndex;
  nsIImageGroup* mImageGroup;
  nsITimer*      mTimer;
  PRBool         mRunning;
  PRUint32       mCompletedImages;

  PRInt32        mPreferredWidth;
  PRInt32        mPreferredHeight;


  nsISupports *mOuter;

//-----------------------------------------------
  nsrefcnt AddRefObject() {
    return ++mRefCnt;
  }

  nsrefcnt ReleaseObject() {
    NS_PRECONDITION(0 != mRefCnt, "dup release");
    if (--mRefCnt == 0) {
      NS_DELETEXPCOM(this);
      return 0;
    }
    return mRefCnt;
  }

  nsresult QueryObject(const nsIID& aIID, void** aInstancePtr) {
    if (NULL == aInstancePtr) {
      return NS_ERROR_NULL_POINTER;
    }
    if (aIID.Equals(kIThrobberIID)) {
      *aInstancePtr = (void*)(nsIThrobber*)this;
      NS_ADDREF_THIS();
      return NS_OK;
    }
    if (aIID.Equals(kIImageObserverIID)) {
      *aInstancePtr = (void*)(nsIImageRequestObserver*)this;
      NS_ADDREF_THIS();
      return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
      *aInstancePtr = (void*)(nsISupports*)(nsIThrobber*)this;
      NS_ADDREF_THIS();
      return NS_OK;
    }
    return NS_NOINTERFACE;
  }

  // This is used when we are not aggregated in
  class InnerSupport : public nsISupports {
  public:
    InnerSupport() {}

    NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr) {
      return INNER_OUTER->QueryObject(aIID, aInstancePtr);
    }

    NS_IMETHOD_(nsrefcnt) AddRef() {
      return INNER_OUTER->AddRefObject();
    }

    NS_IMETHOD_(nsrefcnt) Release() {
      return INNER_OUTER->ReleaseObject();
    }
  } mInner;
};

//----------------------------------------------------------------------

static nsThrobber*
FindThrobberFor(nsIWidget* aWidget)
{
  PRInt32 i, n = gThrobbers.Count();
  for (i = 0; i < n; i++) {
    nsThrobber* th = (nsThrobber*) gThrobbers.ElementAt(i);
    if (nsnull != th) {
      if (th->mWidget == aWidget) {
        return th;
      }
    }
  }
  return nsnull;
}

static void
AddThrobber(nsThrobber* aThrobber)
{
  gThrobbers.AppendElement(aThrobber);
}

static void
RemoveThrobber(nsThrobber* aThrobber)
{
  gThrobbers.RemoveElement(aThrobber);
}

static nsEventStatus PR_CALLBACK
HandleThrobberEvent(nsGUIEvent *aEvent)
{
  nsThrobber* throbber = FindThrobberFor(aEvent->widget);
  if (nsnull == throbber) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->message)
  {
    case NS_PAINT:
    {
      nsPaintEvent *pe = (nsPaintEvent *)aEvent;
      nsIRenderingContext *cx = pe->renderingContext;
      nsRect bounds;
      nsIImageRequest *imgreq;
      nsIImage *img;
      PRBool clipState;
   
      pe->widget->GetClientBounds(bounds);

      cx->SetClipRect(*pe->rect, nsClipCombine_kReplace, clipState);

      cx->SetColor(NS_RGB(255, 255, 255));
      cx->DrawLine(0, bounds.height - 1, 0, 0);
      cx->DrawLine(0, 0, bounds.width, 0);

      cx->SetColor(NS_RGB(128, 128, 128));
      cx->DrawLine(bounds.width - 1, 1, bounds.width - 1, bounds.height - 1);
      cx->DrawLine(bounds.width - 1, bounds.height - 1, 0, bounds.height - 1);

      imgreq = (nsIImageRequest *)
        throbber->mImages->ElementAt(throbber->mIndex);

      if ((nsnull == imgreq) || (nsnull == (img = imgreq->GetImage())))
      {
        char str[10];
        nsFont tfont = nsFont("monospace", 0, 0, 0, 0, 10);
        nsIFontMetrics *met;
        nscoord w, h;

        cx->SetColor(NS_RGB(0, 0, 0));
        cx->FillRect(1, 1, bounds.width - 2, bounds.height - 2);

        PR_snprintf(str, sizeof(str), "%02d", throbber->mIndex);

        cx->SetColor(NS_RGB(255, 255, 255));
        cx->SetFont(tfont);
        cx->GetFontMetrics(met);
        if (nsnull != met)
        {
          cx->GetWidth(str, w);
          met->GetHeight(h);
          cx->DrawString(str, PRUint32(2), (bounds.width - w) >> 1, (bounds.height - h) >> 1);
          NS_RELEASE(met);
        }
      }
      else
      {
        cx->DrawImage(img, 1, 1);
        NS_RELEASE(img);
      }

      break;
    }

    case NS_MOUSE_LEFT_BUTTON_UP:
      // XXX wire up to API
      //gTheViewer->GoTo(nsString("http://www.mozilla.org"));
      break;

    case NS_MOUSE_ENTER:
      aEvent->widget->SetCursor(eCursor_hyperlink);
      break;

    case NS_MOUSE_EXIT:
      aEvent->widget->SetCursor(eCursor_standard);
      break;
  }

  return nsEventStatus_eIgnore;
}

//----------------------------------------------------------------------

// Note: operator new zeros our memory
nsThrobber::nsThrobber(nsISupports* aOuter)
{
  // assign outer
  if (aOuter) 
    mOuter = aOuter;
  else 
    mOuter = &mInner;

  mCompletedImages = 0;

  AddThrobber(this);

  mPreferredWidth  = 0;
  mPreferredHeight = 0;
}

nsThrobber::~nsThrobber()
{
  NS_IF_RELEASE(mWidget);
  RemoveThrobber(this);
  DestroyThrobberImages();
}

nsrefcnt
nsThrobber::AddRef()
{
  return mOuter->AddRef();
}

nsrefcnt
nsThrobber::Release()
{
  return mOuter->Release();
}

nsresult
nsThrobber::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  return mOuter->QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsThrobber::Init(nsIWidget* aParent, const nsRect& aBounds)
{
  nsString mask(THROBBER_AT);

  return Init(aParent, aBounds, mask, THROB_NUM);
}

NS_IMETHODIMP
nsThrobber::Init(nsIWidget* aParent, const nsRect& aBounds, const nsString& aFileNameMask, PRInt32 aNumImages)
{
  mWidth     = aBounds.width;
  mHeight    = aBounds.height;
  mNumImages = aNumImages;

  // Create widget
  nsresult rv = nsComponentManager::CreateInstance(kChildCID, nsnull, kIWidgetIID, (void**)&mWidget);
  if (NS_OK != rv) {
    return rv;
  }
  mWidget->Create(aParent, aBounds, HandleThrobberEvent, NULL);
  return LoadThrobberImages(aFileNameMask, aNumImages);
}

NS_IMETHODIMP
nsThrobber::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWidget, "no widget");
  mWidget->Resize(aX, aY, mWidth, mHeight, PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsThrobber::Show()
{
  mWidget->Show(PR_TRUE);
  return NS_OK;
}

NS_IMETHODIMP
nsThrobber::Hide()
{
  mWidget->Show(PR_FALSE);
  return NS_OK;
}

NS_IMETHODIMP
nsThrobber::Start()
{
  mRunning = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsThrobber::Stop()
{
  mRunning = PR_FALSE;
  mIndex = 0;
  mWidget->Invalidate(PR_FALSE);
  return NS_OK;
}

void  
nsThrobber::Notify(nsIImageRequest *aImageRequest,
                      nsIImage *aImage,
                      nsImageNotification aNotificationType,
                      PRInt32 aParam1, PRInt32 aParam2,
                      void *aParam3)
{
  if (aNotificationType == nsImageNotification_kImageComplete) {
    mCompletedImages++;

    // Remove ourselves as an observer of the image request object, because
    // the image request objects each hold a reference to us. This avoids a
    // circular reference problem. If we don't, our ref count will never reach
    // 0 and we won't get destroyed and neither will the image request objects
    aImageRequest->RemoveObserver((nsIImageRequestObserver*)this);
  }
}

void 
nsThrobber::NotifyError(nsIImageRequest *aImageRequest,
                        nsImageError aErrorType)
{
}

static void throb_timer_callback(nsITimer *aTimer, void *aClosure)
{
  nsThrobber* throbber = (nsThrobber*)aClosure;
  throbber->Tick();
}

void
nsThrobber::Tick()
{
  if (mRunning) {
    mIndex++;
    if (mIndex >= mNumImages)
      mIndex = 0;
    mWidget->Invalidate(PR_TRUE);
  } else if (mCompletedImages == (PRUint32)mNumImages) {
    mWidget->Invalidate(PR_TRUE);
    mCompletedImages = 0;
  }

  NS_RELEASE(mTimer);

  nsresult rv = NS_NewTimer(&mTimer);
  if (NS_OK == rv) {
    mTimer->Init(throb_timer_callback, this, 33);
  }
}

nsresult
nsThrobber::LoadThrobberImages(const nsString& aFileNameMask, PRInt32 aNumImages)
{
  nsresult rv;
  char url[2000];

  mImages = new nsVoidArray(mNumImages);
  if (nsnull == mImages) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  rv = NS_NewImageGroup(&mImageGroup);
  if (NS_OK != rv) {
    return rv;
  }

  nsIDeviceContext *deviceCtx = mWidget->GetDeviceContext();
  mImageGroup->Init(deviceCtx, nsnull);
  NS_RELEASE(deviceCtx);

  rv = NS_NewTimer(&mTimer);
  if (NS_OK != rv) {
    return rv;
  }
  mTimer->Init(throb_timer_callback, this, 33);
  
  char * mask = aFileNameMask.ToNewCString();
  for (PRInt32 cnt = 0; cnt < mNumImages; cnt++)
  {
    PR_snprintf(url, sizeof(url), mask, cnt);
    nscolor bgcolor = NS_RGB(0, 0, 0);
    mImages->InsertElementAt(mImageGroup->GetImage(url,
                                                   (nsIImageRequestObserver *)this,
                                                   &bgcolor,
                                                   mWidth - 2,
                                                   mHeight - 2, 0),
                                                   cnt);
  }

  if (nsnull != mask)
    delete [] mask;

  mWidget->Invalidate(PR_TRUE);

  return rv;
}

void
nsThrobber::DestroyThrobberImages()
{
  if (nsnull != mImageGroup)
  {
    if (nsnull != mTimer)
    {
      mTimer->Cancel();     //XXX this should not be necessary. MMP
      NS_RELEASE(mTimer);
    }

    mImageGroup->Interrupt();
    for (PRInt32 cnt = 0; cnt < mNumImages; cnt++)
    {
      nsIImageRequest *imgreq;
      imgreq = (nsIImageRequest *)mImages->ElementAt(cnt);
      if (nsnull != imgreq)
      {
        NS_RELEASE(imgreq);
        mImages->ReplaceElementAt(nsnull, cnt);
      }
    }
    delete mImages;
    NS_RELEASE(mImageGroup);
  }
}



//----------------------------------------------------------------------

// Factory code for creating nsThrobber's

class nsThrobberFactory : public nsIFactory
{
public:
  nsThrobberFactory();
  virtual ~nsThrobberFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  nsrefcnt  mRefCnt;
};

nsThrobberFactory::nsThrobberFactory()
{
  mRefCnt = 0;
}

nsThrobberFactory::~nsThrobberFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

nsresult
nsThrobberFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();  // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsThrobberFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsThrobberFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsThrobberFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsThrobber *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;

  inst = new nsThrobber(aOuter);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}

nsresult
nsThrobberFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" NS_WEB nsresult
NS_NewThrobberFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsThrobberFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}


