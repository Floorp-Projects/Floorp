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
#ifndef nsEditorAppCore_h___
#define nsEditorAppCore_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsString.h"
#include "nsISupports.h"

#include "nsIDOMEditorAppCore.h"
#include "nsBaseAppCore.h"
#include "nsINetSupport.h"
#include "nsIStreamObserver.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIURL;
class nsIWebShellWindow;
class nsIPresShell;
class nsIHTMLEditor;

////////////////////////////////////////////////////////////////////////////////
// nsEditorAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsEditorAppCore : public nsBaseAppCore, 
                        public nsIDOMEditorAppCore
{
  public:

    nsEditorAppCore();
    virtual ~nsEditorAppCore();
                 

    NS_DECL_ISUPPORTS
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset)  { return nsBaseAppCore::SetDocumentCharset(aCharset); } 

    NS_IMETHOD    SetAttribute(const nsString& aAttr, const nsString& aValue);
    NS_IMETHOD    Undo();
    NS_IMETHOD    Redo();
    NS_IMETHOD    Back();
		NS_IMETHOD    GetContentsAsText(nsString& aContentsAsText);
		NS_IMETHOD    GetContentsAsHTML(nsString& aContentsAsHTML);
    NS_IMETHOD    Forward();
    NS_IMETHOD    LoadUrl(const nsString& aUrl);
    NS_IMETHOD    SetToolbarWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetContentWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetWebShellWindow(nsIDOMWindow* aWin);
    NS_IMETHOD    SetDisableCallback(const nsString& aScript);
    NS_IMETHOD    SetEnableCallback(const nsString& aScript);
   // NS_IMETHOD		OutputText(nsString);

    NS_IMETHOD    Cut();
    NS_IMETHOD    Copy();
    NS_IMETHOD    Paste();
    NS_IMETHOD    SelectAll();
    NS_IMETHOD    ShowClipboard();

    NS_IMETHOD		InsertText(const nsString& textToInsert);

    NS_IMETHOD		InsertLink();
    NS_IMETHOD		InsertImage();
    
    NS_IMETHOD    NewWindow();
    NS_IMETHOD    PrintPreview();
    NS_IMETHOD    Close();
    NS_IMETHOD    Exit();


  protected:
    nsIPresShell* GetPresShellFor(nsIWebShell* aWebShell);
    void DoEditorMode(nsIWebShell *aWebShell);
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

    nsIHTMLEditor * mEditor;
    nsIDOMDocument* mDomDoc;
    nsIDOMNode* mCurrentNode;


};

#endif // nsEditorAppCore_h___
