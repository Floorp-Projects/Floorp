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

/*
  This file provides the implementation for xul popup listener which
  tracks xul popups, context menus, and tooltips
 */

#include "nsCOMPtr.h"
#include "nsIDOMElement.h"
#include "nsIXULPopupListener.h"
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
#include "nsITimer.h"


////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kXULPopupListenerCID,      NS_XULPOPUPLISTENER_CID);
static NS_DEFINE_IID(kIXULPopupListenerIID,     NS_IXULPOPUPLISTENER_IID);
static NS_DEFINE_IID(kISupportsIID,           NS_ISUPPORTS_IID);

static NS_DEFINE_IID(kIDomEventListenerIID,   NS_IDOMEVENTLISTENER_IID);


////////////////////////////////////////////////////////////////////////
// PopupListenerImpl
//
//   This is the popup listener implementation for popup menus, context menus,
//   and tooltips.
//
class XULPopupListenerImpl : public nsIXULPopupListener,
                             public nsIDOMMouseListener,
                             public nsIDOMFocusListener
{
public:
    XULPopupListenerImpl(void);
    virtual ~XULPopupListenerImpl(void);

public:
    // nsISupports
    NS_DECL_ISUPPORTS

    // nsIXULPopupListener
    NS_IMETHOD Init(nsIDOMElement* aElement, const XULPopupType& popupType);

    // nsIDOMMouseListener
    virtual nsresult MouseDown(nsIDOMEvent* aMouseEvent);
    virtual nsresult MouseUp(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseDblClick(nsIDOMEvent* aMouseEvent) { return NS_OK; };
    virtual nsresult MouseOver(nsIDOMEvent* aMouseEvent) ;
    virtual nsresult MouseOut(nsIDOMEvent* aMouseEvent) ;

    // nsIDOMFocusListener
    virtual nsresult Focus(nsIDOMEvent* aEvent) { return NS_OK; };
    virtual nsresult Blur(nsIDOMEvent* aEvent);

    // nsIDOMEventListener
    virtual nsresult HandleEvent(nsIDOMEvent* anEvent) { return NS_OK; };

protected:

    virtual nsresult LaunchPopup(nsIDOMEvent* anEvent);
    virtual nsresult LaunchPopup( nsIDOMElement* aElement, PRInt32 aScreenX, PRInt32 aScreenY, 
                                    PRInt32 aClientX, PRInt32 aClientY ) ;

    nsresult FindDocumentForNode ( nsIDOMNode* inNode, nsIDOMXULDocument** outDoc ) ;

private:
      // |mElement| is the node to which this listener is attached.
    nsIDOMElement* mElement;               // Weak ref. The element will go away first.
    XULPopupType popupType;
    nsCOMPtr<nsIDOMWindow> mPopup;         // The popup. We are responsible for making it go away.
    
    // The following members are not used unless |popupType| is tooltip.
      
      // a timer for determining if a tooltip should be displayed. 
    static void sTooltipCallback ( nsITimer *aTimer, void *aClosure ) ;
    nsCOMPtr<nsITimer> mTooltipTimer;
    PRInt32 mMouseScreenX, mMouseScreenY;       // mouse coordinates for tooltip event
    PRInt32 mMouseClientX, mMouseClientY;
      // The node hovered over that fired the timer. This may turn into the node that
      // triggered the tooltip, but only if the timer ever gets around to firing.
    nsIDOMNode* mPossibleTooltipNode;     // weak ref.
        
};

////////////////////////////////////////////////////////////////////////

XULPopupListenerImpl::XULPopupListenerImpl(void)
  : mElement(nsnull), mPossibleTooltipNode(nsnull), mMouseScreenX(0),
     mMouseScreenY(0), mMouseClientX(0), mMouseClientY(0)
{
	NS_INIT_REFCNT();
	
}

XULPopupListenerImpl::~XULPopupListenerImpl(void)
{
  //XXX do we need to close the popup here? Will we get the right events as
  //XXX the topLevel window is going away when the closebox is pressed?
  if ( mPopup )
    mPopup->Close();

}

NS_IMPL_ADDREF(XULPopupListenerImpl)
NS_IMPL_RELEASE(XULPopupListenerImpl)

NS_IMETHODIMP
XULPopupListenerImpl::QueryInterface(REFNSIID iid, void** result)
{
    if (! result)
        return NS_ERROR_NULL_POINTER;

    *result = nsnull;
    if (iid.Equals(nsIXULPopupListener::GetIID()) ||
        iid.Equals(kISupportsIID)) {
        *result = NS_STATIC_CAST(nsIXULPopupListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMMouseListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMMouseListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    else if (iid.Equals(nsIDOMFocusListener::GetIID())) {
        *result = NS_STATIC_CAST(nsIDOMFocusListener*, this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
      // compiler problems with nsIDOMEventListener::GetIID() cause us to do it this
      // way. Please excuse the lameness (hyatt and pinkerton).
    else if (iid.Equals(kIDomEventListenerIID)) {
        *result = (nsIDOMEventListener*)(nsIDOMMouseListener*)this;
        NS_ADDREF_THIS();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

NS_IMETHODIMP
XULPopupListenerImpl::Init(nsIDOMElement* aElement, const XULPopupType& popup)
{
  mElement = aElement; // Weak reference. Don't addref it.
  popupType = popup;
  return NS_OK;
}

////////////////////////////////////////////////////////////////
// nsIDOMMouseListener

nsresult
XULPopupListenerImpl::MouseDown(nsIDOMEvent* aMouseEvent)
{
  PRUint16 button;

  nsCOMPtr<nsIDOMUIEvent>uiEvent;
  uiEvent = do_QueryInterface(aMouseEvent);
  if (!uiEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  switch (popupType) {
    case eXULPopupType_popup:
      // Check for left mouse button down
      uiEvent->GetButton(&button);
      if (button == 1) {
        // Time to launch a popup menu.
        LaunchPopup(aMouseEvent);
      }
      break;
    case eXULPopupType_context:
#ifdef	XP_MAC
      // XXX: Handle Mac (currently checks if CTRL key is down)
	PRBool	ctrlKey = PR_FALSE;
	uiEvent->GetCtrlKey(&ctrlKey);
	if (ctrlKey == PR_TRUE)
#else
      // Check for right mouse button down
      uiEvent->GetButton(&button);
      if (button == 3)
#endif
      {
        // Time to launch a context menu.
        LaunchPopup(aMouseEvent);
      }
      break;
    
    case eXULPopupType_tooltip:
    case eXULPopupType_blur:
      // ignore
      break;

  }
  return NS_OK;
}


//
// MouseOver
//
// If we're a tooltip, fire off a timer to see if a tooltip should be shown.
//
nsresult
XULPopupListenerImpl::MouseOver(nsIDOMEvent* aMouseEvent)
{
  nsresult rv = NS_OK;
  
  // make sure we're a tooltip. if not, bail.
  if ( popupType != eXULPopupType_tooltip )
    return NS_OK;
  
  //XXX recognize when a popup is already up and immediately show the
  //XXX tooltip for the new item if the dom element is different than
  //XXX the element for which we are currently displaying the tip.
  //XXX
  //XXX for now, just be stupid to get things working.
  if ( mPopup )
    return NS_OK;
  
  // Kill off an old timer and create a new one.
  if ( mTooltipTimer ) {
    mTooltipTimer->Cancel();
    mTooltipTimer = nsnull;
  }
  NS_NewTimer ( getter_AddRefs(mTooltipTimer) );
  if ( mTooltipTimer ) {
    nsCOMPtr<nsIDOMUIEvent> uiEvent ( do_QueryInterface(aMouseEvent) );
    if ( uiEvent ) {
      // stash the coordinates of the event so that we can still get back to it from within the 
      // timer scallback. Also stash the node that started this so we can put it into the
      // document later on (if the timer ever fires).
      nsCOMPtr<nsIDOMNode> eventTarget;
      aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
      mPossibleTooltipNode = eventTarget.get();
      uiEvent->GetScreenX(&mMouseScreenX);
      uiEvent->GetScreenY(&mMouseScreenY);
      uiEvent->GetClientX(&mMouseClientX);
      uiEvent->GetClientY(&mMouseClientY);
      mTooltipTimer->Init(sTooltipCallback, this, 1000);   // one second delay
    }
  }
  else
    NS_WARNING ( "Could not create a timer for tooltip tracking" );
    
  return NS_OK;
  
} // MouseOver


//
// MouseOut
//
// If we're a tooltip, hide any tip that might be showing and remove any
// timer that is pending since the mouse is no longer over this area.
//
nsresult
XULPopupListenerImpl::MouseOut(nsIDOMEvent* aMouseEvent)
{
  // make sure we're a tooltip. if not, bail.
  if ( popupType != eXULPopupType_tooltip )
    return NS_OK;
  
  if ( mTooltipTimer )
    mTooltipTimer->Cancel();
  
  if ( mPopup ) {
    mPopup->Close();  // hide the popup
    mPopup = nsnull;  // release the popup
    
    // clear out the tooltip node on the document
    nsCOMPtr<nsIDOMNode> eventTarget;
    aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
    nsCOMPtr<nsIDOMXULDocument> doc;
    FindDocumentForNode ( eventTarget, getter_AddRefs(doc) );
    if ( doc )
      doc->SetTooltipElement(nsnull);
  }
  
  return NS_OK;
  
} // MouseOut


//
// FindDocumentForNode
//
// Given a DOM content node, finds the XUL document associated with it
//
nsresult
XULPopupListenerImpl :: FindDocumentForNode ( nsIDOMNode* inElement, nsIDOMXULDocument** outDoc )
{
  nsresult rv = NS_OK;
  
  if ( !outDoc || !inElement )
    return NS_ERROR_INVALID_ARG;
  
  // get the document associated with this content element
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(inElement);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  *outDoc = xulDocument;
  NS_ADDREF ( *outDoc );
  
  return rv;

} // FindDocumentForNode


//
// LaunchPopup
//
nsresult
XULPopupListenerImpl::LaunchPopup ( nsIDOMEvent* anEvent )
{
  // Retrieve our x and y position.
  nsCOMPtr<nsIDOMUIEvent> uiEvent ( do_QueryInterface(anEvent) );
  if (!uiEvent) {
    //non-ui event passed in.  bad things.
    return NS_OK;
  }

  PRInt32 xPos, yPos;
  uiEvent->GetScreenX(&xPos); 
  uiEvent->GetScreenY(&yPos); 

  PRInt32 offsetX, offsetY;
  uiEvent->GetClientX(&offsetX);
  uiEvent->GetClientY(&offsetY);

  return LaunchPopup ( mElement, xPos, yPos, offsetX, offsetY );
}


//
// LaunchPopup
//
// Given the element on which the event was triggered and the mouse locations in
// screen and widget coordinates, popup a new window showing the appropriate 
// content.
//
// This looks for an attribute on |aElement| of the appropriate popup type 
// (popup, context, tooltip) and uses that attribute's value as an ID for
// the popup content in the document.
//
nsresult
XULPopupListenerImpl::LaunchPopup( nsIDOMElement* aElement, PRInt32 aScreenX, PRInt32 aScreenY, 
                                      PRInt32 aOffsetX, PRInt32 aOffsetY )
{
#ifdef NS_DEBUG
printf("screen coords %ld %ld\n", aScreenX, aScreenY);
printf("client coords %ld %ld\n", aOffsetX, aOffsetY);
#endif

  nsresult rv = NS_OK;

  nsAutoString type("popup");
  if ( popupType == eXULPopupType_context )
    type = "context";
  else if ( popupType == eXULPopupType_tooltip )
    type = "tooltip";

  nsAutoString identifier;
  mElement->GetAttribute(type, identifier);

  if (identifier == "")
    return rv;

  // Try to find the popup content and the document. We don't use FindDocumentForNode()
  // in this case because we need the nsIDocument interface anyway for the script 
  // context. 
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Turn the document into a XUL document so we can use getElementById
  nsCOMPtr<nsIDOMXULDocument> xulDocument = do_QueryInterface(document);
  if (xulDocument == nsnull) {
    NS_ERROR("Popup attached to an element that isn't in XUL!");
    return NS_ERROR_FAILURE;
  }

  // XXX Handle the _child case for popups and context menus!

  // Use getElementById to obtain the popup content and gracefully fail if 
  // we didn't find any popup content in the document. 
  nsCOMPtr<nsIDOMElement> popupContent;
  if (NS_FAILED(rv = xulDocument->GetElementById(identifier, getter_AddRefs(popupContent)))) {
    NS_ERROR("GetElementById had some kind of spasm.");
    return rv;
  }
  if ( !popupContent )
    return NS_OK;

  // We have some popup content. Obtain our window.
  nsIScriptContextOwner* owner = document->GetScriptContextOwner();
  nsCOMPtr<nsIScriptContext> context;
  if (NS_OK == owner->GetScriptContext(getter_AddRefs(context))) {
    nsIScriptGlobalObject* global = context->GetGlobalObject();
    if (global) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
      if (domWindow != nsnull) {
        // Find out if we're anchored.
        nsAutoString anchorAlignment("none");
        mElement->GetAttribute("popupanchor", anchorAlignment);

        nsAutoString popupAlignment("topleft");
        mElement->GetAttribute("popupalign", popupAlignment);

        // Set the popup in the document for the duration of this call.
        xulDocument->SetPopupElement(mElement);
        
        // If we're anchored, we pass in client/screen offsets so that
        // we can translate the frames corners to screen coords. 
        PRInt32 xPos = aScreenX, yPos = aScreenY;
        if (anchorAlignment != "none") {
          xPos -= aOffsetX;
          yPos -= aOffsetY;
        }
printf("***creating XUL popup at %ld %ld\n", xPos, yPos);
        domWindow->CreatePopup(mElement, popupContent, 
                               xPos, yPos, 
                               type, anchorAlignment, popupAlignment,
                               getter_AddRefs(mPopup));
        if ( popupType == eXULPopupType_popup && mPopup )
          mPopup->Focus();
      
        // XXX For menus only, clear the document.popup field.
      }
      NS_RELEASE(global);
    }
  }
  NS_IF_RELEASE(owner);

  return NS_OK;
}

nsresult
XULPopupListenerImpl::Blur(nsIDOMEvent* aMouseEvent)
{
  nsresult rv = NS_OK;
  
#if 0
// ONLY NEEDED BECAUSE HYATT WAS LAZY
  // Try to find the popup content.
  nsCOMPtr<nsIDocument> document;
  nsCOMPtr<nsIContent> content = do_QueryInterface(mElement);
  if (NS_FAILED(rv = content->GetDocument(*getter_AddRefs(document)))) {
    NS_ERROR("Unable to retrieve the document.");
    return rv;
  }

  // Blur events don't bubble, so this means our window lost focus.
  // Let's check just to make sure.
  nsCOMPtr<nsIDOMNode> eventTarget;
  aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
  
  // We have some popup content. Obtain our window.
  nsIScriptContextOwner* owner = document->GetScriptContextOwner();
  nsCOMPtr<nsIScriptContext> context;
  if (NS_OK == owner->GetScriptContext(getter_AddRefs(context))) {
    nsIScriptGlobalObject* global = context->GetGlobalObject();
    if (global) {
      // Get the DOM window
      nsCOMPtr<nsIDOMWindow> domWindow = do_QueryInterface(global);
  
      // Close, but only if we are the same target.
      nsCOMPtr<nsIDOMNode> windowNode = do_QueryInterface(domWindow);
      if (windowNode.get() == eventTarget.get())
        domWindow->Close();
    }
  }
#endif

  // Blur events don't bubble, so this means our window lost focus.
  // Let's check just to make sure.
  nsCOMPtr<nsIDOMNode> eventTarget;
  aMouseEvent->GetTarget(getter_AddRefs(eventTarget));
  
  // Close, but only if we are the same target.
  nsCOMPtr<nsIDOMNode> windowNode = do_QueryInterface(mPopup);
  if (windowNode.get() == eventTarget.get()) {
    mPopup->Close();
    mPopup = nsnull;
  }
    
  // XXX Figure out how to fire the DESTROY event for the
  // arbitrary XUL case

  // XXX Set document.popup to null in the parent document NOW.
  // xulDocument->SetPopup(nsnull); 
      
  return rv;
}


//
// sTooltipCallback
//
// A timer callback, fired when the mouse has hovered inside of a frame for the 
// appropriate amount of time. Getting to this point means that we should show the
// toolip.
//
// This relies on certain things being cached into the |aClosure| object passed to
// us by the timer:
//   -- the x/y coordinates of the mouse
//   -- the dom node the user hovered over
//
void
XULPopupListenerImpl :: sTooltipCallback (nsITimer *aTimer, void *aClosure)
{
  XULPopupListenerImpl* self = NS_STATIC_CAST(XULPopupListenerImpl*, aClosure);
  if ( self ) {
    // set the node in the document that triggered the tooltip and show it
    nsCOMPtr<nsIDOMXULDocument> doc;
    self->FindDocumentForNode ( self->mPossibleTooltipNode, getter_AddRefs(doc) );
    if ( doc ) {
      nsCOMPtr<nsIDOMElement> element ( do_QueryInterface(self->mPossibleTooltipNode) );
      if ( element ) {
        // check that node is enabled before showing tooltip
        nsAutoString disabledState;
        element->GetAttribute ( "disabled", disabledState );
        if ( disabledState != "true" ) {
          doc->SetTooltipElement ( element );        
          self->LaunchPopup ( element, self->mMouseScreenX, self->mMouseScreenY, 
                               self->mMouseClientX, self->mMouseClientY );
        } // if node enabled
      }
    } // if document
  } // if "self" data valid
  
} // sTimerCallback



////////////////////////////////////////////////////////////////
nsresult
NS_NewXULPopupListener(nsIXULPopupListener** pop)
{
    XULPopupListenerImpl* popup = new XULPopupListenerImpl();
    if (!popup)
      return NS_ERROR_OUT_OF_MEMORY;
    
    NS_ADDREF(popup);
    *pop = popup;
    return NS_OK;
}
