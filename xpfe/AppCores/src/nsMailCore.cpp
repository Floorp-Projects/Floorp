
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

#include "nsMailCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsRepository.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"


// Globals
static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIMailCoreIID,              NS_IDOMMAILCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::IID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::IID());

static NS_DEFINE_IID(kMailCoreCID,               NS_MailCore_CID);
static NS_DEFINE_IID(kBrowserWindowCID,          NS_BROWSER_WINDOW_CID);


/////////////////////////////////////////////////////////////////////////
// nsMailCore
/////////////////////////////////////////////////////////////////////////

nsMailCore::nsMailCore()
{
  printf("Created nsMailCore\n");

  mScriptObject   = nsnull;
  mScriptContext  = nsnull;
  mWindow         = nsnull;

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsMailCore::~nsMailCore()
{
  NS_IF_RELEASE(mScriptContext);
  NS_IF_RELEASE(mWindow);
  DecInstanceCount();  
}


NS_IMPL_ADDREF(nsMailCore)
NS_IMPL_RELEASE(nsMailCore)


NS_IMETHODIMP 
nsMailCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIMailCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMMailCore*)this);
    AddRef();
    return NS_OK;
  }
 
  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsMailCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptMailCore(aContext, 
                                (nsISupports *)(nsIDOMMailCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP    
nsMailCore::Init(const nsString& aId)
{
   
  nsBaseAppCore::Init(aId);

	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
}

NS_IMETHODIMP    
nsMailCore::MailCompleteCallback(const nsString& aScript)
{
  mScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsMailCore::SetWindow(nsIDOMWindow* aWin)
{
  mWindow = aWin;
  NS_ADDREF(aWin);
  mScriptContext = GetScriptContext(aWin);
	return NS_OK;
}

NS_IMETHODIMP    
nsMailCore::SendMail(const nsString& aAddrTo, const nsString& aSubject, const nsString& aMsg)
{
  if (nsnull == mScriptContext) {
    return NS_ERROR_FAILURE;
  }

  printf("----------------------------\n");
  printf("-- Sending Mail Message\n");
  printf("----------------------------\n");
  printf("To: %s  \nSub: %s  \nMsg: %s\n", aAddrTo.ToNewCString(), aSubject.ToNewCString(), aMsg.ToNewCString());
  printf("----------------------------\n");

  if (nsnull != mScriptContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;
    mScriptContext->EvaluateString(mScript, url, 0, rVal, &isUndefined);
    //printf("SendMail [%s] %d [%s]\n", mScript.ToNewCString(), isUndefined, rVal.ToNewCString());
  } 
  return NS_OK;
}




