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

#include "nsXPFCTabWidget.h"
#include "nsxpfcCIID.h"
#include "nspr.h"
#include "nsITabWidget.h"
#include "nsWidgetsCID.h"
#include "nsXPFCToolkit.h"

static NS_DEFINE_IID(kISupportsIID,       NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kCXPFCTabWidgetCID,  NS_XPFC_TABWIDGET_CID);
static NS_DEFINE_IID(kCIXPFCTabWidgetIID, NS_IXPFC_TABWIDGET_IID);
static NS_DEFINE_IID(kCTabWidgetCID,      NS_TABWIDGET_CID);
static NS_DEFINE_IID(kInsTabWidgetIID,    NS_ITABWIDGET_IID);
static NS_DEFINE_IID(kIWidgetIID,         NS_IWIDGET_IID);

#define DEFAULT_WIDTH  50
#define DEFAULT_HEIGHT 50

nsXPFCTabWidget :: nsXPFCTabWidget(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
}

nsXPFCTabWidget :: ~nsXPFCTabWidget()
{
}

nsresult nsXPFCTabWidget::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kCXPFCTabWidgetCID);                         
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCTabWidget *)this;                                        
    AddRef();                                                            
    return NS_OK;                                                        
  }                                                                      
  if (aIID.Equals(kCIXPFCTabWidgetIID)) {                                          
    *aInstancePtr = (void*) (nsIXPFCTabWidget *)this;                                        
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

NS_IMPL_ADDREF(nsXPFCTabWidget)
NS_IMPL_RELEASE(nsXPFCTabWidget)

nsresult nsXPFCTabWidget :: Init()
{
  nsresult res = nsXPFCCanvas::Init();    

  return res;
}

nsresult nsXPFCTabWidget :: SetParameter(nsString& aKey, nsString& aValue)
{
  return (nsXPFCCanvas::SetParameter(aKey, aValue));
}

nsresult nsXPFCTabWidget :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}

nsresult nsXPFCTabWidget :: CreateView()
{
  nsresult res = NS_OK;
#if 0
  nsIWidget * parent = GetWidget();

  res = LoadWidget(kCTabWidgetCID);

  nsITabWidget * tab_widget = nsnull;
  res = QueryInterface(kInsTabWidgetIID,(void**)&tab_widget);

  if (NS_OK == res)
  {
    nsIWidget * tw = nsnull;

    res = tab_widget->QueryInterface(kIWidgetIID,(void**)&tw);

    if (NS_OK == res)
    {

      nsSize size ;
    
      GetClassPreferredSize(size);

      nsRect rect(0,0,size.width,size.height);

      tw->Create(parent, 
                 rect, 
                 gXPFCToolkit->GetShellEventCallback(), 
                 nsnull);

      tw->Show(PR_TRUE);
      SetParameter(nsString("tab1"),nsString("Tab1"));

      NS_RELEASE(tw);

    }

    NS_RELEASE(tab_widget);
  }
  
  SetVisibility(PR_FALSE);      
#endif
  return res;
}
