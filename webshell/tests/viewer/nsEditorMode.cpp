/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsEditorMode.h"
#include "nsString.h"
#include "nsIDOMDocument.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITextEditor.h"
#include "nsEditorCID.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "resources.h"

static nsIHTMLEditor *gEditor;

static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);
static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_CID(kEditorCID, NS_EDITOR_CID);

#ifdef XP_PC
#define EDITOR_DLL "ender.dll"
#else
#ifdef XP_MAC
#define EDITOR_DLL "ENDER_DLL"
#else // XP_UNIX || XP_BEOS
#define EDITOR_DLL "libender"MOZ_DLL_SUFFIX
#endif
#endif


nsresult NS_InitEditorMode(nsIDOMDocument *aDOMDocument, nsIPresShell* aPresShell)
{
  nsresult result = NS_OK;

  // This is a big leak if called > once, but its only temp test code, correct?
  if (gEditor)
    gEditor=nsnull;

  NS_ASSERTION(nsnull!=aDOMDocument, "null document");
  NS_ASSERTION(nsnull!=aPresShell, "null presentation shell");

  if ((nsnull==aDOMDocument) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

/*
  nsISupports *isup = nsnull;
  result = nsServiceManager::GetService(kTextEditorCID,
                                        kITextEditorIID, &isup);
*/
  result = nsComponentManager::CreateInstance(kHTMLEditorCID,
                                        nsnull,
                                        kIHTMLEditorIID, (void **)&gEditor);
  if (NS_FAILED(result))
    return result;
  if (!gEditor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result = gEditor->Init(aDOMDocument, aPresShell);
  return result;
}


static nsresult PrintEditorOutput(nsIHTMLEditor* editor, PRInt32 aCommandID)
{
	nsString		outString;
	char*			cString;
	nsString		formatString;
    PRUint32        flags;
	
	switch (aCommandID)
	{
      case VIEWER_DISPLAYTEXT:
        formatString = "text/plain";
        flags = nsIEditor::EditorOutputFormatted;
        gEditor->OutputToString(outString, formatString, flags);
        break;
        
      case VIEWER_DISPLAYHTML:
        formatString = "text/html";
        gEditor->OutputToString(outString, formatString, flags);
        break;
	}

	cString = outString.ToNewCString();
	printf(cString);
	delete [] cString;
	
	return NS_OK;
}


nsresult NS_DoEditorTest(PRInt32 aCommandID)
{
  if (gEditor)
  {
    switch(aCommandID)
    {
      case VIEWER_EDIT_SET_BGCOLOR_RED:
        gEditor->SetBodyAttribute("bgcolor", "red");
        break;
      case VIEWER_EDIT_SET_BGCOLOR_YELLOW:
        gEditor->SetBodyAttribute("bgcolor", "yellow");
        break;
      case VIEWER_EDIT_INSERT_CELL:  
        gEditor->InsertTableCell(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_INSERT_COLUMN:
        gEditor->InsertTableColumn(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_INSERT_ROW:    
        gEditor->InsertTableRow(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_DELETE_TABLE:  
        gEditor->DeleteTable();
        break;    
      case VIEWER_EDIT_DELETE_CELL:  
        gEditor->DeleteTableCell(1);
        break;    
      case VIEWER_EDIT_DELETE_COLUMN:
        gEditor->DeleteTableColumn(1);
        break;    
      case VIEWER_EDIT_DELETE_ROW:
        gEditor->DeleteTableRow(1);
        break;    
      case VIEWER_EDIT_JOIN_CELL_RIGHT:
        gEditor->JoinTableCells();
        break;    
      case VIEWER_EDIT_JOIN_CELL_BELOW:
        gEditor->JoinTableCells();
        break;
      case VIEWER_DISPLAYTEXT:
      case VIEWER_DISPLAYHTML:
        PrintEditorOutput(gEditor, aCommandID);
        break;
     default:
        return NS_ERROR_NOT_IMPLEMENTED;    
    }
  }
  return NS_OK;
}

