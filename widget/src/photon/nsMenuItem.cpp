/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <Pt.h>

#include "nsMenuItem.h"
#include "nsIMenu.h"
#include "nsIMenuBar.h"
#include "nsIWidget.h"

#include "nsStringUtil.h"

#include "nsIPopUpMenu.h"

#include "nsCOMPtr.h"
#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIWebShell.h"

#include "nsPhWidgetLog.h"

static NS_DEFINE_IID(kIMenuIID,     NS_IMENU_IID);
static NS_DEFINE_IID(kIMenuBarIID,  NS_IMENUBAR_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPopUpMenuIID, NS_IPOPUPMENU_IID);
static NS_DEFINE_IID(kIMenuItemIID, NS_IMENUITEM_IID);

nsresult nsMenuItem::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;

  if (aIID.Equals(kIMenuItemIID))
  {
    *aInstancePtr = (void*)(nsIMenuItem*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtr = (void*)(nsISupports*)(nsIMenuItem*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(kIMenuListenerIID))
  {
    *aInstancePtr = (void*)(nsIMenuListener*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsMenuItem)
NS_IMPL_RELEASE(nsMenuItem)

//-------------------------------------------------------------------------
//
// nsMenuItem constructor
//
//-------------------------------------------------------------------------
nsMenuItem::nsMenuItem() : nsIMenuItem()
{
  NS_INIT_REFCNT();
  mMenuItem    = nsnull;
  mMenuParent  = nsnull;
  mPopUpParent = nsnull;
  mTarget      = nsnull;
  mListener = nsnull;
  mIsSeparator = PR_FALSE;
  mIsSubMenu   = PR_FALSE;
  mWebShell    = nsnull;
  mDOMElement  = nsnull;
}

//-------------------------------------------------------------------------
//
// nsMenuItem destructor
//
//-------------------------------------------------------------------------
nsMenuItem::~nsMenuItem()
{
  char *str=mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuItem::~nsMenuItem Destructor called for <%s>\n", str));
  delete [] str;
   
  NS_IF_RELEASE(mListener);

  /* Destroy my Photon Objects */
  if (mMenuItem)
    PtDestroyWidget (mMenuItem);
}

//-------------------------------------------------------------------------
void nsMenuItem::Create(nsIWidget      *aMBParent, 
                        PtWidget_t     *aParent, 
                        const nsString &aLabel, 
                        PRBool          aIsSeparator)
{
  PtArg_t  arg[5];
  void     *me        = (void *) this;

  mTarget  = aMBParent;
  mLabel   = aLabel;
  
  if (NULL == aParent || nsnull == aMBParent)
  {
    return;
  }

  char * nameStr = mLabel.ToNewCString();

PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuItem::nsMenuItem Create with MenuBar parent this=<%p> aMBParent=<%p> aParent=<%p> label=<%s>\n", this, aMBParent,aParent, nameStr));

  PtSetArg ( &arg[0], Pt_ARG_BUTTON_TYPE, Pt_MENU_TEXT, 0);
  PtSetArg ( &arg[1], Pt_ARG_USER_DATA, &me, sizeof(void *) );

  if (aIsSeparator)
  {
    mIsSeparator = PR_TRUE;
    PtSetArg ( &arg[2], Pt_ARG_SEP_TYPE, Pt_ETCHED_IN, 0);
    mMenuItem = PtCreateWidget (PtSeparator, aParent, 3, arg);
   }
  else
  {
    PtCallback_t callbacks[] = {{ MenuItemActivateCb, this }};

    mIsSeparator = PR_FALSE;
    PtSetArg ( &arg[2], Pt_ARG_TEXT_STRING, nameStr, 0);
    PtSetArg ( &arg[3], Pt_CB_ACTIVATE, callbacks, Pt_LINK_INSERT);
    mMenuItem = PtCreateWidget (PtMenuButton, aParent, 4, arg);
  }
  

  NS_ASSERTION(mMenuItem, "Null Photon PtMenuItem");
  
//PtAddCallback(mMenuItem, Pt_CB_ACTIVATE, MenuItemActivateCb ,this);

  PtAddCallback(mMenuItem, Pt_CB_GOT_FOCUS,  MenuItemArmCb ,this);
  PtAddCallback(mMenuItem, Pt_CB_LOST_FOCUS, MenuItemDisarmCb ,this);

  delete[] nameStr;
}

//-------------------------------------------------------------------------
/* Create a Menu Separator */
NS_METHOD nsMenuItem::Create(nsIMenu * aParent)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::Create Separator with nsIMenu\n"));

  mMenuParent = aParent;
  mIsSeparator = PR_TRUE;

  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu * aParent)
{

  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::Create with nsIPopUpMenu %p - Not Implemented\n", aParent));
  mIsSeparator = PR_TRUE;
  return NS_OK;
}


//-------------------------------------------------------------------------
PtWidget_t *nsMenuItem::GetNativeParent()
{
  void * voidData;

  if (nsnull != mMenuParent)
  {
      mMenuParent->GetNativeData(&voidData);
  }
  else if (nsnull != mPopUpParent)
  {
      mPopUpParent->GetNativeData(voidData);
  }
  else
  {
      return nsnull;
  }

  return (PtWidget_t *) voidData;
}


//-------------------------------------------------------------------------
nsIWidget * nsMenuItem::GetMenuBarParent(nsISupports * aParent)
{
  nsIWidget    * widget  = nsnull; // MenuBar's Parent
  nsIMenu      * menu    = nsnull;
  nsIMenuBar   * menuBar = nsnull;
  nsIPopUpMenu * popup   = nsnull;
  nsISupports  * parent  = aParent;

  // Bump the ref count on the parent, since it gets released unconditionally..
  NS_ADDREF(parent);
  while (1)
  {
    if (NS_OK == parent->QueryInterface(kIMenuIID,(void**)&menu))
	{
      NS_RELEASE(parent);
      if (NS_OK != menu->GetParent(parent))
	  {
        NS_RELEASE(menu);
        return nsnull;
      }
  
      NS_RELEASE(menu);

    }
	else if (NS_OK == parent->QueryInterface(kIPopUpMenuIID,(void**)&popup))
	{
      if (NS_OK != popup->GetParent(widget))
	  {
        widget =  nsnull;
      } 

      NS_RELEASE(popup);
      NS_RELEASE(parent);
      return widget;

    } else if (NS_OK == parent->QueryInterface(kIMenuBarIID,(void**)&menuBar)) {
      if (NS_OK != menuBar->GetParent(widget)) {
        widget =  nsnull;
      } 
      NS_RELEASE(parent);
      NS_RELEASE(menuBar);
      return widget;
    } else {
      NS_RELEASE(parent);
      return nsnull;
    }
  }
  return nsnull;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsISupports     *aParent, 
                             const nsString  &aLabel,  
                             PRBool           aIsSeparator)
{
  char *str=aLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::Create with nsIMenu this=<%p>, aParent=<%p> aLabel=<%s> aIsSep=<%d>\n",this, aParent, str, aIsSeparator));
  delete [] str;

  if (nsnull == aParent)
  {
    return NS_ERROR_FAILURE;
  }
		
  mMenuParent = (nsIMenu *) aParent;	/* HACK */

  nsIWidget   * widget  = nsnull; // MenuBar's Parent
  nsISupports * sups;
  if (NS_OK == aParent->QueryInterface(kISupportsIID,(void**)&sups))
  {
    widget = GetMenuBarParent(sups);
    NS_RELEASE(sups); // Balance the Query Interface
  }
							  
  Create(widget, GetNativeParent(), aLabel, aIsSeparator);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::Create(nsIPopUpMenu   *aParent, 
                             const nsString &aLabel,  
                             PRUint32        aCommand)
{

  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::Create with nsIPopUpMenu - Not Implemented\n"));

  mPopUpParent = aParent;
  mCommand = aCommand;
  mLabel = aLabel;

  nsIWidget * widget = nsnull;
  if (NS_OK != aParent->GetParent(widget))
  {
    widget = nsnull;
  }

  Create(widget, GetNativeParent(), aLabel, aCommand);
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetLabel(nsString &aText)
{
  aText = mLabel;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetLabel(nsString &aText)
{
  mLabel = aText;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetEnabled(PRBool aIsEnabled)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuItem::SetEnabled - Not Implmented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetEnabled(PRBool *aIsEnabled)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuItem::GetEnabled - Not Implmented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetChecked(PRBool aIsEnabled)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuItem::SetChecked - Not Implmented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetChecked(PRBool *aIsEnabled)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR, ("nsMenuItem::GetChecked - Not Implmented\n"));
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetCommand(PRUint32 & aCommand)
{
  aCommand = mCommand;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetTarget(nsIWidget *& aTarget)
{
  aTarget = mTarget;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetNativeData(void *& aData)
{
  aData = (void *)mMenuItem;
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::AddMenuListener(nsIMenuListener * aMenuListener)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::AddMenuListener new mListener=<%p>\n", aMenuListener));

  NS_IF_RELEASE(mListener);
  mListener = aMenuListener;
  NS_ADDREF(mListener);
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::RemoveMenuListener(nsIMenuListener * aMenuListener)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::RemoveMenuListener\n"));

  if (mListener == aMenuListener)
  {
    NS_IF_RELEASE(mListener);
  }
  
  return NS_OK;
}

//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::IsSeparator(PRBool & aIsSep)
{
  aIsSep = mIsSeparator;
  return NS_OK;
}

//-------------------------------------------------------------------------
// nsIMenuListener interface
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuItemSelected(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::MenuItemSelected\n"));

  if(!mIsSeparator)
  {
    DoCommand();
  }
  
  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuSelected(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::MenuSelected - Not Implemented\n"));

  if(mListener)
   return mListener->MenuSelected(aMenuEvent);

  return nsEventStatus_eIgnore;
}

nsEventStatus nsMenuItem::MenuDeselected(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::MenuDeSelected - Not Implemented\n"));

  if(mListener)
    return mListener->MenuDeselected(aMenuEvent);

  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuConstruct(
    const nsMenuEvent & aMenuEvent,
    nsIWidget         * aParentWindow, 
    void              * menuNode,
    void              * aWebShell)
{
 PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::MenuConstruct - Not Implemented\n"));
 if (mListener)
 {
   mListener->MenuSelected(aMenuEvent);
 }

  return nsEventStatus_eIgnore;
}
//-------------------------------------------------------------------------
nsEventStatus nsMenuItem::MenuDestruct(const nsMenuEvent & aMenuEvent)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::MenuDestruct - Not Implemented\n"));
  if (mListener)
  {
    mListener->MenuDeselected(aMenuEvent);
  }

  return nsEventStatus_eIgnore;
}

//-------------------------------------------------------------------------
/**
* Sets the JavaScript Command to be invoked when a "gui" event occurs on a source widget
* @param aStrCmd the JS command to be cached for later execution
* @return NS_OK 
*/
NS_METHOD nsMenuItem::SetCommand(const nsString & aStrCmd)
{
  char *str = aStrCmd.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetCommand  mCommandStr=<%s>\n", str));
  delete [] str;
  
  mCommandStr = aStrCmd;
  return NS_OK;
}

//-------------------------------------------------------------------------
/**
* Executes the "cached" JavaScript Command 
* @return NS_OK if the command was executed properly, otherwise an error code
*/
NS_METHOD nsMenuItem::DoCommand()
{
  nsresult rv = NS_ERROR_FAILURE;

  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::DoCommand\n"));

  if(!mWebShell || !mDOMElement)
    return rv;
    
  nsCOMPtr<nsIContentViewer> contentViewer;
  NS_ENSURE_SUCCESS(mWebShell->GetContentViewer(getter_AddRefs(contentViewer)),
   NS_ERROR_FAILURE);

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

  nsEventStatus status = nsEventStatus_eIgnore;
  nsMouseEvent event;
  event.eventStructType = NS_MOUSE_EVENT;
  event.message = NS_XUL_COMMAND;

  nsCOMPtr<nsIContent> contentNode;
  contentNode = do_QueryInterface(mDOMElement);
  if (!contentNode) {
      NS_ERROR("DOM Node doesn't support the nsIContent interface required to handle DOM events.");
      return rv;
  }

  rv = contentNode->HandleDOMEvent(presContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
  return rv;
}
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetDOMNode(nsIDOMNode * aDOMNode)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetDOMNode\n"));
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMNode(nsIDOMNode ** aDOMNode)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::GetDOMNode\n"));
  return NS_OK;
}
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetDOMElement(nsIDOMElement * aDOMElement)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetDOMElement\n"));
  mDOMElement = aDOMElement;
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::GetDOMElement(nsIDOMElement ** aDOMElement)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::GetDOMElement\n"));
  *aDOMElement = mDOMElement;
  return NS_OK;
}
    
//-------------------------------------------------------------------------
NS_METHOD nsMenuItem::SetWebShell(nsIWebShell * aWebShell)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetWebShell\n"));
  mWebShell = aWebShell;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetShortcutChar(const nsString &aText)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetShortcut - Not Implemented\n"));

  mKeyEquivalent = aText;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::GetShortcutChar(nsString &aText)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::GetShortcut - Not Implemented\n"));

  aText = mKeyEquivalent;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::SetModifiers(PRUint8 aModifiers)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::SetModifiers - Not Implemented\n"));

  mModifiers = aModifiers;
  return NS_OK;
}

//----------------------------------------------------------------------
NS_IMETHODIMP nsMenuItem::GetModifiers(PRUint8 * aModifiers)
{
  PR_LOG(PhWidLog, PR_LOG_ERROR,("nsMenuItem::GetModifiers - Not Implemented\n"));

  *aModifiers = mModifiers; 
  return NS_OK;
}




//-------------------------------------------------------------------------
//-------------------------------------------------------------------------
//------- Native Photon  Routines needed for nsMenuItem
//-------------------------------------------------------------------------
//-------------------------------------------------------------------------

int nsMenuItem::MenuItemActivateCb (PtWidget_t *widget, void *nsClassPtr, PtCallbackInfo_t *cbinfo)
{
  nsMenuEvent    event;
  nsEventStatus  status;
  nsMenuItem    *aMenuItem = (nsMenuItem *) nsClassPtr;
  
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuItem::MenuItemActivate Callback aMenuItem=<%p>\n", aMenuItem));

  
  if (aMenuItem != nsnull)
  {
//    NS_ADDREF(aMenuItem);					/* HACK, maybe this will work! */

    /* Fill out the event structure */
    event.message = NS_MENU_SELECTED;
    event.eventStructType = NS_MENU_EVENT;
    event.point = nsPoint (0,0);
    event.time = PR_IntervalNow();
    event.widget = nsnull;
    event.nativeMsg = NULL;

    aMenuItem->GetCommand(aMenuItem->mCommand);
    event.mMenuItem = aMenuItem;

    nsIMenuListener *menuListener = nsnull;
    aMenuItem->QueryInterface(kIMenuListenerIID, (void**)&menuListener);
	if (menuListener)
	{
	  menuListener->MenuItemSelected(event);
      NS_IF_RELEASE(menuListener);
    }
  }
  
  return Pt_CONTINUE;
}

int nsMenuItem::MenuItemArmCb (PtWidget_t *widget, void *nsClassPtr, PtCallbackInfo_t *cbinfo)
{
  nsMenuEvent event;
  nsEventStatus status;
  nsMenuItem *aMenuItem = (nsMenuItem *) nsClassPtr;

  char *str = aMenuItem->mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuItem::MenuItemArmCB Callback for <%s>\n", str));
  delete [] str;

  return Pt_CONTINUE;
}

int nsMenuItem::MenuItemDisarmCb (PtWidget_t *widget, void *nsClassPtr, PtCallbackInfo_t *cbinfo)
{
  nsMenuEvent event;
  nsEventStatus status;
  nsMenuItem *aMenuItem = (nsMenuItem *) nsClassPtr;

  char *str = aMenuItem->mLabel.ToNewCString();
  PR_LOG(PhWidLog, PR_LOG_DEBUG, ("nsMenuItem::MenuItemDisarmCB Callback for <%s>\n", str));
  delete [] str;

  return Pt_CONTINUE;
}
