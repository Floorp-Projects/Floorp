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

#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

static nsIHTMLEditor *gEditor;

static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_CID(kEditorCID, NS_EDITOR_CID);
static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID); // Still needed to register factory
//Don't think we these - switched to using HTMLEditor
//static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);

#ifdef XP_PC
#define EDITOR_DLL "ender.dll"
#else
#ifdef XP_MAC
#define EDITOR_DLL "ENDER_DLL"
#else // XP_UNIX
#define EDITOR_DLL "libender.so"
#endif
#endif

nsIHTMLEditor * GetEditor()
{
  return gEditor;
}

nsresult NS_InitEditorMode(nsIDOMDocument *aDOMDocument, nsIPresShell* aPresShell)
{
  nsresult result = NS_OK;
  static needsInit=PR_TRUE;
  if (gEditor)

  gEditor=nsnull;

  NS_ASSERTION(nsnull!=aDOMDocument, "null document");
  NS_ASSERTION(nsnull!=aPresShell, "null presentation shell");

  if ((nsnull==aDOMDocument) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  /** temp code until the editor auto-registers **/
  if (PR_TRUE==needsInit)
  {
    needsInit=PR_FALSE;
    result = nsRepository::RegisterComponent(kHTMLEditorCID, NULL, NULL, EDITOR_DLL, 
                                             PR_FALSE, PR_FALSE);
    if (NS_ERROR_FACTORY_EXISTS!=result)
    {
      if (NS_FAILED(result))
        return result;
    }
    result = nsRepository::RegisterComponent(kTextEditorCID, NULL, NULL, EDITOR_DLL, 
                                             PR_FALSE, PR_FALSE);
    if (NS_ERROR_FACTORY_EXISTS!=result)
    {
      if (NS_FAILED(result))
        return result;
    }
    result = nsRepository::RegisterComponent(kEditorCID, NULL, NULL, EDITOR_DLL, 
                                             PR_FALSE, PR_FALSE);
    if (NS_ERROR_FACTORY_EXISTS!=result)
    {
      if (NS_FAILED(result))
        return result;
    }
  }
  /** end temp code **/
/*
  nsISupports *isup = nsnull;
  result = nsServiceManager::GetService(kHTMLEditorCID,
                                        kIHTMLEditorIID, &isup);
*/
  result = nsRepository::CreateInstance(kHTMLEditorCID,
                                        nsnull,
                                        kIHTMLEditorIID, (void **)&gEditor);
  if (NS_FAILED(result))
    return result;
  if (!gEditor) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  gEditor->InitHTMLEditor(aDOMDocument, aPresShell);
  gEditor->EnableUndo(PR_TRUE);

  return result;
}

