/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

#include "nsLookAndFeel.h"
#include "windows.h"
 
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------

nsresult nsLookAndFeel::QueryObject(const nsIID& aIID, void** aInstancePtr)
{
  nsresult result = nsObject::QueryObject(aIID, aInstancePtr);

  if (result == NS_NOINTERFACE && aIID.Equals(kILookAndFeelIID)) {
      *aInstancePtr = (void*) ((nsILookAndFeel*)this);
      AddRef();
      result = NS_OK;
  }

  return result;
}

nsLookAndFeel::nsLookAndFeel(nsISupports *aOuter): nsObject(aOuter)
{
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP_(nscolor) nsLookAndFeel::GetColor(nsColorID aID) 
{
    int idx;
    switch (aID) {
    case WindowBackground:
        idx = COLOR_WINDOW;
        break;
    case WindowForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case WidgetBackground:
        idx = COLOR_BTNFACE;
        break;
    case WidgetForeground:
        idx = COLOR_BTNTEXT;
        break;
    case WidgetSelectBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case WidgetSelectForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    case Widget3DHighlight:
        idx = COLOR_BTNHIGHLIGHT;
        break;
    case Widget3DShadow:
        idx = COLOR_BTNSHADOW;
        break;
    case TextBackground:
        idx = COLOR_WINDOW;
        break;
    case TextForeground:
        idx = COLOR_WINDOWTEXT;
        break;
    case TextSelectBackground:
        idx = COLOR_HIGHLIGHT;
        break;
    case TextSelectForeground:
        idx = COLOR_HIGHLIGHTTEXT;
        break;
    default:
        idx = COLOR_WINDOW;    
        break;
    }

    return ::GetSysColor(idx);
}

NS_IMETHODIMP_(PRInt32) nsLookAndFeel::GetMetric(nsMetricID aID)
{
    PRInt32 res;
    switch (aID) {
    case WindowTitleHeight:
        res = ::GetSystemMetrics(SM_CYCAPTION);
        break;
    case WindowBorderWidth:
        res = ::GetSystemMetrics(SM_CXFRAME);
        break;
    case WindowBorderHeight:
        res = ::GetSystemMetrics(SM_CYFRAME);
        break;
    case Widget3DBorder:
        res = ::GetSystemMetrics(SM_CXEDGE);
        break;
    default:
        res = -1;
    }
    return res;
}

