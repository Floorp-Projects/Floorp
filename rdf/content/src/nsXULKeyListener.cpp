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
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMElement.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMKeyListener.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMUIEvent.h"
#include "nsIDOMWindow.h"
#include "nsIDOMXULDocument.h"
#include "nsINSEvent.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIXULKeyListener.h"
#include "nsRDFCID.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULKeyListenerCID,      NS_XULKEYLISTENER_CID);
static NS_DEFINE_IID(kIXULKeyListenerIID,     NS_IXULKEYLISTENER_IID);
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
    nsresult DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType);

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
    if (iid.Equals(nsIXULKeyListener::GetIID()) ||
        iid.Equals(kISupportsIID)) {
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
  return DoKey(aKeyEvent, eKeyDown);
}

////////////////////////////////////////////////////////////////
/**
 * Processes a key release event
 * @param aKeyEvent @see nsIDOMEvent.h 
 * @returns whether the event was consumed or ignored. @see nsresult
 */
nsresult nsXULKeyListenerImpl::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return DoKey(aKeyEvent, eKeyUp);
}

////////////////////////////////////////////////////////////////
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
  return DoKey(aKeyEvent, eKeyPress);
}

nsresult nsXULKeyListenerImpl::DoKey(nsIDOMEvent* aKeyEvent, eEventType aEventType)
{
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
  
  nsresult rv = NS_ERROR_FAILURE;
  
  nsCOMPtr<nsIDOMNode> keysetNode;
  rootNode->GetFirstChild(getter_AddRefs(keysetNode));
  while (keysetNode) {
     nsString keysetNodeType;
     nsCOMPtr<nsIDOMElement> keysetElement(do_QueryInterface(keysetNode));
     if(!keysetElement)
       return rv;
       
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
		    nsString modControl;
		    nsString modShift;
		    nsString modAlt;
		    nsString cmdToExecute;
		    keyElement->GetNodeName(keyNodeType);
		    //printf("keyNodeType [%s] \n", keyNodeType.ToNewCString()); // this leaks
		    if (keyNodeType.Equals("key")) {
		      keyElement->GetAttribute(nsAutoString("key"), keyName);
		      //printf("Found key [%s] \n", keyName.ToNewCString()); // this leaks
		      keyElement->GetAttribute(nsAutoString("disabled"),        disabled);
		      if (disabled == "false") {
	            PRUint32 theChar;

				      switch(aEventType) {
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
		        if (tempChar2 == keyName) {
			      keyElement->GetAttribute(nsAutoString("command"), modCommand);
			      keyElement->GetAttribute(nsAutoString("control"), modControl);
			      keyElement->GetAttribute(nsAutoString("shift"),   modShift);
			      keyElement->GetAttribute(nsAutoString("alt"),     modAlt);
			      switch(aEventType) {
			        case eKeyPress:
			          keyElement->GetAttribute(nsAutoString("onkeypress"), cmdToExecute);
			        break;
			        case eKeyDown:
			          keyElement->GetAttribute(nsAutoString("onkeydown"), cmdToExecute);
			        break;
			        case eKeyUp:
			          keyElement->GetAttribute(nsAutoString("onkeyup"), cmdToExecute);
			        break;
			      }
			      
			      
			      //printf("onkeypress [%s] \n", cmdToExecute.ToNewCString()); // this leaks
		          do {
		          #ifdef XP_PC
		            // Test Command attribute
		            PRBool isCommand = PR_FALSE;
		            PRBool isControl = PR_FALSE;
		            theEvent->GetMetaKey(&isCommand);
		            theEvent->GetCtrlKey(&isControl);
		            if (((isCommand && (modCommand != "true")) ||
		                (!isCommand && (modCommand == "true"))) && 
		                ((isControl && (modControl != "true")) ||
		                (!isControl && (modControl == "true"))))
		              break;
                    //printf("Passed command/ctrl test \n"); // this leaks
		          #else
		            // Test Command attribute
		            PRBool isCommand = PR_FALSE;
		            theEvent->GetMetaKey(&isCommand);
		            if ((isCommand && (modCommand != "true")) ||
		                (!isCommand && (modCommand == "true")))
		              break;
                    //printf("Passed command test \n"); // this leaks
                     
		            PRBool isControl = PR_FALSE;
		            theEvent->GetCtrlKey(&isControl);
		            if ((isControl && (modControl != "true")) ||
		                (!isControl && (modControl == "true")))
		                break;
		            //printf("Passed control test \n"); // this leaks    
		          #endif
		          
		            // Test Shift attribute
		            PRBool isShift = PR_FALSE;
		            theEvent->GetShiftKey(&isShift);
		            if ((isShift && (modShift != "true")) ||
		                (!isShift && (modShift == "true")))
		                break;
		            
		            // Test Alt attribute
		            PRBool isAlt = PR_FALSE;
		            theEvent->GetAltKey(&isAlt);
		              if ((isAlt && (modAlt != "true")) ||
		                  (!isAlt && (modAlt == "true")))
		                break;
		            
		            // Modifier tests passed so execute onclick command
		            

				    // This code executes in every presentation context in which this
				    // document is appearing.
				    nsCOMPtr<nsIContent> content;
				    content = do_QueryInterface(keyElement);
				    if (!content)
				      return NS_OK;
				
				    nsCOMPtr<nsIDocument> document;
				    content->GetDocument(*getter_AddRefs(document));
				
				    if (!document)
				      return NS_OK;
				
				    PRInt32 count = document->GetNumberOfShells();
				    for (PRInt32 i = 0; i < count; i++) {
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
				        content->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
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
  return NS_ERROR_BASE;
  
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
