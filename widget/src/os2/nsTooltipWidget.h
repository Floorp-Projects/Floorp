/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): 
 *
 */

#ifndef _nstooltipwidget_h
#define _nstooltipwidget_h

#include "nsWindow.h"
#include "nsITooltipWidget.h"

// Tooltip window.

class nsTooltipWidget : public nsWindow, public nsITooltipWidget
{
 public:
   nsTooltipWidget() : nsWindow() {}

   // nsISupports
   NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);                           
   NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
   NS_IMETHOD_(nsrefcnt) Release(void);          

   // platform hooks
   virtual PCSZ     WindowClass();
   virtual ULONG    WindowStyle();

   // Message stopping
   virtual PRBool OnResize( PRInt32 aX, PRInt32 aY)  { return PR_FALSE; }

   // Look like a tooltip
   virtual PRBool OnPaint();
};

#endif
