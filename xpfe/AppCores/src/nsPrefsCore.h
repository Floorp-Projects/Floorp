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
#ifndef nsPrefsCorePrivate_h___
#define nsPrefsCorePrivate_h___

//#include "nsAppCores.h"

#include "nscore.h"
#include "nsISupports.h"

#include "nsIDOMPrefsCore.h"
#include "nsBaseAppCore.h"
#include "prtypes.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIPref;
class nsIDOMNode;
class nsIDOMHTMLInputElement;
class nsString;

//========================================================================================
class nsPrefsCore
//========================================================================================
  : public nsBaseAppCore 
  , public nsIDOMPrefsCore
{
  public:

    nsPrefsCore();
    virtual ~nsPrefsCore();
                 

    NS_DECL_ISUPPORTS_INHERITED
    NS_IMETHOD           GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD           Init(const nsString& aId);
    NS_IMETHOD           GetId(nsString& aId)
                         {
                             return nsBaseAppCore::GetId(aId);
                         } 
    NS_IMETHOD           SetDocumentCharset(const nsString& aCharset)
                         {
                             return nsBaseAppCore::SetDocumentCharset(aCharset);
                         } 

    NS_DECL_IDOMPREFSCORE
    
	enum TypeOfPref
	{
	    eNoType        = 0
	  , eBool
	  , eInt
	  , eString
	  , ePath
	};

  protected:
    
    nsresult             InitializePrefsManager();
    nsresult             InitializePrefWidgets();
    nsresult             InitializeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             InitializeOneWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);
    nsresult             FinalizePrefWidgets();
    nsresult             FinalizeWidgetsRecursive(nsIDOMNode* inParentNode);
    nsresult             FinalizeOneWidget(
                             nsIDOMHTMLInputElement* inElement,
                             const nsString& inWidgetType,
                             const char* inPrefName,
                             TypeOfPref inPrefType,
                             PRInt16 inPrefOrdinal);


  protected:

    nsString             mTreeScript;     
    nsString             mPanelScript;     

    nsIScriptContext*    mTreeScriptContext;
    nsIScriptContext*    mPanelScriptContext;
    
    nsIDOMWindow*        mTreeWindow;
    nsIDOMWindow*        mPanelWindow;
    
    nsIPref*             mPrefs;
};

#endif // nsPrefsCore_h___
