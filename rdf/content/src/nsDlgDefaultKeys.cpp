/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsCOMPtr.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMXULDocument.h"
#include "nsINSEvent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDlgDefaultKeys.h"
#include "nsRDFCID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kDlgDefaultKeysCID,      NS_DLGDEFAULTKEYS_CID);
static NS_DEFINE_IID(kIDlgDefaultKeysIID,     NS_IDLGDEFAULTKEYS_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDomUIEventIID,         NS_IDOMUIEVENT_IID);

enum eEventType {
  eKeyPress,
  eKeyDown,
  eKeyUp
};

// DialogDefaultKeysImpl
//
//   This is the key listener implementation for default keys in dialogs
//
class nsDlgDefaultKeysImpl : public nsIDlgDefaultKeys,
                           public nsIDOMKeyListener
{
public:
    nsDlgDefaultKeysImpl(void);
    virtual ~nsDlgDefaultKeysImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIDlgDefaultKeys
    NS_IMETHOD Init(
      nsIDOMElement  * aElement, 
      nsIDOMDocument * aDocument);

    // The KeyDown event is what handles the 
    virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);

    // We don't really care about these events but since we look like a listener we handle 'em
    virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);
    virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

private:
    nsresult DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType);

    nsIDOMElement* element; // Weak reference. The element will go away first.
    nsIDOMDocument* mDOMDocument; // Weak reference.
}; 

////////////////////////////////////////////////////////////////////////

nsDlgDefaultKeysImpl::nsDlgDefaultKeysImpl(void)
{
	NS_INIT_REFCNT();
	
}

nsDlgDefaultKeysImpl::~nsDlgDefaultKeysImpl(void)
{
}

NS_IMPL_ADDREF(nsDlgDefaultKeysImpl)
NS_IMPL_RELEASE(nsDlgDefaultKeysImpl)

