
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsToolbarCore.h"
#include "nsIBrowserWindow.h"
#include "nsRepository.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMCharacterData.h"


// Globals
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIToolbarCoreIID,           NS_IDOMTOOLBARCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::IID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::IID());
static NS_DEFINE_IID(kIDOMCharacterDataIID,      nsIDOMCharacterData::IID());

static NS_DEFINE_IID(kToolbarCoreCID,            NS_TOOLBARCORE_CID);


/////////////////////////////////////////////////////////////////////////
// nsToolbarCore
/////////////////////////////////////////////////////////////////////////

nsToolbarCore::nsToolbarCore()
{
  printf("Created nsToolbarCore\n");

  mWindow         = nsnull;
  mStatusText     = nsnull;

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsToolbarCore::~nsToolbarCore()
{
  NS_IF_RELEASE(mWindow);
  NS_IF_RELEASE(mStatusText);
  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsToolbarCore)
NS_IMPL_RELEASE(nsToolbarCore)


NS_IMETHODIMP 
nsToolbarCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIToolbarCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMToolbarCore*)this);
    AddRef();
    return NS_OK;
  }
 
  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsToolbarCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptToolbarCore(aContext, 
                                (nsISupports *)(nsIDOMToolbarCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}



NS_IMETHODIMP    
nsToolbarCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);

	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
}




NS_IMETHODIMP    
nsToolbarCore::SetStatus(const nsString& aMsg)
{
  if (nsnull == mStatusText) {
    nsIDOMDocument * domDoc;
    mWindow->GetDocument(&domDoc);
    if (!domDoc)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMNode> parent(GetParentNodeFromDOMDoc(domDoc));
    if (!parent)
      return NS_ERROR_FAILURE;

    PRInt32 count = 0;
    nsCOMPtr<nsIDOMNode> statusNode(FindNamedDOMNode(nsAutoString("#text"), parent, count, 7));
    if (!statusNode)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIDOMCharacterData> charData(statusNode);
    if (!charData)
      return NS_ERROR_FAILURE;

    mStatusText = charData;
    mStatusText->SetData(nsAutoString("Ready.....")); // <<====== EVIL HARD-CODED STRING.
    NS_RELEASE(domDoc);
  }

  mStatusText->SetData(aMsg); 

  return NS_OK;
}

NS_IMETHODIMP    
nsToolbarCore::SetWindow(nsIDOMWindow* aWin)
{
  mWindow = aWin;
  NS_ADDREF(aWin);
	return NS_OK;
}





