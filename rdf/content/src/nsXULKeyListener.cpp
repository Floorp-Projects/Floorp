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

  enum {
    VK_CANCEL = 3,
    VK_BACK = 8,
    VK_TAB = 9,
    VK_CLEAR = 12,
    VK_RETURN = 13,
    VK_ENTER = 14,
    VK_SHIFT = 16,
    VK_CONTROL = 17,
    VK_ALT = 18,
    VK_PAUSE = 19,
    VK_CAPS_LOCK = 20,
    VK_ESCAPE = 27,
    VK_SPACE = 32,
    VK_PAGE_UP = 33,
    VK_PAGE_DOWN = 34,
    VK_END = 35,
    VK_HOME = 36,
    VK_LEFT = 37,
    VK_UP = 38,
    VK_RIGHT = 39,
    VK_DOWN = 40,
    VK_PRINTSCREEN = 44,
    VK_INSERT = 45,
    VK_DELETE = 46,
    VK_0 = 48,
    VK_1 = 49,
    VK_2 = 50,
    VK_3 = 51,
    VK_4 = 52,
    VK_5 = 53,
    VK_6 = 54,
    VK_7 = 55,
    VK_8 = 56,
    VK_9 = 57,
    VK_SEMICOLON = 59,
    VK_EQUALS = 61,
    VK_A = 65,
    VK_B = 66,
    VK_C = 67,
    VK_D = 68,
    VK_E = 69,
    VK_F = 70,
    VK_G = 71,
    VK_H = 72,
    VK_I = 73,
    VK_J = 74,
    VK_K = 75,
    VK_L = 76,
    VK_M = 77,
    VK_N = 78,
    VK_O = 79,
    VK_P = 80,
    VK_Q = 81,
    VK_R = 82,
    VK_S = 83,
    VK_T = 84,
    VK_U = 85,
    VK_V = 86,
    VK_W = 87,
    VK_X = 88,
    VK_Y = 89,
    VK_Z = 90,
    VK_NUMPAD0 = 96,
    VK_NUMPAD1 = 97,
    VK_NUMPAD2 = 98,
    VK_NUMPAD3 = 99,
    VK_NUMPAD4 = 100,
    VK_NUMPAD5 = 101,
    VK_NUMPAD6 = 102,
    VK_NUMPAD7 = 103,
    VK_NUMPAD8 = 104,
    VK_NUMPAD9 = 105,
    VK_MULTIPLY = 106,
    VK_ADD = 107,
    VK_SEPARATOR = 108,
    VK_SUBTRACT = 109,
    VK_DECIMAL = 110,
    VK_DIVIDE = 111,
    VK_F1 = 112,
    VK_F2 = 113,
    VK_F3 = 114,
    VK_F4 = 115,
    VK_F5 = 116,
    VK_F6 = 117,
    VK_F7 = 118,
    VK_F8 = 119,
    VK_F9 = 120,
    VK_F10 = 121,
    VK_F11 = 122,
    VK_F12 = 123,
    VK_F13 = 124,
    VK_F14 = 125,
    VK_F15 = 126,
    VK_F16 = 127,
    VK_F17 = 128,
    VK_F18 = 129,
    VK_F19 = 130,
    VK_F20 = 131,
    VK_F21 = 132,
    VK_F22 = 133,
    VK_F23 = 134,
    VK_F24 = 135,
    VK_NUM_LOCK = 144,
    VK_SCROLL_LOCK = 145,
    VK_COMMA = 188,
    VK_PERIOD = 190,
    VK_SLASH = 191,
    VK_BACK_QUOTE = 192,
    VK_OPEN_BRACKET = 219,
    VK_BACK_SLASH = 220,
    VK_CLOSE_BRACKET = 221,
    VK_QUOTE = 222
  };

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
    PRBool   IsMatchingKey(PRUint32 theChar, nsString keyName);

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
  static PRBool executingKeyBind = PR_FALSE;
  
  if(executingKeyBind)
    return NS_OK;
  else 
    executingKeyBind = PR_TRUE;
  
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
    executingKeyBind = PR_FALSE;
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
     if(!keysetElement) {
       executingKeyBind = PR_FALSE;
       return rv;
     }
       
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
		        //if (tempChar2 == keyName) {
		        if ( IsMatchingKey(theChar, keyName) ) {
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
		          #ifdef XP_MAC
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
		          #else
                    // Test Command attribute
		            PRBool isCommand = PR_FALSE;
		            PRBool isControl = PR_FALSE;
		            theEvent->GetMetaKey(&isCommand);
		            theEvent->GetCtrlKey(&isControl);
		            if (((isCommand && (modCommand == "false")) ||
		                (!isCommand && (modCommand == "true"))) || 
		                ((isControl && (modControl == "false")) ||
		                (!isControl && (modControl == "true"))))
		              break;
                    //printf("Passed command/ctrl test \n"); // this leaks   
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
				    if (!content) {
				      executingKeyBind = PR_FALSE;
				      return NS_OK;
				    }
				
				    nsCOMPtr<nsIDocument> document;
				    content->GetDocument(*getter_AddRefs(document));
				
				    if (!document) {
				      executingKeyBind = PR_FALSE;
				      return NS_OK;
				    }
				
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
				        aKeyEvent->PreventBubble();
				        aKeyEvent->PreventCapture();
				        content->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);

                if (aEventType == eKeyPress) {
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
  executingKeyBind = PR_FALSE;
  return NS_ERROR_BASE;
}

