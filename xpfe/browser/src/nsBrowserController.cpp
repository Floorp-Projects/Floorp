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

#include "nsBrowserController.h"
#include "nsXPComFactory.h"

#include "nsIDOMWindow.h"
#include "nsIDOMHTMLDocument.h"
#include "nsIDOMHTMLIFrameElement.h"
#include "nsIDOMHTMLInputElement.h"

#include "nsIWebShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"

static NS_DEFINE_IID(kIDOMWindowIID,        NS_IDOMWINDOW_IID);
static NS_DEFINE_IID(kIDOMHTMLDocumentIID,  NS_IDOMHTMLDOCUMENT_IID);
static NS_DEFINE_IID(kIDOMMouseListenerIID, NS_IDOMMOUSELISTENER_IID);
static NS_DEFINE_IID(kIDOMKeyListenerIID,   NS_IDOMKEYLISTENER_IID);
static NS_DEFINE_IID(kIDOMFormListenerIID,  NS_IDOMFORMLISTENER_IID);
static NS_DEFINE_IID(kIDOMLoadListenerIID,  NS_IDOMLOADLISTENER_IID);

static NS_DEFINE_IID(kIWebShellIID,             NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIScriptContextOwnerIID,   NS_ISCRIPTCONTEXTOWNER_IID);
static NS_DEFINE_IID(kIDOMHTMLIFrameElementIID, NS_IDOMHTMLIFRAMEELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLInputElementIID,  NS_IDOMHTMLINPUTELEMENT_IID);


nsBrowserController::nsBrowserController()
{
  mLocationForm = nsnull;
  mForwardBtn   = nsnull;
  mBackBtn      = nsnull;
  mURLTypeIn    = nsnull;
  mWebWindow    = nsnull;
  mWebShell     = nsnull;
}


nsBrowserController::~nsBrowserController()
{
  NS_IF_RELEASE(mLocationForm);
  NS_IF_RELEASE(mForwardBtn);
  NS_IF_RELEASE(mBackBtn);
  NS_IF_RELEASE(mURLTypeIn);
  NS_IF_RELEASE(mWebWindow);
  NS_IF_RELEASE(mWebShell);
}


NS_IMETHODIMP
nsBrowserController::Initialize(nsIDOMDocument* aDocument, nsISupports* aContainer) 
{
  nsresult rv = NS_OK;
  nsIDOMHTMLDocument* doc = nsnull;
  /* Argument validation... */
  if ((nsnull == aContainer) || (nsnull == aDocument)) {
    rv = NS_ERROR_NULL_POINTER;
    goto done;
  }
  // XXX:  What happens if this fails?
  rv = aContainer->QueryInterface(kIWebShellIID, (void**)&mWebShell);
  /*
   * If a web shell was found, then hook up an IDOMLoadEvent listener
   */

  // Locate the nsIWebShell that represents the browser.webwindow IFrame...
  // This webshell is not availableuntil the entire document has
  // been loaded...
  if (NS_SUCCEEDED(rv)) {
    nsAutoString name("browser.webwindow");

    rv = mWebShell->FindChildWithName(name.GetUnicode(), mWebWindow);
    /*
     * If a web shell was found, then hook up an IDOMLoadEvent listener
     */
    if (nsnull != mWebWindow) {
      nsIScriptContextOwner* owner;

      rv = mWebWindow->QueryInterface(kIScriptContextOwnerIID, (void**)&owner);
      if (NS_SUCCEEDED(rv)) {
        nsIScriptGlobalObject* scriptGlobal = nsnull;

        rv = owner->GetScriptGlobalObject(&scriptGlobal);
        if (NS_SUCCEEDED(rv)) {
          /* Attach the DOMLoadEvent listener... */
          AddEventListener(scriptGlobal, kIDOMLoadListenerIID);
          NS_RELEASE(scriptGlobal);
        }
        NS_RELEASE(owner);
      }
    }
  }

  rv = aDocument->QueryInterface(kIDOMHTMLDocumentIID,(void **)&doc);
  if (NS_SUCCEEDED(rv)) {
    doc->GetElementById("browser.toolbar",        &mLocationForm);
    doc->GetElementById("browser.toolbar.back",   &mBackBtn);
    doc->GetElementById("browser.toolbar.forward",&mForwardBtn);
    doc->GetElementById("browser.toolbar.url",    &mURLTypeIn);

    AddEventListener(mBackBtn,    kIDOMMouseListenerIID);
    AddEventListener(mForwardBtn, kIDOMMouseListenerIID);
    AddEventListener(mURLTypeIn,  kIDOMKeyListenerIID);

    NS_RELEASE(doc);
  }

  doUpdateToolbarState();
done:
  return rv;
}


