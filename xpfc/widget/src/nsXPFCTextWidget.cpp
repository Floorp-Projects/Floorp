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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "nsXPFCTextWidget.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsITextWidget.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"
#include "nsXPFCActionCommand.h"
#include "nsRepository.h"
#include "nsxpfcCIID.h"
#include "nsViewsCID.h"
#include "nsIViewManager.h"
#include "nsXPFCToolkit.h"

static NS_DEFINE_IID(kISupportsIID,         NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCTextWidgetCID,   NS_XPFC_TEXTWIDGET_CID);
static NS_DEFINE_IID(kCIXPFCTextWidgetIID,  NS_IXPFC_TEXTWIDGET_IID);
static NS_DEFINE_IID(kCTextWidgetCID,       NS_TEXTFIELD_CID);
static NS_DEFINE_IID(kInsTextWidgetIID,     NS_ITEXTWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID,           NS_IWIDGET_IID);
static NS_DEFINE_IID(kViewCID,                    NS_VIEW_CID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

#define MAX_SIZE 2048

nsXPFCTextWidget :: nsXPFCTextWidget(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCTextWidget :: ~nsXPFCTextWidget()
{
}

nsresult nsXPFCTextWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCTextWidgetCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCTextWidget *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCTextWidgetIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCTextWidget *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kISupportsIID)) {                                      
    *aInstancePtr = (void*) (this);                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  return (nsXPFCCanvas::QueryInterface(aIID, aInstancePtr));
}

NS_IMPL_ADDREF(nsXPFCTextWidget)
NS_IMPL_RELEASE(nsXPFCTextWidget)

nsresult nsXPFCTextWidget :: Init()
{
  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPFCTextWidget :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCTextWidget :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}


nsresult nsXPFCTextWidget :: CreateView()
{
  
  nsresult res = NS_OK;

  nsIView * parent = GetView();

  LoadView(kViewCID, &kCTextWidgetCID, parent);

  nsIWidget * tw = nsnull;
  nsITextWidget * text_widget = nsnull;

  mView->GetWidget(tw);

  res = tw->QueryInterface(kInsTextWidgetIID,(void**)&text_widget);

  if (NS_OK == res)
  {
    nsSize size ;
  
    GetClassPreferredSize(size);

    nsRect rect(0,0,size.width,size.height);

    tw->SetBackgroundColor(GetBackgroundColor());
    tw->SetForegroundColor(GetForegroundColor());

    PRUint32 num;

    text_widget->SetText(GetLabel(),num);

    if (gXPFCToolkit->GetCanvasManager()->GetFocusedCanvas() == this)
      SetFocus();

    gXPFCToolkit->GetViewManager()->MoveViewTo(mView, rect.x, rect.y);
    gXPFCToolkit->GetViewManager()->ResizeView(mView, rect.width, rect.height);
    gXPFCToolkit->GetViewManager()->UpdateView(mView, rect, NS_VMREFRESH_AUTO_DOUBLE_BUFFER | NS_VMREFRESH_NO_SYNC) ;

    NS_RELEASE(text_widget);
  }
  
  NS_RELEASE(tw);
  SetVisibility(PR_FALSE);      

  return res;
}

nsresult nsXPFCTextWidget :: SetLabel(nsString& aString)
{
  nsXPFCCanvas::SetLabel(aString);
  return NS_OK;
}

nsEventStatus nsXPFCTextWidget::OnKeyDown(nsGUIEvent *aEvent)
{  

  nsresult res = NS_OK;

  if (((nsKeyEvent*)aEvent)->keyCode == NS_VK_RETURN)
  {
    /*
     * We hit the enter key in the edit field.
     *
     * Pass this command up the chain....
     */

    nsEventStatus status = nsEventStatus_eIgnore;
    nsString command = GetCommand();
    PRUint32 offset = command.Find("$label");

    // If this command contains a $Label, replace it with
    // our text

    if (offset != -1)
    {

      nsITextWidget * text_widget = nsnull;

      nsIWidget * widget = nsnull;
      
      GetView()->GetWidget(widget);

      res = widget->QueryInterface(kInsTextWidgetIID,(void**)&text_widget);

      NS_RELEASE(widget);

      if (NS_OK == res)
      {
        nsString text ;
        PRUint32 size;
                
        text_widget->GetText(text, MAX_SIZE, size);

        command.Cut(offset,6);
        command.Insert(text,offset,text.Length());

        NS_RELEASE(text_widget);

      }
    }

    // Create an Action Command and pass it on...

    nsXPFCActionCommand * action;

    static NS_DEFINE_IID(kCXPFCActionCommandCID, NS_XPFC_ACTION_COMMAND_CID);
    static NS_DEFINE_IID(kXPFCCommandIID, NS_IXPFC_COMMAND_IID);

    res = nsRepository::CreateInstance(kCXPFCActionCommandCID, 
                                       nsnull, 
                                       kXPFCCommandIID, 
                                       (void **)&action);

    if (NS_OK != res)
      return status ;

    action->Init();

    action->mAction = command;

    status = ProcessCommand(action);

    NS_RELEASE(action);

    return (status);

  }

  return (nsXPFCCanvas::OnKeyDown(aEvent));
}