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
#include "nsIDOMNode.h"
#include "nsIDOMElement.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMHTMLInputElement.h"
#include "nsIDOMHTMLButtonElement.h"

// FOr JS Execution
#include "nsIScriptContextOwner.h"
#include "nsIWebShell.h"

#include "nsRepository.h"

#include "nsXULCommand.h"

//----------------------------------------------------------------------

// Class IID's
static NS_DEFINE_IID(kXULCommandCID,           NS_XULCOMMAND_CID);

// IID's
static NS_DEFINE_IID(kIXULCommandIID,          NS_IXULCOMMAND_IID);
static NS_DEFINE_IID(kIFactoryIID,             NS_IFACTORY_IID);
static NS_DEFINE_IID(kIDOMNodeIID,             NS_IDOMNODE_IID);
static NS_DEFINE_IID(kISupportsIID,            NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID,    NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,      NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMEventReceiverIID,    NS_IDOMEVENTRECEIVER_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID, NS_IDOMHTMLINPUTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLButtonElement,   NS_IDOMHTMLBUTTONELEMENT_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID,  NS_ISCRIPTCONTEXTOWNER_IID);

//----------------------------------------------------------------------

//----------------------------------------------------------------------
// Note: operator new zeros our memory
nsXULCommand::nsXULCommand()
{
  NS_INIT_REFCNT();
  mWebShell   = nsnull;
  mDOMElement = nsnull;
  mIsEnabled  = PR_FALSE;
}

//----------------------------------------------------------------------
nsXULCommand::~nsXULCommand()
{
  NS_IF_RELEASE(mWebShell);
  NS_IF_RELEASE(mDOMElement);

  while (mSrcWidgets.Count() > 0) {
    nsIDOMNode* node = (nsIDOMNode*) mSrcWidgets.ElementAt(0);
    mSrcWidgets.RemoveElementAt(0);
    NS_RELEASE(node);
  }
}


NS_IMPL_ADDREF(nsXULCommand)
NS_IMPL_RELEASE(nsXULCommand)

