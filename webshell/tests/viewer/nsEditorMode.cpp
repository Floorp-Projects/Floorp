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
#include "nsEditorInterfaces.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventCapturer.h"
#include "nsString.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"

#include "nsIEditor.h"
#include "nsEditorCID.h"

#include "nsRepository.h"
#include "nsIServiceManager.h"

static nsIDOMDocument* kDomDoc;
static nsIDOMNode* kCurrentNode;

static nsIEditor *gEditor;

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_CID(kEditorCID, NS_EDITOR_CID);

#ifdef XP_PC
#define EDITOR_DLL "ender.dll"
#else
#ifdef XP_MAC
#define EDITOR_DLL "ENDER_DLL"
#else // XP_UNIX
#define EDITOR_DLL "libender.so"
#endif
#endif


nsresult NS_InitEditorMode(nsIDOMDocument *aDOMDocument, nsIPresShell* aPresShell)
{
  nsresult result = NS_OK;
  static needsInit=PR_TRUE;

  NS_ASSERTION(nsnull!=aDOMDocument, "null document");
  NS_ASSERTION(nsnull!=aPresShell, "null presentation shell");

  if ((nsnull==aDOMDocument) || (nsnull==aPresShell))
    return NS_ERROR_NULL_POINTER;

  if (PR_TRUE==needsInit)
  {
    gEditor=(nsIEditor*)1;  // XXX: hack to get around null pointer problem
    needsInit=PR_FALSE;
    result = nsRepository::RegisterFactory(kEditorCID, EDITOR_DLL, 
                                           PR_TRUE, PR_TRUE);
    if (NS_FAILED(result))
      return result;
  }

  NS_IF_RELEASE(kCurrentNode);
  NS_IF_RELEASE(kDomDoc);
  
  kCurrentNode = nsnull;

  kDomDoc = aDOMDocument;
  NS_IF_ADDREF(kDomDoc);


  nsISupports *isup = nsnull;

  result = nsServiceManager::GetService(kEditorCID,
                                        kIEditorIID, &isup);
  if (NS_FAILED(result) || !isup) {
    printf("ERROR: Failed to get Editor nsISupports interface.\n");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  result = isup->QueryInterface(kIEditorIID, (void **)&gEditor);
  if (NS_FAILED(result)) {
    printf("ERROR: Failed to get Editor interface. (%d)\n", result);
    return result;
  }
  if (nsnull==gEditor) {
    printf("ERROR: QueryInterface() returned NULL pointer.\n");
    return NS_ERROR_NULL_POINTER;
  } 
  gEditor->Init(aDOMDocument, aPresShell);

  return result;
}
