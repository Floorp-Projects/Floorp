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
{ 0xf8030015, 0xc342, 0x11d1, { 0x97, 0xf0, 0x0, 0x60, 0x97, 0x3, 0xc1, 0x4e } }


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
  * Create the file filter. This differs from the standard
  * widget Create method because it passes in the mode
  *
  * @param      aParent  the parent to place this widget into
  * @param      aTitle   The title for the file widget
  * @param      aMode    load or save              
  * @return     void
  *
  */
  NS_IMETHOD Create(nsIWidget *aParent,
                      nsString& aTitle,
                      nsMode aMode,
                      nsIDeviceContext *aContext = nsnull,
                      nsIAppShell *aAppShell = nsnull,
                      nsIToolkit *aToolkit = nsnull,
                      void *aInitData = nsnull) = 0;


 /**
  * Set the list of file filters 
  *
  * @param      aNumberOfFilter  number of filters.
  * @param      aTitle           array of filter titles
  * @param      aFilter          array of filters to associate with titles            
  * @return     void
  *
  */

  NS_IMETHOD SetFilterList(PRUint32 aNumberOfFilters,const nsString aTitles[],const nsString aFilters[]) = 0;

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
  
  NS_IMETHOD GetFile(nsString& aFile) = 0;


 /**
  * Set the default string that appears in file open/save dialog 
  *
  * @param      aString          the name of the file
  * @return     void
  *
  */
  NS_IMETHOD SetDefaultString(nsString& aString) = 0;

 /**
  * Set the directory that the file open/save dialog initially displays
  *
  * @param      aDirectory          the name of the directory
  * @return     void
  *
  */
  NS_IMETHOD SetDisplayDirectory(nsString& aDirectory) = 0;

 /**
  * Get the directory that the file open/save dialog was last displaying
  *
  * @param      aDirectory          the name of the directory
  * @return     void
  *
  */
  NS_IMETHOD GetDisplayDirectory(nsString& aDirectory) = 0;


};

#endif // nsIFileWidget_h__