//----------------------------------------------------------------------
nsresult
nsXULCommand::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv = NS_NOINTERFACE;

  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIXULCommandIID)) {
    *aInstancePtr = (void*)(nsIXULCommand*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIDOMMouseListenerIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIDOMMouseListener*)this);
    return NS_OK;
  }
  if (aIID.Equals(kIDOMKeyListenerIID)) {
    NS_ADDREF_THIS(); // Increase reference count for caller
    *aInstancePtr = (void *)((nsIDOMKeyListener*)this);
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtr = (void*)(nsISupports*)(nsIXULCommand*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return rv;
}


// XUL UI Objects
//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetName(const nsString &aName)
{
  mName = aName;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::GetName(nsString &aName) const
{
  aName = mName;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::AddUINode(nsIDOMNode * aNode)
{
  nsIDOMEventReceiver * receiver;

  NS_PRECONDITION(nsnull != aNode, "adding event listener to null node");

  if (NS_OK == aNode->QueryInterface(kIDOMEventReceiverIID, (void**) &receiver)) {
    receiver->AddEventListener((nsIDOMMouseListener*)this, kIDOMMouseListenerIID);
    receiver->AddEventListener((nsIDOMKeyListener*)this, kIDOMKeyListenerIID);
    mSrcWidgets.AppendElement(aNode);
    NS_ADDREF(aNode);
    NS_RELEASE(receiver);
    return NS_OK;
  }
  return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::RemoveUINode(nsIDOMNode * aCmd)
{
  PRInt32 index = mSrcWidgets.IndexOf(aCmd);
  if (index > 0) {
    mSrcWidgets.RemoveElementAt(index);
  }
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetEnabled(PRBool aIsEnabled)
{
  mIsEnabled = aIsEnabled;
  PRInt32 i, n = mSrcWidgets.Count();
  for (i = 0; i < n; i++) {
    nsIDOMNode* node = (nsIDOMNode*) mSrcWidgets.ElementAt(i);
    nsIDOMHTMLInputElement * input;
    if (NS_OK == node->QueryInterface(kIDOMHTMLInputElementIID,(void**)&input)) {
      input->SetDisabled(aIsEnabled);
      NS_RELEASE(input);
    } else {
      nsIDOMHTMLButtonElement * btn;
      if (NS_OK == node->QueryInterface(kIDOMHTMLButtonElement,(void**)&btn)) {
        btn->SetDisabled(aIsEnabled);
        NS_RELEASE(btn);
      }
    }
  }
  
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::GetEnabled(PRBool & aIsEnabled)
{
  aIsEnabled = mIsEnabled;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetCommand(const nsString & aStrCmd)
{
  mCommandStr = aStrCmd;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::DoCommand()
{
  //printf("DoCommand mWebShell is 0x%x\n", mWebShell);
  return ExecuteJavaScriptString(mWebShell, mCommandStr);
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetWebShell(nsIWebShell * aWebShell)
{
  mWebShell = aWebShell;
  NS_ADDREF(mWebShell);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetDOMElement(nsIDOMElement * aDOMElement)
{
  mDOMElement = aDOMElement;
  NS_ADDREF(mDOMElement);
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::ExecuteJavaScriptString(nsIWebShell* aWebShell, nsString& aJavaScript)
{
  if (0 == aJavaScript.Length()) {
    return NS_ERROR_FAILURE;
  }
  nsresult status;

  NS_ASSERTION(nsnull != aWebShell, "null webshell passed to EvaluateJavaScriptString");

  // Get nsIScriptContextOwner
  nsIScriptContextOwner* scriptContextOwner;
  status = aWebShell->QueryInterface(kIScriptContextOwnerIID,(void**)&scriptContextOwner);
	if (NS_OK == status) {
    const char* url = "";
      // Get nsIScriptContext
    nsIScriptContext* scriptContext;
    status = scriptContextOwner->GetScriptContext(&scriptContext);
    if (NS_OK == status) {
      // Ask the script context to evalute the javascript string
      PRBool isUndefined;
      nsString rVal;
      scriptContext->EvaluateString(aJavaScript, url, 0, rVal, &isUndefined);

      NS_RELEASE(scriptContext);
    } 		NS_RELEASE(scriptContextOwner);

  }
  return status;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetTooltip(const nsString &aTip)
{
  mTooltip = aTip;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::GetTooltip(nsString &aTip) const
{
  aTip = mTooltip;
  return NS_OK;
}
//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::SetDescription(const nsString &aDescription)
{
  mDescription = aDescription;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsXULCommand::GetDescription(nsString &aDescription) const
{
  aDescription = mDescription;
  return NS_OK;
}

//-----------------------------------------------------------------
//-- nsIDOMMouseListener
//-----------------------------------------------------------------

//-----------------------------------------------------------------
nsresult nsXULCommand::ProcessEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseDown(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseClick(nsIDOMEvent* aMouseEvent)
{
  //printf("Executing [%s]\n", mCommandStr.ToNewCString());
  DoCommand();
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return NS_OK;
}

//-----------------------------------------------------------------
//-----------------------------------------------------------------
// nsIDOMKeyListener 
//-----------------------------------------------------------------
nsresult nsXULCommand::KeyDown(nsIDOMEvent* aKeyEvent)
{
  PRUint32 type;
  aKeyEvent->GetKeyCode(&type);
  //printf("Key KeyDown: [%d]\n", type);
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::KeyUp(nsIDOMEvent* aKeyEvent)
{
  PRUint32 type;
  aKeyEvent->GetKeyCode(&type);
  //printf("Key Pressed: [%d]\n", type);
  if (nsIDOMEvent::VK_RETURN != type) {
    return NS_OK;
  }
  nsIDOMHTMLInputElement * input;
  if (NS_OK == mDOMElement->QueryInterface(kIDOMHTMLInputElementIID,(void**)&input)) {
    nsAutoString value;
    input->GetValue(value);
    //printf("Value [%s]\n", value.ToNewCString());
    mWebShell->LoadURL(value);
    NS_RELEASE(input);
  }
  return NS_OK;
}

//-----------------------------------------------------------------
nsresult nsXULCommand::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
//----------------------------------------------------------------------
// Factory code for creating nsXULCommand's

class nsXULCommandFactory : public nsIFactory
{
public:
  nsXULCommandFactory();
  virtual ~nsXULCommandFactory();

  // nsISupports methods
  NS_IMETHOD QueryInterface(const nsIID &aIID, void **aResult);
  NS_IMETHOD_(nsrefcnt) AddRef(void);
  NS_IMETHOD_(nsrefcnt) Release(void);

  // nsIFactory methods
  NS_IMETHOD CreateInstance(nsISupports *aOuter,
                            const nsIID &aIID,
                            void **aResult);

  NS_IMETHOD LockFactory(PRBool aLock);

private:
  nsrefcnt  mRefCnt;
};


nsXULCommandFactory::nsXULCommandFactory()
{
  mRefCnt = 0;
}

nsXULCommandFactory::~nsXULCommandFactory()
{
  NS_ASSERTION(mRefCnt == 0, "non-zero refcnt at destruction");
}

nsresult
nsXULCommandFactory::QueryInterface(const nsIID &aIID, void **aResult)
{
  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aResult = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;
  } else if (aIID.Equals(kIFactoryIID)) {
    *aResult = (void *)(nsIFactory*)this;
  }

  if (*aResult == NULL) {
    return NS_NOINTERFACE;
  }

  NS_ADDREF_THIS();  // Increase reference count for caller
  return NS_OK;
}

nsrefcnt
nsXULCommandFactory::AddRef()
{
  return ++mRefCnt;
}

nsrefcnt
nsXULCommandFactory::Release()
{
  if (--mRefCnt == 0) {
    delete this;
    return 0; // Don't access mRefCnt after deleting!
  }
  return mRefCnt;
}

nsresult
nsXULCommandFactory::CreateInstance(nsISupports *aOuter,
                                  const nsIID &aIID,
                                  void **aResult)
{
  nsresult rv;
  nsXULCommand *inst;

  if (aResult == NULL) {
    return NS_ERROR_NULL_POINTER;
  }
  *aResult = NULL;
  if (nsnull != aOuter) {
    rv = NS_ERROR_NO_AGGREGATION;
    goto done;
  }

  NS_NEWXPCOM(inst, nsXULCommand);
  if (inst == NULL) {
    rv = NS_ERROR_OUT_OF_MEMORY;
    goto done;
  }

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);

done:
  return rv;
}

nsresult
nsXULCommandFactory::LockFactory(PRBool aLock)
{
  // Not implemented in simplest case.
  return NS_OK;
}

extern "C" nsresult
NS_NewXULCommandFactory(nsIFactory** aFactory)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsXULCommandFactory();
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aFactory = inst;
  return rv;
}
