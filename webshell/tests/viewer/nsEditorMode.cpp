/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
#include "nsCOMPtr.h"

#include "nsEditorMode.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIDOMDocument.h"

#include "nsIEditor.h"
#include "nsIHTMLEditor.h"
#include "nsITableEditor.h"
#include "nsIDocumentEncoder.h" // for output flags

#include "nsEditorCID.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "resources.h"
#include "nsISelectionController.h"
#include "nsIPresShell.h"

static nsIEditor *gEditor;

static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);

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

  result = CallCreateInstance(kHTMLEditorCID, &gEditor);
  if (NS_FAILED(result))
    return result;
  if (!gEditor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsCOMPtr<nsISelectionController> selCon;
  selCon = do_QueryInterface(aPresShell);
  result = gEditor->Init(aDOMDocument, aPresShell, nsnull, selCon, 0);
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
        formatString.AssignLiteral("text/plain");
        flags = nsIDocumentEncoder::OutputFormatted;
        editor->OutputToString(formatString, flags, outString);
        break;
        
      case VIEWER_DISPLAYHTML:
        formatString.AssignLiteral("text/html");
        editor->OutputToString(formatString, flags, outString);
        break;
	}

	cString = ToNewCString(outString);
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
        htmlEditor->SetBodyAttribute(NS_LITERAL_STRING("bgcolor"), NS_LITERAL_STRING("red"));
        break;
      case VIEWER_EDIT_SET_BGCOLOR_YELLOW:
        htmlEditor->SetBodyAttribute(NS_LITERAL_STRING("bgcolor"), NS_LITERAL_STRING("yellow"));
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
        tableEditor->JoinTableCells(PR_FALSE);
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

