/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsThrobber.h"
#include "nsIFactory.h"
#include "nsIWidget.h"
#include "nsVoidArray.h"
#include "nsITimer.h"
#include "nsFont.h"
#include "nsIRenderingContext.h"
#include "nsIFontMetrics.h"
#include "nsIComponentManager.h"
#include "nsWidgetsCID.h"
#include "nsCRT.h"
#include "prprf.h"
#include "nsIDeviceContext.h"
#include "nsGUIEvent.h"
#include "nsIImage.h"
#include "nsReadableUtils.h"


static NS_DEFINE_IID(kChildCID, NS_CHILD_CID);

static NS_DEFINE_IID(kIWidgetIID,        NS_IWIDGET_IID);
static NS_DEFINE_IID(kISupportsIID,      NS_ISUPPORTS_IID);


#define THROB_NUM 14
#define THROBBER_AT "resource:/res/throbber/anims%02d.gif"

nsThrobber* nsThrobber::NewThrobber()
{
  nsThrobber* t = new nsThrobber();
  if (t) {
    NS_ADDREF(t);
  }
  return t;
}

//----------------------------------------------------------------------

nsVoidArray* nsThrobber::gThrobbers;

nsThrobber*
nsThrobber::FindThrobberFor(nsIWidget* aWidget)
{
  if (gThrobbers) {
    PRInt32 i, n = gThrobbers->Count();
    for (i = 0; i < n; i++) {
      nsThrobber* th = (nsThrobber*) gThrobbers->ElementAt(i);
      if (nsnull != th) {
        if (th->mWidget == aWidget) {
          return th;
        }
      }
    }
  }
  return nsnull;
}

void
nsThrobber::AddThrobber(nsThrobber* aThrobber)
{
  if (gThrobbers) {
    gThrobbers->AppendElement(aThrobber);
  }
}

void
nsThrobber::RemoveThrobber(nsThrobber* aThrobber)
{
  if (gThrobbers) {
    gThrobbers->RemoveElement(aThrobber);
  }
}

nsEventStatus PR_CALLBACK
nsThrobber::HandleThrobberEvent(nsGUIEvent *aEvent)
{
  nsThrobber* throbber = FindThrobberFor(aEvent->widget);
  if (nsnull == throbber) {
    return nsEventStatus_eIgnore;
  }

  switch (aEvent->message)
  {
    case NS_PAINT:
    {
#if 0

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
#endif
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

PRInt32 nsThrobber::gNumThrobbers;

// Note: operator new zeros our memory
nsThrobber::nsThrobber()
{
  NS_INIT_REFCNT();
  if (0 == gNumThrobbers++) {
    gThrobbers = new nsVoidArray;
  }
  AddThrobber(this);
}

nsThrobber::~nsThrobber()
{
  Destroy();
}

void
nsThrobber::Destroy()
{
  if (mWidget) {
    mWidget->Destroy();
    NS_RELEASE(mWidget);
  }
  RemoveThrobber(this);
  DestroyThrobberImages();

  if (0 == --gNumThrobbers) {
    delete gThrobbers;
    // Do this in case an event shows up later for the throbber...
    gThrobbers = nsnull;
  }
}

NS_IMPL_ISUPPORTS0(nsThrobber)

nsresult
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

nsresult
nsThrobber::MoveTo(PRInt32 aX, PRInt32 aY)
{
  NS_PRECONDITION(nsnull != mWidget, "no widget");
  mWidget->Resize(aX, aY, mWidth, mHeight, PR_TRUE);
  return NS_OK;
}

nsresult
nsThrobber::Show()
{
  mWidget->Show(PR_TRUE);
  return NS_OK;
}

nsresult
nsThrobber::Hide()
{
  mWidget->Show(PR_FALSE);
  return NS_OK;
}

nsresult
nsThrobber::Start()
{
  mRunning = PR_TRUE;
  return NS_OK;
}

nsresult
nsThrobber::Stop()
{
  mRunning = PR_FALSE;
  mIndex = 0;
  mWidget->Invalidate(PR_FALSE);
  return NS_OK;
}

#if 0
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
#endif

void
nsThrobber::ThrobTimerCallback(nsITimer *aTimer, void *aClosure)
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

#ifndef REPEATING_TIMERS

  nsresult rv;
  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_OK == rv) {
    mTimer->Init(ThrobTimerCallback, this, 33);
  }
#endif
}

nsresult
nsThrobber::LoadThrobberImages(const nsString& aFileNameMask, PRInt32 aNumImages)
{
#if 0
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

  mTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_OK != rv) {
    return rv;
  }
  mTimer->Init(ThrobTimerCallback, this, 33, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
  
  char * mask = ToNewCString(aFileNameMask);
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
    nsMemory::Free(mask);

  mWidget->Invalidate(PR_TRUE);

  return rv;
#endif

return NS_OK;
}

void
nsThrobber::DestroyThrobberImages()
{
  if (mTimer) {
    mTimer->Cancel();
  }

#if 0
  if (mImageGroup) {
    mImageGroup->Interrupt();
    for (PRInt32 cnt = 0; cnt < mNumImages; cnt++) {
      nsIImageRequest *imgreq = (nsIImageRequest*) mImages->ElementAt(cnt);
      NS_IF_RELEASE(imgreq);
    }
    NS_RELEASE(mImageGroup);
  }

  if (mImages) {
    delete mImages;
    mImages = nsnull;
  }
#endif
}
