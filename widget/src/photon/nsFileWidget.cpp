/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either ex7press or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsFileWidget.h"
#include "nsIToolkit.h"

#include <Pt.h>
#include "nsPhWidgetLog.h"

NS_DEFINE_IID(kIFileWidgetIID, NS_IFILEWIDGET_IID);
NS_IMPL_ISUPPORTS(nsFileWidget, kIFileWidgetIID);

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
{
  NS_INIT_REFCNT();
  mWidget = nsnull;
  mParent = nsnull;
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
  if (mWidget)
  	PtDestroyWidget(mWidget);
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::Show()
{
  int         err;
  PRBool      res = PR_FALSE;
  char       *title = mTitle.ToNewCString();
  PtWidget_t *myParent = nsnull;

  if (mParent)
  {
    myParent = (PtWidget_t *) mParent->GetNativeData(NS_NATIVE_WIDGET);
  }
    
  PhPoint_t thePos= {100,100};
  char      *root_dir = mDisplayDirectory.ToNewCString();
  char      *file_spec = nsnull;
  char      *btn1 = nsnull;
  char      *btn2 = nsnull;
  char      format[] = "100n50d";			/* 100 Pix for Name, 50 Pix for Date */
  int       flags = 0;
  PtFileSelectionInfo_t    info;
  
  /* NULL out the structure else it will be a whacky size.. not sure if this is a bug or not... */
  memset(&info, NULL, sizeof info);
    
  if (mMode == eMode_load)
  {
    /* Load a File or directory */  
  }
  else
  {
   /* Save a File or directory */  
    flags = flags | Pt_FSDIALOG_NO_FCHECK;
  }

  int i;
  nsString mFilterList;
  for(i=0; i<(mNumberOfFilters-1); i++)
  {
    mFilterList.Append(mFilters[i]);
    mFilterList.AppendWithConversion(";");
  }

  mFilterList.Append(mFilters[i]);  /* Add the last one */

  file_spec = mFilterList.ToNewCString();  /* convert it into a string */
  
  if (myParent == nsnull)
    PtSetParentWidget( NULL );
	  
  err = PtFileSelection(myParent, &thePos, title, root_dir, file_spec, btn1, btn2, format, &info, flags);
  if (err == 0)
  {
	/* Successfully selected a file or directory */
    if (info.ret == Pt_FSDIALOG_BTN1)
	{
		mSelectedFile.SetLength(0);
		mSelectedFile.AppendWithConversion(info.path);
		res = PR_TRUE;
	}
	else
	{
		/* User pressed cancel */	
	}
  }
  
  delete [] title;
  delete [] root_dir;
  delete [] file_spec;
      
  return res;
}


//-------------------------------------------------------------------------
//
// Show - Display the replace dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::AskReplace()
{
  PRBool      res = PR_FALSE;

  /* Almost the same as ::Show Just different button labels */
  
  return res;
}
//-------------------------------------------------------------------------
//
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,
				      const nsString aTitles[],
				      const nsString aFilters[])
{
  mNumberOfFilters  = aNumberOfFilters;
  mTitles           = aTitles;
  mFilters          = aFilters;

  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the file + path
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDefaultString(const nsString& aString)
{
  mDefault = aString;  
  return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::SetDisplayDirectory(const nsFileSpec & aDirectory)
{
  mDisplayDirectory = (const PRUnichar *) aDirectory.GetCString(); /* LEAK? */
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  aDirectory = mDisplayDirectory;
  return NS_OK;
}


//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::Create(nsIWidget *aParent,
                               const nsString& aTitle,
                               nsFileDlgMode aMode,
                               nsIDeviceContext *aContext,
                               nsIAppShell *aAppShell,
                               nsIToolkit *aToolkit,
                               void *aInitData)
{
  mMode = aMode;
  mTitle.SetLength(0);
  mTitle.Append(aTitle);
  mParent = aParent;
  
  return NS_OK;
}



NS_METHOD  nsFileWidget::GetFile(nsFileSpec& aFile)
{
    char *str = mSelectedFile.ToNewCString();
    aFile = strdup(str);
    delete [] str;

  return NS_OK;
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
// Not Implemented
NS_METHOD  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  theType = mSelectedType;
  return NS_OK;
}

	
nsFileDlgResults nsFileWidget::GetFile(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog
{
	Create(aParent, promptString, eMode_load, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}
    
nsFileDlgResults nsFileWidget::GetFolder(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog 
{
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

  return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::PutFile(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog 
{
  nsFileDlgResults theResult = nsFileDlgResults_Cancel;
  
  Create(aParent, promptString, eMode_save, nsnull, nsnull);
  if (Show() == PR_TRUE)
  {
    GetFile(theFileSpec);
    if( theFileSpec.Exists() )
    {
      PRBool result = AskReplace();
      theResult = result ? nsFileDlgResults_Replace : nsFileDlgResults_Cancel;
      // Ask for replace dialog
    }
    else
    {
      theResult = nsFileDlgResults_OK;
    }
  }
  return theResult; 
}
