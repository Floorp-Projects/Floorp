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

#include "nsXPFCButton.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsIButton.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"

#include "nsIAppShell.h"
#include "nsIWidget.h"
#include "nsITextWidget.h"
#include "nsIButton.h"
#include "nsIImageGroup.h"
#include "nsITimer.h"
#include "nsIThrobber.h"

static NS_DEFINE_IID(kISupportsIID,   NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCButtonCID, NS_XPFC_BUTTON_CID);
static NS_DEFINE_IID(kCIXPFCButtonIID,NS_IXPFC_BUTTON_IID);
static NS_DEFINE_IID(kCButtonCID,     NS_BUTTON_CID);
static NS_DEFINE_IID(kInsButtonIID,   NS_IBUTTON_IID);
static NS_DEFINE_IID(kIWidgetIID,     NS_IWIDGET_IID);
static NS_DEFINE_IID(kThrobberCID,    NS_THROBBER_CID);
static NS_DEFINE_IID(kIThrobberIID,   NS_ITHROBBER_IID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

static nsIThrobber * mThrobber = nsnull;

nsXPFCButton :: nsXPFCButton(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCButton :: ~nsXPFCButton()
{
}

nsresult nsXPFCButton::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCButtonCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCButton *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCButtonIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCButton *)this;                                        
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

NS_IMPL_ADDREF(nsXPFCButton)
NS_IMPL_RELEASE(nsXPFCButton)

nsresult nsXPFCButton :: Init()
{
  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPFCButton :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCButton :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

nsresult nsXPFCButton :: CreateWidget()
{
  nsresult res = NS_OK;

  if (GetLabel() == "Throbber")
  {
    res = nsRepository::CreateInstance(kThrobberCID, 
                                       nsnull, 
                                       kIThrobberIID,
                                       (void**)&mThrobber);
    if (NS_OK != res)
      return res;

    nsIWidget * parent = GetWidget();

    nsSize size ;

    GetClassPreferredSize(size);

    nsRect rect(0,0,size.width,size.height);

    mThrobber->Init(parent, rect);
    
    mThrobber->Show();

  } else {


    nsIWidget * parent = GetWidget();

    res = LoadWidget(kCButtonCID);

    nsIButton * button = nsnull;
    res = QueryInterface(kInsButtonIID,(void**)&button);

    if (NS_OK == res)
    {

      nsIWidget * bw = nsnull;

      res = button->QueryInterface(kIWidgetIID,(void**)&bw);

      if (NS_OK == res)
      {

        nsSize size ;
      
        GetClassPreferredSize(size);

        nsRect rect(0,0,size.width,size.height);

        bw->Create(parent, 
                   rect, 
                   gXPFCToolkit->GetShellEventCallback(), 
                   nsnull);

        bw->SetBackgroundColor(GetBackgroundColor());
        bw->SetForegroundColor(GetForegroundColor());
        bw->Show(PR_TRUE);

        button->SetLabel(GetLabel());

        NS_RELEASE(bw);

      }

      NS_RELEASE(button);
    }
  
    SetVisibility(PR_FALSE);      
  }
  return res;
}

nsresult nsXPFCButton :: SetLabel(nsString& aString)
{
  nsXPFCCanvas::SetLabel(aString);

  /*
   * If we are aggregating a button, set it specifically
   *
   * At this point we probably want to offer implementations
   * of the needed widgets!
   */
  static NS_DEFINE_IID(kInsButtonIID, NS_IBUTTON_IID);

  nsIButton * button = nsnull;
  nsresult res = QueryInterface(kInsButtonIID,(void**)&button);

  if (NS_OK == res)
  {
    button->SetLabel(aString);
    NS_RELEASE(button);
  }

  return NS_OK;
}

nsresult nsXPFCButton :: SetBounds(const nsRect &aBounds)
{

  if (mWidget == nsnull)
  {
    mThrobber->MoveTo(aBounds.x, aBounds.y);
    return NS_OK;
  } 

  return (nsXPFCCanvas::SetBounds(aBounds));

}

nsEventStatus nsXPFCButton :: OnPaint(nsIRenderingContext& aRenderingContext,
                                      const nsRect& aDirtyRect)
{
  if (GetLabel() == "Throbber")
  {
    mThrobber->Hide();
    mThrobber->Show();
  }
  return (nsXPFCCanvas::OnPaint(aRenderingContext,aDirtyRect));
}