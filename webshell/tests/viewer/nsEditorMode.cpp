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
 
#include "nsCOMPtr.h"

#include "nsEditorMode.h"
#include "nsString.h"
#include "nsIDOMDocument.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITableEditor.h"
#include "nsIDocumentEncoder.h" // for output flags

#include "nsEditorCID.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "resources.h"

static nsIEditor *gEditor;

static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
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
                                        nsIEditor::GetIID(), (void **)&gEditor);
  if (NS_FAILED(result))
    return result;
  if (!gEditor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result = gEditor->Init(aDOMDocument, aPresShell, 0);
  if (NS_SUCCEEDED(result))
    result = gEditor->PostCreate();
  return result;
}


static nsresult PrintEditorOutput(nsIEditor* editor, PRInt32 aCommandID)
{
	nsString		outString;
	char*			cString;
	nsString		formatString;
    PRUint32        flags = 0;
	
	switch (aCommandID)
	{
      case VIEWER_DISPLAYTEXT:
        formatString = "text/plain";
        flags = nsIDocumentEncoder::OutputFormatted;
        editor->OutputToString(outString, formatString, flags);
        break;
        
      case VIEWER_DISPLAYHTML:
        formatString = "text/html";
        editor->OutputToString(outString, formatString, flags);
        break;
	}

	cString = outString.ToNewCString();
	printf(cString);
	delete [] cString;
	
	return NS_OK;
}


nsresult NS_DoEditorTest(PRInt32 aCommandID)
{
  nsCOMPtr<nsIHTMLEditor> htmlEditor = do_QueryInterface(gEditor);
  nsCOMPtr<nsITableEditor> tableEditor = do_QueryInterface(gEditor);
  if (htmlEditor && tableEditor)
  {
    switch(aCommandID)
    {
      case VIEWER_EDIT_SET_BGCOLOR_RED:
        htmlEditor->SetBodyAttribute("bgcolor", "red");
        break;
      case VIEWER_EDIT_SET_BGCOLOR_YELLOW:
        htmlEditor->SetBodyAttribute("bgcolor", "yellow");
        break;
      case VIEWER_EDIT_INSERT_CELL:  
        tableEditor->InsertTableCell(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_INSERT_COLUMN:
        tableEditor->InsertTableColumn(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_INSERT_ROW:    
        tableEditor->InsertTableRow(1, PR_FALSE);
        break;    
      case VIEWER_EDIT_DELETE_TABLE:  
        tableEditor->DeleteTable();
        break;    
      case VIEWER_EDIT_DELETE_CELL:  
        tableEditor->DeleteTableCell(1);
        break;    
      case VIEWER_EDIT_DELETE_COLUMN:
        tableEditor->DeleteTableColumn(1);
        break;    
      case VIEWER_EDIT_DELETE_ROW:
        tableEditor->DeleteTableRow(1);
        break;    
      case VIEWER_EDIT_JOIN_CELL_RIGHT:
        tableEditor->JoinTableCells();
        break;    
      case VIEWER_EDIT_JOIN_CELL_BELOW:
        tableEditor->JoinTableCells();
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

