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

#ifndef nsIComboBox_h__
#define nsIComboBox_h__

#include "nsIListWidget.h"
#include "nsString.h"

// {961085F6-BD28-11d1-97EF-00609703C14E}
#define NS_ICOMBOBOX_IID \
{ 0x961085f6, 0xbd28, 0x11d1, { 0x97, 0xef, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } };


/**
 * Initialize combobox data
 */

struct nsComboBoxInitData : public nsWidgetInitData {
  PRUint32 mDropDownHeight; // in pixels
};

/**
 * Single selection drop down list. See nsIListWidget for capabilities 
 */

class nsIComboBox : public nsIListWidget {
};


#endif // nsIComboBox_h__



