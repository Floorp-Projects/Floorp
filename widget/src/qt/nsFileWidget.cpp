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

#include "nsFileWidget.h"
#include "nsWidget.h"

NS_IMPL_ISUPPORTS(nsFileWidget, nsIFileWidget::GetIID());

//-------------------------------------------------------------------------
//
// nsFileWidget constructor
//
//-------------------------------------------------------------------------
nsFileWidget::nsFileWidget() : nsIFileWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::nsFileWidget()\n"));
    NS_INIT_REFCNT();
    mWidget          = nsnull;
    mMode            = eMode_load;
    mNumberOfFilters = 0;
    mTitles          = nsnull;
    mFilters         = nsnull;
}

//-------------------------------------------------------------------------
//
// nsFileWidget destructor
//
//-------------------------------------------------------------------------
nsFileWidget::~nsFileWidget()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::~nsFileWidget()\n"));
    if (mWidget)
    {
        delete mWidget;
        mWidget = nsnull;
    }
}

//-------------------------------------------------------------------------
//
// Show - Display the file dialog
//
//-------------------------------------------------------------------------
PRBool nsFileWidget::Show()
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::Show()\n"));
    // make things shorter
    PRBool ret;

	if (mWidget)
	{
    	mWidget->show();

#if 0
    	QObject::connect(mWidget, 
                    	 SIGNAL(clicked()), 
                	     SLOT());
    	QObject::connect(mWidget, 
        	             SIGNAL(clicked()), 
            	         SLOT());
#endif

  	  	int result = mWidget->result();

    	ret = (result == QDialog::Accepted);
	}
    return ret;
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::SetFilterList()\n"));
    QStringList strList;
    QString  filter;

    mNumberOfFilters  = aNumberOfFilters;
    mTitles           = aTitles;
    mFilters          = aFilters;

    for (PRUint32 i = 0; i < mNumberOfFilters; i++)
    {
        char * t = aTitles[i].ToNewCString();
        char * f = aFilters[i].ToNewCString();

        if (t && f)
        {
            filter = t;
            filter += "(";
            filter += f;
            filter += ")";

            strList.append((const char *) filter);
        }

        delete [] t;
        delete [] f;
    }

	if (mWidget)
	{
    	mWidget->setFilters(strList);
	}

    return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::GetSelectedType(PRInt16& theType)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::nsFileWidget()\n"));
    theType = mSelectedType;
    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the file + path
//
//-------------------------------------------------------------------------

NS_METHOD nsFileWidget::GetFile(nsFileSpec& aFile)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::GetFile()\n"));
	if (mWidget)
	{
    	QString dir = mWidget->dirPath();
    	QString file = mWidget->selectedFile();

    	aFile = (const char *)(dir + "/" + file);
	}

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Set the default file
//
//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::SetDefaultString(const nsString& aString)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::SetDefaultString()\n"));
    char *fn = aString.ToNewCString();
	if (mWidget)
	{
    	mWidget->setSelection(fn);
	}
    delete [] fn;
    return NS_OK;
}


//-------------------------------------------------------------------------
//
// Set the display directory
//
//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::SetDisplayDirectory(const nsFileSpec& aDirectory)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::SetDisplayDirectory()\n"));
    mDisplayDirectory = aDirectory;

    char * dir = mDisplayDirectory.ToNewCString();

	if (mWidget)
	{
    	mWidget->setDir(dir);
	}

    delete [] dir;

    return NS_OK;
}

//-------------------------------------------------------------------------
//
// Get the display directory
//
//-------------------------------------------------------------------------
NS_METHOD nsFileWidget::GetDisplayDirectory(nsFileSpec& aDirectory)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::GetDisplayDirectory()\n"));
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
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::Create()\n"));
    QFileDialog::Mode mode;
    mMode = aMode;
    mTitle.SetLength(0);
    mTitle.Append(aTitle);
    char *title = mTitle.ToNewCString();

    mWidget = new QFileDialog(nsnull, QFileDialog::tr("nsFileWidget"), TRUE);

    if (mWidget)
    {
        mWidget->setCaption(title);

        switch (mMode)
        {
        case eMode_load:
            mode = QFileDialog::ExistingFile;
            break;
        case eMode_save:
            mode = QFileDialog::AnyFile;
            break;
        case eMode_getfolder:
            mode = QFileDialog::Directory;
            break;
        default:
            mode = QFileDialog::AnyFile;
            break;
        }

        mWidget->setMode(mode);
    }

    delete [] title;

    return NS_OK;
}


nsFileDlgResults nsFileWidget::GetFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::GetFile()\n"));
	Create(aParent, promptString, eMode_load, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

    return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::GetFolder(nsIWidget *aParent,
                                         const nsString &promptString,
                                         nsFileSpec &theFileSpec)
{
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::GetFolder()\n"));
	Create(aParent, promptString, eMode_getfolder, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

    return nsFileDlgResults_Cancel;
}

nsFileDlgResults nsFileWidget::PutFile(nsIWidget *aParent,
                                       const nsString &promptString,
                                       nsFileSpec &theFileSpec)
{ 
    PR_LOG(QtWidgetsLM, PR_LOG_DEBUG, ("nsFileWidget::PutFile()\n"));
	Create(aParent, promptString, eMode_save, nsnull, nsnull);
	if (Show() == PR_TRUE)
	{
		GetFile(theFileSpec);
		return nsFileDlgResults_OK;
	}

    return nsFileDlgResults_Cancel; 
}
