/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
#include "nsIDOMElement.h"
#include "nsIXULKeyListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMFocusListener.h"
#include "nsRDFCID.h"

#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindow.h"
#include "nsIScriptContextOwner.h"
#include "nsIDOMXULDocument.h"
#include "nsIDocument.h"
#include "nsIContent.h"
#include "nsIDOMUIEvent.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULKeyListenerCID,      NS_XULKEYLISTENER_CID);
static NS_DEFINE_IID(kIXULKeyListenerIID,     NS_IXULKEYLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomNodeIID,            NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIDomElementIID,         NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);
static NS_DEFINE_IID(kIDomUIEventIID,         NS_IDOMUIEVENT_IID);

////////////////////////////////////////////////////////////////////////
// KeyListenerImpl
//
//   This is the key listener implementation for keybinding
//
class nsXULKeyListenerImpl : public nsIXULKeyListener,
                           public nsIDOMKeyListener
{
public:
    nsXULKeyListenerImpl(void);
    virtual ~nsXULKeyListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULKeyListener
    NS_IMETHOD Init(
      nsIDOMElement  * aElement, 
      nsIDOMDocument * aDocument);

    // nsIDOMKeyListener
    
    /**
     * Processes a key pressed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyDown(nsIDOMEvent* aKeyEvent);

    /**
     * Processes a key release event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     */
    virtual nsresult KeyUp(nsIDOMEvent* aKeyEvent);

    /**
     * Processes a key typed event
     * @param aKeyEvent @see nsIDOMEvent.h 
     * @returns whether the event was consumed or ignored. @see nsresult
     *
     */
    virtual nsresult KeyPress(nsIDOMEvent* aKeyEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

private:
    nsIDOMElement* element; // Weak reference. The element will go away first.
    nsIDOMDocument* mDOMDocument; // Weak reference.
}; 

////////////////////////////////////////////////////////////////////////

nsXULKeyListenerImpl::nsXULKeyListenerImpl(void)
{
	NS_INIT_REFCNT();
	
}

nsXULKeyListenerImpl::~nsXULKeyListenerImpl(void)
{
}

NS_IMPL_ADDREF(nsXULKeyListenerImpl)
NS_IMPL_RELEASE(nsXULKeyListenerImpl)

NS_IMETHODIMP
nsXULKeyListenerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIXULKeyListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIXULKeyListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMKeyListener::GetIID())) {
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
nsXULKeyListenerImpl::Init(
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
 * Processes a key down event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyDown(nsIDOMEvent* aKeyEvent)
{
  nsresult result = NS_OK;
  return result;
}

/**
 * Processes a key release event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyUp(nsIDOMEvent* aKeyEvent)
{
  nsresult result = NS_OK;
  return result;
}

/**
 * Processes a key typed event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 *
 */
 //  // Get the main document 
 //  // find the keyset
 // iterate over key(s) looking for appropriate handler
nsresult nsXULKeyListenerImpl::KeyPress(nsIDOMEvent* aKeyEvent)
{
  nsresult res = NS_OK;
  
  if(aKeyEvent && mDOMDocument) {
    // Get DOMEvent target
    nsIDOMNode* target = nsnull;
    aKeyEvent->GetTarget(&target);
    
  nsIDOMUIEvent * theEvent;
  aKeyEvent->QueryInterface(kIDomUIEventIID, (void**)&theEvent);
    // Find a keyset node
    
    
    
  // locate the window element which holds the top level key bindings
  nsCOMPtr<nsIDOMElement> rootElement;
  mDOMDocument->GetDocumentElement(getter_AddRefs(rootElement));
  if (!rootElement) {
    return !NS_OK;
  }
  nsString rootName;
  rootElement->GetNodeName(rootName);
  //printf("Root Node [%s] \n", rootName.ToNewCString()); // this leaks
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  
  nsresult rv = -1;
  
  nsCOMPtr<nsIDOMNode> keysetNode;
  rootNode->GetFirstChild(getter_AddRefs(keysetNode));
  while (keysetNode) {
     nsString keysetNodeType;
     nsCOMPtr<nsIDOMElement> keysetElement(do_QueryInterface(keysetNode));
     keysetElement->GetNodeName(keysetNodeType);
	 if (keysetNodeType.Equals("keyset")) {
	  // Given the DOM node and Key Event
	  // Walk the node's children looking for 'key' types
	  
	  // If the node isn't tagged disabled
	    // Compares the received key code to found 'key' types
	    // Executes command if found
	    // Marks event as consumed
	    nsCOMPtr<nsIDOMNode> keyNode;
	    keysetNode->GetFirstChild(getter_AddRefs(keyNode));
	    while (keyNode) {
	      nsCOMPtr<nsIDOMElement> keyElement(do_QueryInterface(keyNode));
		  if (keyElement) {
		    nsString keyNodeType;
		    nsString keyName;
		    nsString disabled;
		    nsString modCommand;
		    nsString modShift;
		    nsString modAlt;
		    nsString cmdToExecute;
		    keyElement->GetNodeName(keyNodeType);
		    //printf("keyNodeType [%s] \n", keyNodeType.ToNewCString()); // this leaks
		    if (keyNodeType.Equals("key")) {
		      keyElement->GetAttribute(nsAutoString("key"), keyName);
		      printf("Found key [%s] \n", keyName.ToNewCString()); // this leaks
		      keyElement->GetAttribute(nsAutoString("disabled"),        disabled);
		      if (disabled == "false") {
	            PRUint32 theChar;
		        //theEvent->GetCharCode(&theChar);
		        theEvent->GetKeyCode(&theChar);
		        printf("event key [%c] \n", theChar); // this leaks
		        
		        char tempChar[2];
		        tempChar[0] = theChar;
		        tempChar[1] = 0;
		        nsString tempChar2 = tempChar;
		        printf("compare key [%s] \n", tempChar2.ToNewCString()); // this leaks
		         // NOTE - convert theChar and keyName to upper
		        if (tempChar2 == keyName) {
			      keyElement->GetAttribute(nsAutoString("modifiercommand"), modCommand);
			      keyElement->GetAttribute(nsAutoString("modifiershift"),   modShift);
			      keyElement->GetAttribute(nsAutoString("modifieralt"),     modAlt);
			      keyElement->GetAttribute(nsAutoString("onkeypress"),         cmdToExecute);
		          do {
		            // Test Command attribute
		            /*
		            #ifdef XP_MAC
		              if (theEvent.isCommand && (modCommand != "true"))
		                break;
		            #else
		              if (theEvent.isControl && (modCommand != "true"))
		                break;
		            #endif // XP_MAC
		            */
		            PRBool isControl = PR_FALSE;
		            theEvent->GetCtrlKey(&isControl);
		            //if (isControl && (modCommand != "true"))
		               // break;
		                
		            // Test Shift attribute
		            PRBool isShift = PR_FALSE;
		            theEvent->GetShiftKey(&isShift);
		              if (isShift && (modShift != "true"))
		                break;
		            
		            // Test Alt attribute
		            PRBool isAlt = PR_FALSE;
		            theEvent->GetShiftKey(&isAlt);
		              if (isAlt && (modAlt != "true"))
		                break;
		            
		            // Modifier tests passed so execute onclick command
		            
					  nsresult rv = NS_ERROR_FAILURE;
					 /*
					  nsCOMPtr<nsIContentViewerContainer> contentViewerContainer;
					  contentViewerContainer = do_QueryInterface(theWebShell);
					  if (!contentViewerContainer) {
					      NS_ERROR("Webshell doesn't support the content viewer container interface");
					      return rv;
					  }

					  nsCOMPtr<nsIContentViewer> contentViewer;
					  if (NS_FAILED(rv = contentViewerContainer->GetContentViewer(getter_AddRefs(contentViewer)))) {
					      NS_ERROR("Unable to retrieve content viewer.");
					      return rv;
					  }

					  nsCOMPtr<nsIDocumentViewer> docViewer;
					  docViewer = do_QueryInterface(contentViewer);
					  if (!docViewer) {
					      NS_ERROR("Document viewer interface not supported by the content viewer.");
					      return rv;
					  }
					
					  nsCOMPtr<nsIPresContext> presContext;
					  if (NS_FAILED(rv = docViewer->GetPresContext(*getter_AddRefs(presContext)))) {
					      NS_ERROR("Unable to retrieve the doc viewer's presentation context.");
					      return rv;
					  }
					  				  
					  theStatus = nsEventStatus_eIgnore;
					  nsMouseEvent event;
					  event.eventStructType = NS_MOUSE_EVENT;
					  event.message = NS_MOUSE_LEFT_CLICK;

					  nsCOMPtr<nsIContent> contentNode;
					  contentNode = do_QueryInterface(keyNode);
					  if (!contentNode) {
					      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
					      return rv;
					  }
	                  */
	                  nsCOMPtr<nsIContent> contentNode;
					  contentNode = do_QueryInterface(element);
					 // rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, theStatus);
		          } while (false);
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
  return res;
}

////////////////////////////////////////////////////////////////
nsresult
NS_NewXULKeyListener(nsIXULKeyListener** aListener)
{
    nsXULKeyListenerImpl * listener = new nsXULKeyListenerImpl();
    if (!listener)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(listener);
    *aListener = listener;
    return NS_OK;
}