NS_IMETHODIMP
nsDlgDefaultKeysImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(NS_GET_IID(nsIDlgDefaultKeys)) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIDlgDefaultKeys*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(NS_GET_IID(nsIDOMKeyListener))) {
        *result = NS_STATIC_CAST(nsIDOMKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMMouseListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsDlgDefaultKeysImpl::Init(
  nsIDOMElement  * aElement,
  nsIDOMDocument * aDocument)
{
  element = aElement; // Weak reference. Don't addref it.
  mDOMDocument = aDocument; // Weak reference.
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMKeyListener

/**
 * Processes a key down event to see if it is a default key for ok
 * or cancel for the dialog
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsDlgDefaultKeysImpl::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return DoKey(aKeyEvent, eKeyDown);
}

////////////////////////////////////////////////////////////////
/**
 * Stub for a key release event
 */
nsresult nsDlgDefaultKeysImpl::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////
/**
 * Stub for a key typed event
 */
nsresult nsDlgDefaultKeysImpl::KeyPress(nsIDOMEvent* aKeyEvent)
{
  return NS_OK;
}

////////////////////////////////////////////////////////////////
/**
 * Here's where the actual work happens
 */
nsresult nsDlgDefaultKeysImpl::DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType)
{
  static PRBool inDlgDefaultKey = PR_FALSE;
  
  if(inDlgDefaultKey)
    return NS_OK;
  else 
    inDlgDefaultKey = PR_TRUE;
  
  if(aKeyEvent && mDOMDocument)
  {
    // Get DOMEvent target
    nsIDOMNode* target = nsnull;
    aKeyEvent->GetTarget(&target);
    
  nsIDOMUIEvent * theEvent;
  aKeyEvent->QueryInterface(kIDomUIEventIID, (void**)&theEvent);
      
  // locate the window element which holds the top level key bindings
  nsCOMPtr<nsIDOMElement> rootElement;
  mDOMDocument->GetDocumentElement(getter_AddRefs(rootElement));
  if (!rootElement) {
    inDlgDefaultKey = PR_FALSE;
    return !NS_OK;
  }
  nsString rootName;
  rootElement->GetNodeName(rootName);

  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNode> keysetNode;
  rootNode->GetFirstChild(getter_AddRefs(keysetNode));
  while (keysetNode)
  {
     nsString keysetNodeType;
     nsCOMPtr<nsIDOMElement> keysetElement(do_QueryInterface(keysetNode));
     if(!keysetElement)
     {
       inDlgDefaultKey = PR_FALSE;
       return rv;
     }
       
     keysetElement->GetNodeName(keysetNodeType);
	 if (keysetNodeType.Equals("keyset"))
	 {
	  // Given the DOM node and Key Event
	  // Walk the node's children looking for 'key' types
	  
	  // If the node isn't tagged disabled
	    // Compares the received key code to found 'key' types
	    // Executes command if found
	    // Marks event as consumed
	    nsCOMPtr<nsIDOMNode> keyNode;
	    keysetNode->GetFirstChild(getter_AddRefs(keyNode));
	    while (keyNode)
	    {
	      nsCOMPtr<nsIDOMElement> keyElement(do_QueryInterface(keyNode));
		  if (keyElement)
		  {
		    nsString keyNodeType;
		    nsString keyName;
		    nsString disabled;
		    nsString cmdToExecute;
		    keyElement->GetNodeName(keyNodeType);

		    if (keyNodeType.Equals("key"))
		    {
		      keyElement->GetAttribute(nsAutoString("key"), keyName);

		      keyElement->GetAttribute(nsAutoString("disabled"),        disabled);
		      if (disabled == "false")
		      {
	            PRUint32 theChar;

				      switch(aEventType)
				      {
				        case eKeyPress:
		              theEvent->GetCharCode(&theChar);
				        break;
				        case eKeyUp:
				        case eKeyDown:
		              theEvent->GetKeyCode(&theChar);
				        break;
				      }
		        
		        char tempChar[2];
		        tempChar[0] = theChar;
		        tempChar[1] = 0;
		        nsString tempChar2 = tempChar;
		        //printf("compare key [%s] \n", tempChar2.ToNewCString()); // this leaks
		         // NOTE - convert theChar and keyName to upper
		         keyName.ToUpperCase();
		         tempChar2.ToUpperCase();
		        if (tempChar2 == keyName)
		        {
			      switch(aEventType)
			      {
			        case eKeyPress:
			          keyElement->GetAttribute(nsAutoString("oncommand"), cmdToExecute);
			        break;
			        case eKeyDown:
			          keyElement->GetAttribute(nsAutoString("onkeydown"), cmdToExecute);
			        break;
			        case eKeyUp:
			          keyElement->GetAttribute(nsAutoString("onkeyup"), cmdToExecute);
			        break;
			      }
			      
			      
			      //printf("onkeypress [%s] \n", cmdToExecute.ToNewCString()); // this leaks
		          do
		          {
		            
		            // Modifier tests passed so execute onclick command
		            

				    // This code executes in every presentation context in which this
				    // document is appearing.
				    nsCOMPtr<nsIContent> content;
				    content = do_QueryInterface(keyElement);
				    if (!content)
				    {
				      inDlgDefaultKey = PR_FALSE;
				      return NS_OK;
				    }
				
				    nsCOMPtr<nsIDocument> document;
				    content->GetDocument(*getter_AddRefs(document));
				
				    if (!document)
				    {
				      inDlgDefaultKey = PR_FALSE;
				      return NS_OK;
				    }
				
				    PRInt32 count = document->GetNumberOfShells();
				    for (PRInt32 i = 0; i < count; i++)
				    {
				        nsIPresShell* shell = document->GetShellAt(i);
				        if (nsnull == shell)
				            continue;
				
				        // Retrieve the context in which our DOM event will fire.
				        nsCOMPtr<nsIPresContext> aPresContext;
				        shell->GetPresContext(getter_AddRefs(aPresContext));
				    
				        NS_RELEASE(shell);
				
				        // Handle the DOM event
				        nsEventStatus status = nsEventStatus_eIgnore;
				        nsKeyEvent event;
				        event.eventStructType = NS_KEY_EVENT;
				        switch (aEventType)
				        {
				          case eKeyPress:  event.message = NS_KEY_PRESS; break;
				          case eKeyDown:   event.message = NS_KEY_DOWN; break;
				          default:         event.message = NS_KEY_UP; break;
				        }
				        aKeyEvent->PreventBubble();
				        aKeyEvent->PreventCapture();
				        content->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

		                if (aEventType == eKeyPress)
		                {
		                  // Also execute the oncommand handler on a key press.
		                  // Execute the oncommand event handler.
		                  nsEventStatus stat = nsEventStatus_eIgnore;
		                  nsMouseEvent evt;
		                  evt.eventStructType = NS_EVENT;
		                  evt.message = NS_MENU_ACTION;
		                  content->HandleDOMEvent(*aPresContext, &evt, nsnull, NS_EVENT_FLAG_INIT, stat);
						  }
		              }
		          } while (PR_FALSE);
		        } // end if (theChar == keyName)
		      } // end if (disabled == "false")
		    } // end if (keyNodeType.Equals("key"))
		  } // end if(keyelement)
		  
		  nsCOMPtr<nsIDOMNode> oldkeyNode(keyNode);  
	      oldkeyNode->GetNextSibling(getter_AddRefs(keyNode));
	      
		} // end while(keynode)
	  } // end  if (keysetNodeType.Equals("keyset")) {
	  
	  nsCOMPtr<nsIDOMNode> oldkeysetNode(keysetNode);  
      oldkeysetNode->GetNextSibling(getter_AddRefs(keysetNode));
      
	} // end while(keysetNode)
  } // end if(aKeyEvent && mDOMDocument) 

  inDlgDefaultKey = PR_FALSE;
  return NS_ERROR_BASE;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewDlgDefaultKeys(nsIDlgDefaultKeys** aHandler)
{
    nsDlgDefaultKeysImpl * handler = new nsDlgDefaultKeysImpl();
    if (!handler)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(handler);
    *aHandler = handler;
    return NS_OK;
}
