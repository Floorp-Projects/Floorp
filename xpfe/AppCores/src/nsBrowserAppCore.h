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
#ifndef nsBrowserAppCore_h___
#define nsBrowserAppCore_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIDOMBrowserAppCore.h"
#include "nsBaseAppCore.h"
#include "nsINetSupport.h"
#include "nsIStreamObserver.h"
#include "nsIURLListener.h"
#include "nsIDocumentLoaderObserver.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIURL;
class nsIWebShellWindow;
class nsIGlobalHistory;
struct nsIFindComponent;


////////////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserAppCore : public nsBaseAppCore, 
                         public nsIDOMBrowserAppCore,
                         public nsINetSupport,
                         //     public nsIStreamObserver,
                         public nsIDocumentLoaderObserver
{
  public:

    nsBrowserAppCore();
    virtual ~nsBrowserAppCore();
                 
    NS_DECL_ISUPPORTS_INHERITED

    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 

    NS_IMETHOD    Back();
    NS_IMETHOD    Forward();

    NS_IMETHOD    WalletEditor();
    NS_IMETHOD    WalletSafeFillin(nsIDOMWindow* aWin);
    NS_IMETHOD    WalletQuickFillin(nsIDOMWindow* aWin);
    NS_IMETHOD    WalletSamples();
    NS_IMETHOD    SignonViewer();
    NS_IMETHOD    CookieViewer();

    NS_IMETHOD    LoadUrl(const nsString& aUrl);
    NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    NewWindow();
    NS_IMETHOD    OpenWindow();
    NS_IMETHOD    PrintPreview();
    NS_IMETHOD    Print();
    NS_IMETHOD    Copy();
    NS_IMETHOD    Close();
    NS_IMETHOD    Exit();
    NS_IMETHOD    Find();
    NS_IMETHOD    FindNext();
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset); 
    NS_IMETHOD    LoadInitialPage();

#if 0
    // nsIStreamObserver
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);
#endif /* 0 */

    // nsIDocumentLoaderObserver
    NS_IMETHOD OnStartDocumentLoad(nsIURL* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIURL *aUrl, PRInt32 aStatus);
    NS_IMETHOD OnStartURLLoad(nsIURL* aURL, const char* aContentType, 
                            nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIURL* aURL, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus);


    // nsINetSupport
    NS_IMETHOD_(void) Alert(const nsString &aText);
  
    NS_IMETHOD_(PRBool) Confirm(const nsString &aText);
  NS_IMETHOD HandleUnknownContentType( nsIURL *aURL,
                                       const char *aContentType,
                                       const char *aCommand );


    NS_IMETHOD_(PRBool) Prompt(const nsString &aText,
                               const nsString &aDefault,
                               nsString &aResult);

    NS_IMETHOD_(PRBool) PromptUserAndPassword(const nsString &aText,
                                              nsString &aUser,
                                              nsString &aPassword);

    NS_IMETHOD_(PRBool) PromptPassword(const nsString &aText,
                                       nsString &aPassword);  

  protected:
    NS_IMETHOD DoDialog();
    NS_IMETHOD ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);
    void SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName);
    void       InitializeSearch();
    void       ResetSearchContext();

    nsIScriptContext   *mToolbarScriptContext;
    nsIScriptContext   *mContentScriptContext;

    nsIDOMWindow       *mToolbarWindow;
    nsIDOMWindow       *mContentWindow;

    nsIWebShellWindow  *mWebShellWin;
    nsIWebShell *       mWebShell;
    nsIWebShell *       mContentAreaWebShell;

    nsIGlobalHistory* mGHistory;

    static nsIFindComponent *  mFindComponent;
    nsISupports *       mSearchContext;
};

#endif // nsBrowserAppCore_h___
