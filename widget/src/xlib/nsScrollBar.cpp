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

NS_IMPL_ADDREF(nsScrollbar)
NS_IMPL_RELEASE(nsScrollbar)

nsScrollbar::nsScrollbar(PRBool aIsVertical) : nsWidget(), nsIScrollbar()
{
  NS_INIT_REFCNT();
};

nsScrollbar::~nsScrollbar()
{
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
  return NS_OK;
}

PRUint32 nsScrollbar::GetMaxRange(PRUint32& aRange)
{
  return 0;
}

NS_METHOD nsScrollbar::SetPosition(PRUint32 aPos)
{
  return NS_OK;
}

PRUint32 nsScrollbar::GetPosition(PRUint32& aPosition)
{
  return 0;
}

NS_METHOD nsScrollbar::SetThumbSize(PRUint32 aSize)
{
  return NS_OK;
}

NS_METHOD nsScrollbar::GetThumbSize(PRUint32& aSize)
{
  return NS_OK;
}

NS_METHOD nsScrollbar::SetLineIncrement(PRUint32 aSize)
{
  return NS_OK;
}

NS_METHOD nsScrollbar::GetLineIncrement(PRUint32& aSize)
{
  return NS_OK;
}

NS_METHOD nsScrollbar::SetParameters(PRUint32 aMaxRange, PRUint32 aThumbSize,
                                PRUint32 aPosition, PRUint32 aLineIncrement)
{
  return NS_OK;
}

PRBool nsScrollbar::OnPaint()
{
    return PR_FALSE;
}


PRBool nsScrollbar::OnResize(nsRect &aWindowRect)
{
    return PR_FALSE;
}


PRBool nsScrollbar::OnScroll(PRUint32 scrollCode, int cPos)
{
  return PR_FALSE;
}

NS_METHOD nsScrollbar::GetBounds(nsRect &aRect)
{
  return NS_OK;
}

