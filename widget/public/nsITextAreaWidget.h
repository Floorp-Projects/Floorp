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

#ifndef nsITextAreaWidget_h__
#define nsITextAreaWidget_h__

#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsString.h"

// {F8030012-C342-11d1-97F0-00609703C14E}
#define NS_ITEXTAREAWIDGET_IID \
{ 0xf8030012, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } };

/**
 * Multi-line text editor. 
 * @see nsITextWidget for capabilities. 
 * Displays a scrollbar when the text content exceeds the number of lines 
 * displayed.
 */

class nsITextAreaWidget : public nsITextWidget 
{
};

#endif // nsITextAreaWidget_h__

