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

#ifndef __nsILookAndFeel
#define __nsILookAndFeel
#include "nsISupports.h"
#include "nsColor.h"

// {21B51DE1-21A3-11d2-B6E0-00805F8A2676}
#define NS_ILOOKANDFEEL_IID \
{ 0x21b51de1, 0x21a3, 0x11d2, \
    { 0xb6, 0xe0, 0x0, 0x80, 0x5f, 0x8a, 0x26, 0x76 } }

class nsILookAndFeel: public nsISupports {
public:
  typedef enum {
    WindowBackground,
    WindowForeground,
    WidgetBackground,
    WidgetForeground,
    WidgetSelectBackground,
    WidgetSelectForeground,
    Widget3DHighlight,
    Widget3DShadow,
    TextBackground,
    TextForeground,
    TextSelectBackground,
    TextSelectForeground
  } nsColorID;

  typedef enum {
    WindowTitleHeight,
    WindowBorderWidth,
    WindowBorderHeight,
    Widget3DBorder
  } nsMetricID;

  NS_IMETHOD_(nscolor) GetColor(nsColorID aID) = 0;
  NS_IMETHOD_(PRInt32) GetMetric(nsMetricID aID) = 0;
};

#define nsLAF nsILookAndFeel

#endif /* __nsILookAndFeel */
