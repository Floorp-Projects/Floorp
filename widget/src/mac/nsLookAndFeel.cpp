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
 
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

//-------------------------------------------------------------------------
//
// Query interface implementation
//
//-------------------------------------------------------------------------

NS_IMPL_ISUPPORTS(nsLookAndFeel, kILookAndFeelIID);

nsLookAndFeel::nsLookAndFeel(nsISupports *aOuter)
{
    mRefCnt = 0;
}

nsLookAndFeel::~nsLookAndFeel()
{
}

NS_IMETHODIMP_(nscolor) nsLookAndFeel::GetColor(nsColorID aID) 
{
    nscolor res = NS_RGB(0xff,0xff,0xff);
    switch (aID) {
    case WindowBackground:
        res = NS_RGB(0xff,0xff,0xff);
        break;
    case WindowForeground:
        res = NS_RGB(0x00,0x00,0x00);        
        break;
    case WidgetBackground:
        res = NS_RGB(0x80,0x80,0x80);
        break;
    case WidgetForeground:
        res = NS_RGB(0x00,0x00,0x00);        
        break;
    case WidgetSelectBackground:
        res = NS_RGB(0x80,0x80,0x80);
        break;
    case WidgetSelectForeground:
        res = NS_RGB(0x00,0x00,0x80);
        break;
    case Widget3DHighlight:
        res = NS_RGB(0xa0,0xa0,0xa0);
        break;
    case Widget3DShadow:
        res = NS_RGB(0x40,0x40,0x40);
        break;
    case TextBackground:
        res = NS_RGB(0xff,0xff,0xff);
        break;
    case TextForeground: 
        res = NS_RGB(0x00,0x00,0x00);
	break;
    case TextSelectBackground:
        res = NS_RGB(0x00,0x00,0x80);
        break;
    case TextSelectForeground:
        res = NS_RGB(0xff,0xff,0xff);
        break;
    default:
        break;
    }

    return res;
}

NS_IMETHODIMP_(PRInt32) nsLookAndFeel::GetMetric(nsMetricID aID)
{
    PRInt32 res;
    switch (aID) {
    case WindowTitleHeight:
        res = 0;
        break;
    case WindowBorderWidth:
        res = 4;
        break;
    case WindowBorderHeight:
        res = 4;
        break;
    case Widget3DBorder:
        res = 4;
        break;
    default:
        res = 0;
    }
    return res;
}


