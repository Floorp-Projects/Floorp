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

#include "nsScrollBar.h"
#include "nsGfxCIID.h"

NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)

nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWidget(), nsIScrollbar()
{
  NS_INIT_REFCNT();
  mMaxRange = 0;
  mPosition = 0;
  mThumbSize = 0;
  mLineIncrement = 1;
  mIsVertical = aIsVertical;
  mRenderingContext = nsnull;
};

nsScrollbar::~nsScrollbar()
{
  if (mRenderingContext)
    NS_RELEASE(mRenderingContext);
}

nsresult nsScrollbar::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsWidget::QueryInterface(aIID, aInstancePtr);
  
  static NS_DEFINE_IID(kInsScrollbarIID, NS_ISCROLLBAR_IID);
  if (result == NS_NOINTERFACE && aIID.Equals(kInsScrollbarIID)) {
    *aInstancePtr = (void*) ((nsIScrollbar*)this);
    NS_ADDREF_THIS();
    result = NS_OK;
  }
  
  return result;
}

NS_METHOD nsScrollbar::SetMaxRange(PRUint32 aEndRange)
{
  printf("nsScrollbar::SetMaxRange()\n");
  printf("Max Range set to %d\n", aEndRange);
  mMaxRange = aEndRange;
  return NS_OK;
}

PRUint32 nsScrollbar::GetMaxRange(PRUint32& aRange)
{
  printf("nsScrollbar::GetMaxRange()\n");
  aRange = mMaxRange;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  printf("nsScrollbar::SetPosition()\n");
  printf("Scroll to %d\n", aPos);
  mPosition = aPos;
  return NS_OK;
}

PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
  printf("nsScrollbar::GetPosition()\n");
  aPosition = mPosition;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  printf("nsScrollbar::SetThumbSize()\n");
  printf("Thumb size set to %d\n", aSize);
  mThumbSize = aSize;
  return NS_OK;
}

NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  printf("nsScrollbar::GetThumbSize()\n");
  aSize = mThumbSize;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
  printf("nsScrollbar::SetLineIncrement()\n");
  printf("Set Line Increment to %d\n", aSize);
  mLineIncrement = aSize;
  return NS_OK;
}

NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
  printf("nsScrollbar::GetLineIncrement()\n");
  aSize = mLineIncrement;
  return NS_OK;
}

NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{
  printf("nsScrollbar::SetParameters()\n");
  printf("MaxRange = %d ThumbSize = %d aPosition = %d LineIncrement = %d\n",
         aMaxRange, aThumbSize, aPosition, aLineIncrement);
  SetMaxRange(aMaxRange);
  SetThumbSize(aThumbSize);
  SetPosition(aPosition);
  SetLineIncrement(aLineIncrement);
  return NS_OK;
}

PRBool nsScrollbar::OnScroll(PRUint32 scrollCode, int cPos)
{
  printf("nsScrollbar::OnScroll\n");
  return PR_FALSE;
}

PRBool nsScrollbar::OnPaint(nsPaintEvent &event)
{
  PRBool result;
  nsRect scrollRect;
  printf("nsScrollbar::OnPaint\n");
  // create a rendering context for this window
  static NS_DEFINE_IID(kRenderingContextCID, NS_RENDERING_CONTEXT_CID);
  static NS_DEFINE_IID(kRenderingContextIID, NS_IRENDERING_CONTEXT_IID);
  if (nsnull == mRenderingContext) {
    if (NS_OK != nsComponentManager::CreateInstance(kRenderingContextCID,
                                                    nsnull,
                                                    kRenderingContextIID,
                                                    (void **)&mRenderingContext)) {
      // oy vey!
      printf("Oh, dear.  I couldn't create an nsRenderingContext for this scrollbar.  All hell is about to break loose.\n");
    }
    mRenderingContext->Init(mContext, this);
    SetBackgroundColor(NS_RGB(255, 255, 255));
  }
  // draw the scrollbar itself
  mRenderingContext->SetColor(NS_RGB(0,0,0));
  scrollRect.x = 0;
  scrollRect.y = 0;
  scrollRect.width = mBounds.width/2;
  scrollRect.height = mBounds.height/2;
  printf("mBounds %d %d\n",
         mBounds.width, mBounds.height);
  printf("About to fill rect %d %d\n",
         scrollRect.width, scrollRect.height);
  mRenderingContext->FillRect(scrollRect);
  result = PR_FALSE;
  return result;
}

PRBool nsScrollbar::OnResize(nsSizeEvent &event)
{
  PRBool result;
  printf("nsScrollbar::OnResize\n");
  result = PR_FALSE;
  return result;
}

PRBool nsScrollbar::DispatchMouseEvent(nsMouseEvent &aEvent)
{
  PRBool result;
  printf("nsScrollbar::DispatchMouseEvent\n");
  result = PR_FALSE;
  return result;
}