/* 
 * Event handler for mouse clicks...
 *
 * This routine handles the forward and back buttons...
 */
nsresult nsBrowserController::MouseClick(nsIDOMEvent* aMouseEvent)
{
  nsIDOMNode * node = nsnull;

  aMouseEvent->GetTarget(&node);
  if ((nsnull != mWebWindow) && (nsnull != node)) {
    if (mBackBtn == node) {
      mWebWindow->Back();
    } else if (mForwardBtn == node) {
      mWebWindow->Forward();
    }
  }
  NS_IF_RELEASE(node);
  return NS_OK;
}


nsresult nsBrowserController::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsresult rv;
  PRUint32 key;

  rv = aKeyEvent->GetKeyCode(&key);
  if (NS_SUCCEEDED(rv)) {
    // If ENTER was pressed in the URL type-in then change to the new URL...
    if (0x0d == key) {

      if ((nsnull != mWebWindow) && (nsnull != mURLTypeIn)) {
        nsIDOMHTMLInputElement* element;
        rv = mURLTypeIn->QueryInterface(kIDOMHTMLInputElementIID, (void**) &element);
        // Get the contents of the URL type-in...
        if (NS_SUCCEEDED(rv)) {
          nsAutoString name;

          element->GetValue(name);
          mWebWindow->LoadURL(name.GetUnicode());
          NS_RELEASE(element);
        }
      }
    }
  }

  return NS_OK;
}

nsresult nsBrowserController::Load(nsIDOMEvent* aLoadEvent)
{
  nsresult rv = NS_OK;

  doUpdateToolbarState();

  return rv;
}

nsresult nsBrowserController::Unload(nsIDOMEvent* aLoadEvent)
{
  return NS_OK;
}

void nsBrowserController::doUpdateToolbarState(void)
{
  nsresult rv;
  nsIDOMHTMLInputElement* element;

  if (nsnull != mWebWindow) {
    /* Update the state of the back button... */
    if (nsnull != mBackBtn) {
      rv = mBackBtn->QueryInterface(kIDOMHTMLInputElementIID, (void**)&element);
      if (NS_SUCCEEDED(rv)) {
        rv = mWebWindow->CanBack();
        element->SetDisabled((rv == NS_OK) ? PR_FALSE : PR_TRUE);
        NS_RELEASE(element);
      }
    }
    /* Update the state of the forward button... */
    if (nsnull != mForwardBtn) {
      rv = mForwardBtn->QueryInterface(kIDOMHTMLInputElementIID, (void**)&element);
      if (NS_SUCCEEDED(rv)) {
        rv = mWebWindow->CanForward();
        element->SetDisabled((rv == NS_OK) ? PR_FALSE : PR_TRUE);
        NS_RELEASE(element);
      }
    }
    /* Update the value of the URL typein... */
    if (nsnull != mURLTypeIn) {
      rv = mURLTypeIn->QueryInterface(kIDOMHTMLInputElementIID, (void**)&element);
      if (NS_SUCCEEDED(rv)) {
        nsAutoString name;
        PRInt32 index = 0;
        const PRUnichar * url;

        mWebWindow->GetHistoryIndex(index);
        rv = mWebWindow->GetURL(index, &url);
        if (NS_SUCCEEDED(rv)) {
          name = url;
          element->SetValue(name);
        }
        NS_RELEASE(element);
      }
    }
  }
}


//----------------------------------------------------------------------

// Entry point to create nsBrowserController factory instances...
NS_DEF_FACTORY(BrowserController,nsBrowserController)


nsresult NS_NewBrowserControllerFactory(nsIFactory** aResult)
{
  nsresult rv = NS_OK;
  nsIFactory* inst = new nsBrowserControllerFactory;
  if (nsnull == inst) {
    rv = NS_ERROR_OUT_OF_MEMORY;
  }
  else {
    NS_ADDREF(inst);
  }
  *aResult = inst;
  return rv;
}
