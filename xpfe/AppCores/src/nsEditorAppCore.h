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
#include "nsIDocumentLoaderObserver.h"
#include "nsBaseAppCore.h"
#include "nsINetSupport.h"
#include "nsIStreamObserver.h"
#include "nsVoidArray.h"
#include "nsTextServicesCID.h"
#include "nsISpellChecker.h"

class nsIBrowserWindow;
class nsIWebShell;
class nsIScriptContext;
class nsIDOMWindow;
class nsIURI;
class nsIWebShellWindow;
class nsIPresShell;
class nsIHTMLEditor;
class nsITextEditor;
class nsIOutputStream;

//#define TEXT_EDITOR 1

////////////////////////////////////////////////////////////////////////////////
// nsEditorAppCore:
////////////////////////////////////////////////////////////////////////////////

class nsEditorAppCore : public nsBaseAppCore, 
                        public nsIDOMEditorAppCore,
                        public nsIDocumentLoaderObserver
{
  public:

    nsEditorAppCore();
    virtual ~nsEditorAppCore();

    NS_DECL_ISUPPORTS_INHERITED
    
    NS_IMETHOD    GetScriptObject(nsIScriptContext *aContext, void** aScriptObject);
    NS_IMETHOD    Init(const nsString& aId);
    NS_IMETHOD    GetId(nsString& aId) { return nsBaseAppCore::GetId(aId); } 
    NS_IMETHOD    SetDocumentCharset(const nsString& aCharset)  { return nsBaseAppCore::SetDocumentCharset(aCharset); } 

    NS_IMETHOD    GetEditorDocument(nsIDOMDocument** aEditorDocument);
    NS_IMETHOD    GetSelectedElement(const nsString& aTagName, nsIDOMElement** aReturn);
    NS_IMETHOD    CreateElementWithDefaults(const nsString& aTagName, nsIDOMElement** aReturn);

  // XXX  aReturn in InsertElement is always forced to zero, should be removed
    NS_IMETHOD    InsertElement(nsIDOMElement* aElement, PRBool aDeleteSelection, nsIDOMElement** aReturn);
    NS_IMETHOD    InsertLinkAroundSelection(nsIDOMElement* aAnchorElement);
    NS_IMETHOD    SelectElement(nsIDOMElement* aElement);
    NS_IMETHOD    SetCaretAfterElement(nsIDOMElement* aElement);

	  NS_IMETHOD    SetEditorType(const nsString& aEditorType);
		NS_IMETHOD    SetTextProperty(const nsString& aProp, 
                                  const nsString& aAttr, 
                                  const nsString& aValue);
		NS_IMETHOD    RemoveTextProperty(const nsString& aProp, const nsString& aAttr);

	  NS_IMETHOD    GetTextProperty(const nsString& aProp,
	  															const nsString& aAttr,
	  															const nsString& aValue,
	  															nsString& aFirstHas,
	  															nsString& aAnyHas,
	  															nsString& aAllHas);

    NS_IMETHOD    SetBackgroundColor(const nsString& aColor);
    NS_IMETHOD    SetBodyAttribute(const nsString& aAttr, const nsString& aValue);
		NS_IMETHOD    GetParagraphFormat(nsString& aParagraphFormat);
    NS_IMETHOD    SetParagraphFormat(const nsString& aParagraphFormat);

    NS_IMETHOD    GetWrapColumn(PRInt32* aWrapColumn);
    NS_IMETHOD    SetWrapColumn(PRInt32 aWrapColumn);

		NS_IMETHOD    GetContentsAsText(nsString& aContentsAsText);
		NS_IMETHOD    GetContentsAsHTML(nsString& aContentsAsHTML);
		// can't use overloading in interfaces
		NS_IMETHOD    GetContentsAsTextStream(nsIOutputStream* aContentsAsText);
		NS_IMETHOD    GetContentsAsHTMLStream(nsIOutputStream* aContentsAsHTML);

		NS_IMETHOD    GetEditorSelection(nsIDOMSelection** aEditorSelection);

	  NS_IMETHOD    NewWindow();
	  NS_IMETHOD    Open();
	  NS_IMETHOD    Save();
  	NS_IMETHOD    SaveAs();
  	NS_IMETHOD    CloseWindow();
    NS_IMETHOD    PrintPreview();
	  NS_IMETHOD    Print();
    NS_IMETHOD    Exit();
    NS_IMETHOD    GetLocalFileURL(nsIDOMWindow* aParent, const nsString& aFilterType, nsString& aReturn);

    NS_IMETHOD    Undo();
    NS_IMETHOD    Redo();
    NS_IMETHOD    Back();

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
    NS_IMETHOD    PasteAsQuotation();
    NS_IMETHOD    PasteAsCitedQuotation(const nsString& aCiteString);
    NS_IMETHOD    SelectAll();

    NS_IMETHOD    InsertAsQuotation(const nsString& aquotedText);
    NS_IMETHOD    InsertAsCitedQuotation(const nsString& aquotedText,
                                         const nsString& aCiteString);

    NS_IMETHOD		InsertText(const nsString& textToInsert);
		NS_IMETHOD    Find();
		NS_IMETHOD    FindNext();

    // These next two will be replaced with the SetElementProperties
    NS_IMETHOD		InsertLink();
    NS_IMETHOD		InsertImage();
    NS_IMETHOD		InsertList(const nsString& aListType);
    NS_IMETHOD		Indent(const nsString& aIndent);
    NS_IMETHOD		Align(const nsString& aAlign);

    NS_IMETHOD    StartSpellChecking(nsString& aFirstMisspelledWord);
    NS_IMETHOD    GetNextMisspelledWord(nsString& aNextMisspelledWord);
    NS_IMETHOD    GetSuggestedWord(nsString& aSuggestedWord);
    NS_IMETHOD    CheckCurrentWord(const nsString& aSuggestedWord, PRBool* aIsMisspelled);
    NS_IMETHOD    ReplaceWord(const nsString& aMisspelledWord, const nsString& aReplaceWord, PRBool aAllOccurrences);
    NS_IMETHOD    IgnoreWordAllOccurrences(const nsString& aWord);
    NS_IMETHOD    AddWordToDictionary(const nsString& aWord);
    NS_IMETHOD    RemoveWordFromDictionary(const nsString& aWord);
    NS_IMETHOD    GetPersonalDictionaryWord(nsString& aSuggestedWord);
    NS_IMETHOD    CloseSpellChecking();

	  NS_IMETHOD    BeginBatchChanges();
	  NS_IMETHOD    EndBatchChanges();

    NS_IMETHOD    RunUnitTests();

    // nsIDocumentLoaderObserver
    NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aCommand);
    NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* loader, nsIURI *aUrl, PRInt32 aStatus,
								 nsIDocumentLoaderObserver * aObserver);
    NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, const char* aContentType, 
                           		 nsIContentViewer* aViewer);
    NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
    NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, nsString& aMsg);
    NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* loader, nsIURI* aURL, PRInt32 aStatus);
    NS_IMETHOD HandleUnknownContentType(nsIDocumentLoader* loader,
                                        nsIURI *aURL,
                                        const char *aContentType,
                                        const char *aCommand );
  protected:
    nsCOMPtr<nsISpellChecker> mSpellChecker;
    nsStringArray   mSuggestedWordList;
    PRInt32         mSuggestedWordIndex;
    NS_IMETHOD      DeleteSuggestedWordList();

  	typedef enum {
  		ePlainTextEditorType = 1,
  		eHTMLTextEditorType = 2
  	} EEditorType;
  	
    nsIPresShell* 	GetPresShellFor(nsIWebShell* aWebShell);
    NS_IMETHOD 			DoEditorMode(nsIWebShell *aWebShell);
    NS_IMETHOD	 		ExecuteScript(nsIScriptContext * aContext, const nsString& aScript);
    NS_IMETHOD			InstantiateEditor(nsIDOMDocument *aDoc, nsIPresShell *aPresShell);
    NS_IMETHOD			RemoveOneProperty(const nsString& aProp, const nsString& aAttr);
    void 						SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName);
		NS_IMETHOD			CreateWindowWithURL(const char* urlStr);
		NS_IMETHOD  	  PrepareDocumentForEditing();
		NS_IMETHOD      DoFind(PRBool aFindNext);
		
    nsString            mEnableScript;     
    nsString            mDisableScript;     

    nsIScriptContext   *mToolbarScriptContext;
    nsIScriptContext   *mContentScriptContext;

    nsIDOMWindow       *mToolbarWindow;				// weak reference
    nsIDOMWindow       *mContentWindow;				// weak reference

    nsIWebShellWindow  *mWebShellWin;					// weak reference
    nsIWebShell        *mWebShell;						// weak reference
    nsIWebShell        *mContentAreaWebShell;	// weak reference

		EEditorType					mEditorType;
		nsString						mEditorTypeString;	// string which describes which editor type will be instantiated (lowercased)
    nsCOMPtr<nsISupports>	 	mEditor;						// this can be either an HTML or plain text (or other?) editor

    nsCOMPtr<nsISupports>   mSearchContext;		// context used for search and replace. Owned by the appshell.
};

#endif // nsEditorAppCore_h___
