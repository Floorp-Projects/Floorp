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


nsresult NS_InitEditorMode(nsIDOMDocument *aDOMDocument)
{
  nsresult result = NS_OK;
  static needsInit=PR_TRUE;

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
  gEditor->Init(aDOMDocument);

  return result;
}

nsresult RegisterEventListeners()
{
  nsIDOMEventReceiver *er;

  static NS_DEFINE_IID(kIDOMEventReceiverIID, NS_IDOMEVENTRECEIVER_IID);
  static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
  static NS_DEFINE_IID(kIDOMKeyListenerIID, NS_IDOMKEYLISTENER_IID);
  
  if (NS_OK == kDomDoc->QueryInterface(kIDOMEventReceiverIID, (void**) &er)) {
  
      nsIDOMEventListener * mouseListener;
      nsIDOMEventListener * keyListener;

      if (NS_OK == NS_NewEditorKeyListener(&keyListener)) {
        er->AddEventListener(keyListener, kIDOMKeyListenerIID);
      }
      if (NS_OK == NS_NewEditorMouseListener(&mouseListener)) {
        er->AddEventListener(mouseListener, kIDOMMouseListenerIID);
      }
  }
  
  return NS_OK;
}

nsresult nsDeleteLast()
{
  nsIDOMNode *mNode=nsnull;
  nsIDOMText *mText=nsnull;
  nsString mStr;
  PRInt32 mLength;

  if (NS_OK == nsGetCurrentNode(&mNode) && 
      NS_OK == mNode->QueryInterface(kIDOMTextIID, (void**)&mText)) {
    mText->GetData(mStr);
    mLength = mStr.Length();
    if (mLength > 0) {
      mText->DeleteData(mLength-1, 1);
    }
    NS_RELEASE(mText);
  }

  NS_IF_RELEASE(mNode);

  return NS_OK;
}

nsresult GetFirstTextNode(nsIDOMNode *aNode, nsIDOMNode **aRetNode)
{
  PRUint16 mType;
  PRBool mCNodes;
  
  *aRetNode = nsnull;

  aNode->GetNodeType(&mType);

  if (nsIDOMNode::ELEMENT_NODE == mType) {
    if (NS_OK == aNode->HasChildNodes(&mCNodes) && PR_TRUE == mCNodes) {
      nsIDOMNode *mNode, *mSibNode;

      aNode->GetFirstChild(&mNode);
      while(nsnull == *aRetNode) {
        GetFirstTextNode(mNode, aRetNode);
        mNode->GetNextSibling(&mSibNode);
        NS_RELEASE(mNode);
        mNode = mSibNode;
      }
      NS_IF_RELEASE(mNode);
    }
  }
  else if (nsIDOMNode::TEXT_NODE == mType) {
    *aRetNode = aNode;
    NS_ADDREF(aNode);
  }

  return NS_OK;
}

nsresult nsGetCurrentNode(nsIDOMNode ** aNode)
{
  if (kCurrentNode != nsnull) {
    *aNode = kCurrentNode;
    NS_ADDREF(kCurrentNode);
    return NS_OK;
  }
  
  /* If no node set, get first text node */
  nsIDOMNode *mDocNode = nsnull, *mFirstTextNode;

  if (NS_OK == kDomDoc->GetFirstChild(&mDocNode) && 
      NS_OK == GetFirstTextNode(mDocNode, &mFirstTextNode)) {
    nsSetCurrentNode(mFirstTextNode);
    NS_RELEASE(mFirstTextNode);
    NS_RELEASE(mDocNode);
    *aNode = kCurrentNode;
    NS_ADDREF(kCurrentNode);
    return NS_OK;
  }

  NS_IF_RELEASE(mDocNode);
  return NS_ERROR_FAILURE;
}

nsresult nsSetCurrentNode(nsIDOMNode * aNode)
{
  NS_IF_RELEASE(kCurrentNode);
  kCurrentNode = aNode;
  NS_ADDREF(kCurrentNode);
  return NS_OK;
}

nsresult nsAppendText(nsString *aStr)
{
  nsIDOMNode *mNode = nsnull;
  nsIDOMText *mText;

  if (NS_OK == nsGetCurrentNode(&mNode) && 
      NS_OK == mNode->QueryInterface(kIDOMTextIID, (void**)&mText)) {
    mText->AppendData(*aStr);
    NS_RELEASE(mText);
  }

  NS_IF_RELEASE(mNode);

  return NS_OK;
}
