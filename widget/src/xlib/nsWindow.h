/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsWindow_h__
#define nsWindow_h__

#include "nsWidget.h"

#include "nsString.h"

class nsWindow : public nsWidget
{
 public:
  nsWindow();
  ~nsWindow();
  NS_IMETHOD Invalidate(PRBool aIsSynchronous);
  NS_IMETHOD Invalidate(const nsRect & aRect, PRBool aIsSynchronous);
  NS_IMETHOD Update();
  NS_IMETHOD Scroll(PRInt32 aDx, PRInt32 aDy, nsRect *aClipRect);
  NS_IMETHOD SetTitle(const nsString& aTitle);
protected:
  virtual void DestroyNative(void);

  virtual long GetEventMask();

#if 0
  virtual void CreateNative(Window aParent, nsRect aRect);
#endif
};

class ChildWindow : public nsWindow
{
 public:
  ChildWindow();
  virtual PRInt32 IsChild() { return PR_TRUE; };
};

#endif
