/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsKeyBindMgr.h"
#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsDOMEvent.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIContent.h"

NS_IMPL_ADDREF(nsKeyBindMgr)
NS_IMPL_RELEASE(nsKeyBindMgr)

// XXX BWEEP BWEEP This is ONLY TEMPORARY until the service manager
// has a way of registering instances
// (see http://bugzilla.mozilla.org/show_bug.cgi?id=3509 ).
nsIKeyBindMgr* theKeyBindMgr = 0;

extern "C" NS_EXPORT nsIKeyBindMgr* GetKeyBindMgr();
extern "C" NS_EXPORT nsIKeyBindMgr* GetKeyBindMgr()
{
  return theKeyBindMgr;
}

nsresult NS_NewKeyBindMgr(nsIKeyBindMgr** aInstancePtrResult);
nsresult NS_NewKeyBindMgr(nsIKeyBindMgr** aInstancePtrResult)
{
  nsKeyBindMgr* sm = new nsKeyBindMgr();
  return sm->QueryInterface(nsIKeyBindMgr::GetIID(),
                            (void**) aInstancePtrResult);
}

nsKeyBindMgr::nsKeyBindMgr()
{
  NS_INIT_REFCNT();

  theKeyBindMgr = this;
}

nsKeyBindMgr::~nsKeyBindMgr()
{
}

nsresult nsKeyBindMgr::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(aInstancePtrResult, "null pointer");
  if (!aInstancePtrResult) 
  {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(nsISupports::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsISupports*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIKeyBindMgr::GetIID())) 
  {
    *aInstancePtrResult = (void*)(nsIKeyBindMgr*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return !NS_OK;
}

nsresult nsKeyBindMgr::ProcessKeyEvent(
  nsIDOMDocument   * domDoc, 
  const nsKeyEvent & theEvent, 
  nsIWebShell      * theWebShell, 
  nsEventStatus    & theStatus)
{
  // locate the window element which holds the top level key bindings
  nsCOMPtr<nsIDOMElement> rootElement;
  domDoc->GetDocumentElement(getter_AddRefs(rootElement));
  if (!rootElement) {
    return !NS_OK;
  }
  nsString rootName;
  rootElement->GetNodeName(rootName);
   printf("Root Node [%s] \n", rootName.ToNewCString()); // this leaks
  nsCOMPtr<nsIDOMNode> rootNode(do_QueryInterface(rootElement));
  
  nsresult rv = -1;
  // Given the DOM node and Key Event
  // Walk the node's children looking for 'key' types
  
  // If the node isn't tagged disabled
    // Compares the received key code to found 'key' types
    // Executes command if found
    // Marks event as consumed
    nsCOMPtr<nsIDOMNode> keyNode;
    rootNode->GetFirstChild(getter_AddRefs(keyNode));
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
	    printf("keyNodeType [%s] \n", keyNodeType.ToNewCString()); // this leaks
	    if (keyNodeType.Equals("key")) {
	      keyElement->GetAttribute(nsAutoString("key"), keyName);
	      printf("Found key [%s] \n", keyName.ToNewCString()); // this leaks
	      keyElement->GetAttribute(nsAutoString("disabled"),        disabled);
	      if (disabled == "false") {
	        char tempChar[1];
	        tempChar[0] = theEvent.charCode;
	        nsString theChar = tempChar;
	         // NOTE - convert theChar and keyName to upper
	        if (theChar == keyName) {
		      keyElement->GetAttribute(nsAutoString("modifiercommand"), modCommand);
		      keyElement->GetAttribute(nsAutoString("modifiershift"),   modShift);
		      keyElement->GetAttribute(nsAutoString("modifieralt"),     modAlt);
		      keyElement->GetAttribute(nsAutoString("onkeypress"),         cmdToExecute);
	          do {
	            // Test Command attribute
	            #ifdef XP_MAC
	              if (theEvent.isCommand && (modCommand != "true"))
	                break;
	            #else
	              if (theEvent.isControl && (modCommand != "true"))
	                break;
	            #endif // XP_MAC
	            
	            // Test Shift attribute
	              if (theEvent.isShift && (modShift != "true"))
	                break;
	            
	            // Test Alt attribute
	              if (theEvent.isAlt && (modAlt != "true"))
	                break;
	            
	            // Modifier tests passed so execute onclick command
	            
				  nsresult rv = NS_ERROR_FAILURE;
				 
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

				  rv = contentNode->HandleDOMEvent(*presContext, &event, nsnull, NS_EVENT_FLAG_INIT, theStatus);
	          } while (false);
	        } // end if (theChar == keyName)
	      } // end if (disabled == "false")
	    } // end if (keyNodeType.Equals("key"))
	  } // end if(keyelement)
	  nsCOMPtr<nsIDOMNode> oldkeyNode(keyNode);  
      oldkeyNode->GetNextSibling(getter_AddRefs(keyNode));
	} // end while(keynode)
  
  return rv;
}

