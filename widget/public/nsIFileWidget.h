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

#ifndef nsIFileWidget_h__
#define nsIFileWidget_h__

#include "nsString.h"

// {F8030015-C342-11d1-97F0-00609703C14E}
#define NS_IFILEWIDGET_IID \
{ 0xf8030015, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } };


/**
 * File selector mode 
 */

enum nsMode {
        /// Load a file or directory
      eMode_load,
        /// Save a file or directory
      eMode_save };

/**
 * File selector widget.
 * Modally selects files for loading or saving from a list.
 */

class nsIFileWidget : public nsISupports 
{

public:

 /**
  * Creates a file widget with the specified title and mode.
  * @param aParent the owner of the widget
  * @param aTitle the title of the widget
  * @param aMode the mode of the widget
  * @param aContext context for displaying widget
  * @param aToolkit toolkit associated with file widget
  */ 

  virtual void  Create( nsIWidget *aParent,
                        nsString& aTitle,
                        nsMode aMode,
                        nsIDeviceContext *aContext = nsnull,
                        nsIToolkit *aToolkit = nsnull) = 0;

 /**
  * Set the list of file filters 
  *
  * @param      aNumberOfFilter  number of filters.
  * @param      aTitle           array of filter titles
  * @param      aFilter          array of filters to associate with titles            
  * @return     void
  *
  */

  virtual void SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[]) = 0;

 /**
  * Show File Dialog. The dialog is displayed modally.
  *
  * @return PR_TRUE if user selects OK, PR_FALSE if user selects CANCEL
  *
  */

  virtual PRBool Show() = 0;

 /**
  * Get the file or directory including the full path.
  *
  * @param aFile on exit it contains the file or directory selected
  */
  
  virtual void GetFile(nsString& aFile) = 0;

};

#endif // nsIFileWidget_h__

