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

#ifndef _nsfilewidget_h
#define _nsfilewidget_h

// File dialog wrapper.
//
// This is really a temporary thing - eventually want to replace with
// some better things, especially if 'select directory' appears.
//
// Want to do some kind of mode filtering - like EAs are meant to work,
// but better.

class nsIWidget;
class nsIAppShell;
class nsIDeviceContext;

#include "nsIFileWidget.h"

class nsFileDialog : public nsIFileWidget
{
 public:
   nsFileDialog();
   virtual ~nsFileDialog();

   NS_DECL_ISUPPORTS

   // nsIFileWidget
   NS_IMETHOD Create( nsIWidget *aParent, const nsString &aTitle,
                      nsFileDlgMode aMode, nsIDeviceContext *aContext = nsnull,
                      nsIAppShell *aAppShell = nsnull,
                      nsIToolkit *aToolkit  = nsnull, void *aInitData = 0);
   NS_IMETHOD SetFilterList( PRUint32 aNumberOfFilters,
                             const nsString aTitles[],
                             const nsString aFilters[]);
   NS_IMETHOD GetSelectedType( PRInt16 &theType);
   virtual    PRBool Show();
   NS_IMETHOD GetFile( nsFileSpec &aFile);
   NS_IMETHOD SetDefaultString( const nsString &aFilename);
   NS_IMETHOD SetDisplayDirectory( const nsFileSpec &aDirectory);
   NS_IMETHOD GetDisplayDirectory( nsFileSpec &aDirectory);

   nsFileDlgResults GetFile( nsIWidget *aParent, const nsString &promptString,
                             nsFileSpec &theFileSpec);
     
   nsFileDlgResults GetFolder( nsIWidget *aParent, const nsString &promptString,
                               nsFileSpec &theFileSpec);
     
   nsFileDlgResults PutFile( nsIWidget *aParent, const nsString &promptString,
                             nsFileSpec &theFileSpec);

 protected:
   HWND     mWndOwner;
   FILEDLG  mFileDlg;
   PRUint32 mCFilters;
};

#endif
