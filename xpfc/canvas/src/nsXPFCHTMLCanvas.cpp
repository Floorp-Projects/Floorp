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

#include "nsXPFCHTMLCanvas.h"
#include "nsxpfcCIID.h"
#include "nsIWebShell.h"
#include "nsIDeviceContext.h"
#include "nsFont.h"
#include "nsIFontMetrics.h"
#include "nspr.h"
#include "nsxpfcstrings.h"
#include "nsIStreamListener.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kXPFCHTMLCanvasCID, NS_XPFC_HTML_CANVAS_CID);

#define kNotFound -1

#define DEFAULT_WIDTH  400
#define DEFAULT_HEIGHT 400

nsXPFCHTMLCanvas :: nsXPFCHTMLCanvas(nsISupports* outer) : nsXPFCCanvas(outer)
{
  NS_INIT_REFCNT();
  mWebShell = nsnull;
}

nsXPFCHTMLCanvas :: ~nsXPFCHTMLCanvas()
{
  if (nsnull != mWebShell) {
    mWebShell->Destroy();
    NS_RELEASE(mWebShell);
  }
}

nsresult nsXPFCHTMLCanvas::QueryInterface(REFNSIID aIID, void** aInstancePtr)      
{                                                                        
  if (NULL == aInstancePtr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  static NS_DEFINE_IID(kISupportsIID,  NS_ISUPPORTS_IID);                 
  static NS_DEFINE_IID(kClassIID, kXPFCHTMLCanvasCID);
  if (aIID.Equals(kClassIID)) {                                          
    *aInstancePtr = (void*) (nsXPFCHTMLCanvas *)this;                                        
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

NS_IMPL_ADDREF(nsXPFCHTMLCanvas)
NS_IMPL_RELEASE(nsXPFCHTMLCanvas)

nsresult nsXPFCHTMLCanvas :: Init()
{  
  static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
  static NS_DEFINE_IID(kCWebShellCID, NS_WEB_SHELL_CID);

  nsresult res = nsRepository::CreateInstance(kCWebShellCID, 
                                              nsnull, 
                                              kIWebShellIID, 
                                              (void**)&mWebShell);


  if (res != NS_OK)
    return res;

  mWebShell->Init(GetWidget()->GetNativeData(NS_NATIVE_WIDGET), 
                  0,
                  0,
                  DEFAULT_WIDTH, 
                  DEFAULT_HEIGHT,
                  nsScrollPreference_kAuto, PR_TRUE);

  nsIWebShellContainer * container = nsnull;

  static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
  static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);

  res = mWebShell->QueryInterface(kIWebShellContainerIID, (void**)&container);

  if (res == NS_OK)
  {
    //mWebShell->SetContainecontainer);
    NS_RELEASE(container);
  }

  nsIStreamObserver * observer = nsnull;

  res = mWebShell->QueryInterface(kIStreamObserverIID, (void**)&observer);

  if (res == NS_OK)
  {
    mWebShell->SetObserver(observer);
    NS_RELEASE(observer);
  }

  //mWebShell->SetPrefs(aPrefs);
  mWebShell->SetBounds(0,0,400,400);

  mWebShell->Show();

  nsString url("about:blank");

  PRUnichar* p = url.ToNewUnicode();

  mWebShell->LoadURL(p, nsnull);

  delete p;

  return NS_OK;
}

nsEventStatus nsXPFCHTMLCanvas :: OnResize(nscoord aX, nscoord aY, nscoord aWidth, nscoord aHeight)
{
  return (nsXPFCCanvas::OnResize(aX, aY, aWidth, aHeight));
}

nsresult nsXPFCHTMLCanvas :: SetBounds(const nsRect &aBounds)
{
  nsXPFCCanvas::SetBounds(aBounds);
  if (mWebShell != nsnull)
    mWebShell->SetBounds(aBounds.x,aBounds.y,aBounds.width,aBounds.height);
  return (NS_OK);
}


nsEventStatus nsXPFCHTMLCanvas :: OnPaint(nsIRenderingContext& aRenderingContext,
                                          const nsRect& aDirtyRect)

{
  nsEventStatus es = nsXPFCCanvas::OnPaint(aRenderingContext,aDirtyRect);
  if (mWebShell != nsnull)
    mWebShell->Repaint(PR_FALSE);
  return (es);
}

nsEventStatus nsXPFCHTMLCanvas :: HandleEvent(nsGUIEvent *aEvent)
{
  return (nsXPFCCanvas::HandleEvent(aEvent));
}

nsresult nsXPFCHTMLCanvas :: GetClassPreferredSize(nsSize& aSize)
{
  aSize.width  = DEFAULT_WIDTH;
  aSize.height = DEFAULT_HEIGHT;
  return (NS_OK);
}


nsresult nsXPFCHTMLCanvas :: SetParameter(nsString& aKey, nsString& aValue)
{
  if (aKey.EqualsIgnoreCase(XPFC_STRING_SRC)) {

    PRUnichar* p = aValue.ToNewUnicode();

    mWebShell->LoadURL(p, nsnull);

    delete p;

  }   
  
  return (nsXPFCCanvas::SetParameter(aKey,aValue));
}
