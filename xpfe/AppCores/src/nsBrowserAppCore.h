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

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIURL;
class nsIWebShellWindow;
class nsIHistoryDataSource;


////////////////////////////////////////////////////////////////////////////////
// nsBrowserAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsBrowserAppCore : public nsBaseAppCore, 
                         public nsIDOMBrowserAppCore,
                         public nsINetSupport,
                         public nsIStreamObserver,
                         public nsIURLListener
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
    NS_IMETHOD    WalletSafeFillin();
    NS_IMETHOD    WalletQuickFillin();
    NS_IMETHOD    WalletSamples();

    NS_IMETHOD    LoadUrl(const nsString& aUrl);
    NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetDisableCallback(const nsString& aScript);
    NS_IMETHOD    SetEnableCallback(const nsString& aScript);
    NS_IMETHOD    NewWindow();
    NS_IMETHOD    PrintPreview();
    NS_IMETHOD    Print();
    NS_IMETHOD    Copy();
    NS_IMETHOD    Close();
    NS_IMETHOD    Exit();
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset); 

    // nsIStreamObserver
    NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
    NS_IMETHOD OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax);
    NS_IMETHOD OnStatus(nsIURL* aURL, const PRUnichar* aMsg);
    NS_IMETHOD OnStopBinding(nsIURL* aURL, nsresult aStatus, const PRUnichar* aMsg);


    // nsIURlListener methods
  NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
                         const PRUnichar* aURL,
                         nsLoadType aReason);

  NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
                          const PRUnichar* aURL);

  NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
                             const PRUnichar* aURL,
                             PRInt32 aProgress,
                             PRInt32 aProgressMax);

  NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
                        const PRUnichar* aURL,
                        PRInt32 aStatus);

    // nsINetSupport
    NS_IMETHOD_(void) Alert(const nsString &aText);
  
    NS_IMETHOD_(PRBool) Confirm(const nsString &aText);

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

    nsString            mEnableScript;     
    nsString            mDisableScript;     

    nsIScriptContext   *mToolbarScriptContext;
    nsIScriptContext   *mContentScriptContext;

    nsIDOMWindow       *mToolbarWindow;
    nsIDOMWindow       *mContentWindow;

    nsIWebShellWindow  *mWebShellWin;
    nsIWebShell *       mWebShell;
    nsIWebShell *       mContentAreaWebShell;

    nsIHistoryDataSource* mHistory;
};

#endif // nsBrowserAppCore_h___