PRBool nsXULKeyListenerImpl::IsMatchingKey(PRUint32 theChar, nsString keyName)
{
  PRBool ret = PR_FALSE;
  
  //printf("theChar = %d \n", theChar);
  //printf("keyName = %s \n", keyName.ToNewCString());
  //printf("\n");
  
  switch ( theChar ) {
    case VK_CANCEL:
      if(keyName == "VK_CANCEL")
        ret = PR_TRUE;
        break;
    case VK_BACK:
      if(keyName == "VK_BACK")
        ret = PR_TRUE;
        break;
    case VK_TAB:
      if(keyName == "VK_TAB")
        ret = PR_TRUE;
        break;
    case VK_CLEAR:
      if(keyName == "VK_CLEAR")
        ret = PR_TRUE;
        break;
    case VK_RETURN:
      if(keyName == "VK_RETURN")
        ret = PR_TRUE;
        break;
    case VK_ENTER:
      if(keyName == "VK_ENTER")
        ret = PR_TRUE;
        break;
    case VK_SHIFT:
      if(keyName == "VK_SHIFT")
        ret = PR_TRUE;
        break;
    case VK_CONTROL:
      if(keyName == "VK_CONTROL")
        ret = PR_TRUE;
        break;
    case VK_ALT:
      if(keyName == "VK_ALT")
        ret = PR_TRUE;
        break;
    case VK_PAUSE:
      if(keyName == "VK_PAUSE")
        ret = PR_TRUE;
        break;
    case VK_CAPS_LOCK:
      if(keyName == "VK_CAPS_LOCK")
        ret = PR_TRUE;
        break;
    case VK_ESCAPE:
      if(keyName == "VK_ESCAPE")
        ret = PR_TRUE;
        break;
    case VK_SPACE:
      if(keyName == "VK_SPACE")
        ret = PR_TRUE;
        break;
    case VK_PAGE_UP:
      if(keyName == "VK_PAGE_UP")
        ret = PR_TRUE;
        break;
    case VK_PAGE_DOWN:
      if(keyName == "VK_PAGE_DOWN")
        ret = PR_TRUE;
        break;
    case VK_END:
      if(keyName == "VK_END")
        ret = PR_TRUE;
        break;
    case VK_HOME:
      if(keyName == "VK_HOME")
        ret = PR_TRUE;
        break;
    case VK_LEFT:
      if(keyName == "VK_LEFT")
        ret = PR_TRUE;
        break;
    case VK_UP:
      if(keyName == "VK_UP")
        ret = PR_TRUE;
        break;
    case VK_RIGHT:
      if(keyName == "VK_RIGHT")
        ret = PR_TRUE;
        break;
    case VK_DOWN:
      if(keyName == "VK_DOWN")
        ret = PR_TRUE;
        break;
    case VK_PRINTSCREEN:
      if(keyName == "VK_PRINTSCREEN")
        ret = PR_TRUE;
        break;
    case VK_INSERT:
      if(keyName == "VK_INSERT")
        ret = PR_TRUE;
        break;
    case VK_DELETE:
      if(keyName == "VK_DELETE")
        ret = PR_TRUE;
        break;
    case VK_0:
      if(keyName == "VK_0")
        ret = PR_TRUE;
        break;
    case VK_1:
      if(keyName == "VK_1")
        ret = PR_TRUE;
        break;
    case VK_2:
      if(keyName == "VK_2")
        ret = PR_TRUE;
        break;
    case VK_3:
      if(keyName == "VK_3")
        ret = PR_TRUE;
        break;
    case VK_4:
      if(keyName == "VK_4")
        ret = PR_TRUE;
        break;
    case VK_5:
      if(keyName == "VK_5")
        ret = PR_TRUE;
        break;
    case VK_6:
      if(keyName == "VK_6")
        ret = PR_TRUE;
        break;
    case VK_7:
      if(keyName == "VK_7")
        ret = PR_TRUE;
        break;
    case VK_8:
      if(keyName == "VK_8")
        ret = PR_TRUE;
        break;
    case VK_9:
      if(keyName == "VK_9")
        ret = PR_TRUE;
        break;
    case VK_SEMICOLON:
      if(keyName == "VK_SEMICOLON")
        ret = PR_TRUE;
        break;
    case VK_EQUALS:
      if(keyName == "VK_EQUALS")
        ret = PR_TRUE;
        break;
    case VK_A:
      if(keyName == "VK_A"  || keyName == "A" || keyName == "a")
        ret = PR_TRUE;
        break;
    case VK_B:
      if(keyName == "VK_B" || keyName == "B" || keyName == "b")
        ret = PR_TRUE;
    break;
    case VK_C:
      if(keyName == "VK_C"  || keyName == "C" || keyName == "c")
        ret = PR_TRUE;
        break;
    case VK_D:
      if(keyName == "VK_D"  || keyName == "D" || keyName == "d")
        ret = PR_TRUE;
        break;
    case VK_E:
      if(keyName == "VK_E"  || keyName == "E" || keyName == "e")
        ret = PR_TRUE;
        break;
    case VK_F:
      if(keyName == "VK_F"  || keyName == "F" || keyName == "f")
        ret = PR_TRUE;
        break;
    case VK_G:
      if(keyName == "VK_G"  || keyName == "G" || keyName == "g")
        ret = PR_TRUE;
        break;
    case VK_H:
      if(keyName == "VK_H"  || keyName == "H" || keyName == "h")
        ret = PR_TRUE;
        break;
    case VK_I:
      if(keyName == "VK_I"  || keyName == "I" || keyName == "i")
        ret = PR_TRUE;
        break;
    case VK_J:
      if(keyName == "VK_J"  || keyName == "J" || keyName == "j")
        ret = PR_TRUE;
        break;
    case VK_K:
      if(keyName == "VK_K"  || keyName == "K" || keyName == "k")
        ret = PR_TRUE;
        break;
    case VK_L:
      if(keyName == "VK_L"  || keyName == "L" || keyName == "l")
        ret = PR_TRUE;
        break;
    case VK_M:
      if(keyName == "VK_M"  || keyName == "M" || keyName == "m")
        ret = PR_TRUE;
        break;
    case VK_N:
      if(keyName == "VK_N"  || keyName == "N" || keyName == "n")
        ret = PR_TRUE;
        break;
    case VK_O:
      if(keyName == "VK_O"  || keyName == "O" || keyName == "o")
        ret = PR_TRUE;
        break;
    case VK_P:
      if(keyName == "VK_P"  || keyName == "P" || keyName == "p")
        ret = PR_TRUE;
        break;
    case VK_Q:
      if(keyName == "VK_Q"  || keyName == "Q" || keyName == "q")
        ret = PR_TRUE;
        break;
    case VK_R:
      if(keyName == "VK_R"  || keyName == "R" || keyName == "r")
        ret = PR_TRUE;
        break;
    case VK_S:
      if(keyName == "VK_S"  || keyName == "S" || keyName == "s")
        ret = PR_TRUE;
        break;
    case VK_T:
      if(keyName == "VK_T"  || keyName == "T" || keyName == "t")
        ret = PR_TRUE;
        break;
    case VK_U:
      if(keyName == "VK_U"  || keyName == "U" || keyName == "u")
        ret = PR_TRUE;
        break;
    case VK_V:
      if(keyName == "VK_V"  || keyName == "V" || keyName == "v")
        ret = PR_TRUE;
        break;
    case VK_W:
      if(keyName == "VK_W"  || keyName == "W" || keyName == "w")
        ret = PR_TRUE;
        break;
    case VK_X:
      if(keyName == "VK_X"  || keyName == "X" || keyName == "x")
        ret = PR_TRUE;
        break;
    case VK_Y:
      if(keyName == "VK_Y"  || keyName == "Y" || keyName == "y")
        ret = PR_TRUE;
        break;
    case VK_Z:
      if(keyName == "VK_Z"  || keyName == "Z" || keyName == "z")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD0:
      if(keyName == "VK_NUMPAD0")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD1:
      if(keyName == "VK_NUMPAD1")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD2:
      if(keyName == "VK_NUMPAD2")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD3:
      if(keyName == "VK_NUMPAD3")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD4:
      if(keyName == "VK_NUMPAD4")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD5:
      if(keyName == "VK_NUMPAD5")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD6:
      if(keyName == "VK_NUMPAD6")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD7:
      if(keyName == "VK_NUMPAD7")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD8:
      if(keyName == "VK_NUMPAD8")
        ret = PR_TRUE;
        break;
    case VK_NUMPAD9:
      if(keyName == "VK_NUMPAD9")
        ret = PR_TRUE;
        break;
    case VK_MULTIPLY:
      if(keyName == "VK_MULTIPLY")
        ret = PR_TRUE;
        break;
    case VK_ADD:
      if(keyName == "VK_ADD")
        ret = PR_TRUE;
        break;
    case VK_SEPARATOR:
      if(keyName == "VK_SEPARATOR")
        ret = PR_TRUE;
        break;
    case VK_SUBTRACT:
      if(keyName == "VK_SUBTRACT")
        ret = PR_TRUE;
        break;
    case VK_DECIMAL:
      if(keyName == "VK_DECIMAL")
        ret = PR_TRUE;
        break;
    case VK_DIVIDE:
      if(keyName == "VK_DIVIDE")
        ret = PR_TRUE;
        break;
    case VK_F1:
      if(keyName == "VK_F1")
        ret = PR_TRUE;
        break;
    case VK_F2:
      if(keyName == "VK_F2")
        ret = PR_TRUE;
        break;
    case VK_F3:
      if(keyName == "VK_F3")
        ret = PR_TRUE;
        break;
    case VK_F4:
      if(keyName == "VK_F4")
        ret = PR_TRUE;
        break;
    case VK_F5:
      if(keyName == "VK_F5")
        ret = PR_TRUE;
        break;
    case VK_F6:
      if(keyName == "VK_F6")
        ret = PR_TRUE;
        break;
    case VK_F7:
      if(keyName == "VK_F7")
        ret = PR_TRUE;
        break;
    case VK_F8:
      if(keyName == "VK_F8")
        ret = PR_TRUE;
        break;
    case VK_F9:
      if(keyName == "VK_F9")
        ret = PR_TRUE;
        break;
    case VK_F10:
      if(keyName == "VK_F10")
        ret = PR_TRUE;
        break;
    case VK_F11:
      if(keyName == "VK_F11")
        ret = PR_TRUE;
        break;
    case VK_F12:
      if(keyName == "VK_F12")
        ret = PR_TRUE;
        break;
    case VK_F13:
      if(keyName == "VK_F13")
        ret = PR_TRUE;
        break;
    case VK_F14:
      if(keyName == "VK_F14")
        ret = PR_TRUE;
        break;
    case VK_F15:
      if(keyName == "VK_F15")
        ret = PR_TRUE;
        break;
    case VK_F16:
      if(keyName == "VK_F16")
        ret = PR_TRUE;
        break;
    case VK_F17:
      if(keyName == "VK_F17")
        ret = PR_TRUE;
        break;
    case VK_F18:
      if(keyName == "VK_F18")
        ret = PR_TRUE;
        break;
    case VK_F19:
      if(keyName == "VK_F19")
        ret = PR_TRUE;
        break;
    case VK_F20:
      if(keyName == "VK_F20")
        ret = PR_TRUE;
        break;
    case VK_F21:
      if(keyName == "VK_F21")
        ret = PR_TRUE;
        break;
    case VK_F22:
      if(keyName == "VK_F22")
        ret = PR_TRUE;
        break;
    case VK_F23:
      if(keyName == "VK_F23")
        ret = PR_TRUE;
        break;
    case VK_F24:
      if(keyName == "VK_F24")
        ret = PR_TRUE;
        break;
    case VK_NUM_LOCK:
      if(keyName == "VK_NUM_LOCK")
        ret = PR_TRUE;
        break;
    case VK_SCROLL_LOCK:
      if(keyName == "VK_SCROLL_LOCK")
        ret = PR_TRUE;
        break;
    case VK_COMMA:
      if(keyName == "VK_COMMA")
        ret = PR_TRUE;
        break;
    case VK_PERIOD:
      if(keyName == "VK_PERIOD")
        ret = PR_TRUE;
        break;
    case VK_SLASH:
      if(keyName == "VK_SLASH")
        ret = PR_TRUE;
        break;
    case VK_BACK_QUOTE:
      if(keyName == "VK_BACK_QUOTE")
        ret = PR_TRUE;
        break;
    case VK_OPEN_BRACKET:
      if(keyName == "VK_OPEN_BRACKET")
        ret = PR_TRUE;
        break;
    case VK_BACK_SLASH:
      if(keyName == "VK_BACK_SLASH")
        ret = PR_TRUE;
        break;
    case VK_CLOSE_BRACKET:
      if(keyName == "VK_CLOSE_BRACKET")
        ret = PR_TRUE;
        break;
    case VK_QUOTE:
      if(keyName == "VK_QUOTE")
        ret = PR_TRUE;
        break;
  }
  
  return ret;
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
