/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; -*- */
/*
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

#include "nsFileWidget.h"

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
  mNumberOfFilters = 0;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::~nsFileWidget - Destructor Called\n"));

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

  PtWidget_t *myParent = (PtWidget_t *) mParent->GetNativeData(NS_NATIVE_WIDGET);

  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::Show myParent=<%p> - Not Implemented\n", myParent));
  
  PhPoint_t thePos= {100,100};
  char      *root_dir = mDisplayDirectory.ToNewCString();
  char      *file_spec = nsnull;
  char      *btn1 = nsnull;
  char      *btn2 = nsnull;
  char      format[] = "100n50d";			/* 100 Pix for Name, 50 Pix for Date */
  int       flags = 0;
  PtFileSelectionInfo_t    info;
    
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
	mFilterList.Append(";");
  }

  mFilterList.Append(mFilters[i]);  /* Add the last one */

  file_spec = mFilterList.ToNewCString();  /* convert it into a string */
  
  myParent = nsnull; /* HACK! */
  
  err = PtFileSelection(myParent, &thePos, title, root_dir, file_spec, btn1, btn2, format, &info, flags);
  if (err == 0)
  {
	/* Successfully selected a file or directory */
    if (info.ret == Pt_FSDIALOG_BTN1)
	{
		mSelectedFile.SetLength(0);
        mSelectedFile.Append(info.path);

        char *str = mSelectedFile.ToNewCString();
        PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::Create Success from PtFileSelection str=<%s>\n", str));
 		delete [] str;
		
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
// Set the list of filters
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::SetFilterList(PRUint32 aNumberOfFilters,
				      const nsString aTitles[],
				      const nsString aFilters[])
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::SetFilterList numFilters=<%d>\n", aNumberOfFilters));
#if 1
{
  int i;
  for(i=0; i<aNumberOfFilters; i++)
  {
    char *str1 = aTitles[i].ToNewCString();
    char *str2 = aFilters[i].ToNewCString();

    PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::SetFilterList %d Title=<%s> Filter=<%s>\n", i, str1, str2));

	delete [] str1;
	delete [] str2;	  
  }

}
#endif

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
  char *str = aString.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::SetDefaultString new string=<%s> - Not Implemented\n", str));
  delete [] str;

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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::SetDisplayDirectory to <%s> - Not Implemented\n", aDirectory.GetCString()));
  
  mDisplayDirectory = aDirectory;
  return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::GetDisplayDirectory  - Not Implemented\n"));
  
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
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::GetFile with nsFileSpec - Not Implemented\n"));
  
  nsresult res = NS_ERROR_FAILURE;
  return res;
}

//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
NS_METHOD  nsFileWidget::GetSelectedType(PRInt16& theType)
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::GetSelectedType - Not Implemented\n"));

  theType = mSelectedType;
  return NS_OK;
}

	
nsFileDlgResults nsFileWidget::GetFile(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::GetFile with nsIWidget - Not Implemented\n"));
  return nsFileDlgResults_OK;
}
    
nsFileDlgResults nsFileWidget::GetFolder(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog 
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::GetFolder with nsIWidget - Not Implemented\n"));
  return nsFileDlgResults_OK;
}
nsFileDlgResults nsFileWidget::PutFile(
      nsIWidget        * aParent,
      const nsString   & promptString,    // Window title for the dialog
      nsFileSpec       & theFileSpec)     // Populate with initial path for file dialog 
{
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsFileWidget::PutFile with nsIWidget - Not Implemented\n"));
  return nsFileDlgResults_OK;
}