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

#ifndef nsIDMWidget_h___
#define nsIDMWidget_h___

#include "nsISupports.h"
#include "nsGUIEvent.h"

class nsIDataModel;
class nsIDMItem;

// {E6ED4EC4-5985-11d2-96ED-00104B7B7DEB}
#define NS_IDMWIDGET_IID      \
{ 0xe6ed4ec4, 0x5985, 0x11d2, \
	{ 0x96, 0xed, 0x0, 0x10, 0x4b, 0x7b, 0x7d, 0xeb } }

// Events that the widget can receive from the data model.
const cDSImageLoaded = 0;

class nsIDMWidget : public nsISupports
{

public:
	// Inspectors
	NS_IMETHOD GetDataModel(nsIDataModel*& pDataModel) = 0;

	// Setters
	NS_IMETHOD SetDataModel(nsIDataModel* pDataModel) = 0;

	NS_IMETHOD HandleDataModelEvent(int aEvent, nsIDMItem* pNode) = 0;
};

#endif /* nsIDMWidget_h___ */

