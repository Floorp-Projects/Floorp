
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include <stdio.h>
#include "nsEditorAppCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsIDOMAppCoresManager.h"

#include "nsIScriptContext.h"
#include "nsIScriptContextOwner.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMDocument.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"

#include "nsIScriptGlobalObject.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIURL.h"
#include "nsIWidget.h"
#include "plevent.h"

#include "nsIAppShell.h"
#include "nsIAppShellService.h"
#include "nsAppShellCIDs.h"

#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsEditorMode.h"

///////////////////////////////////////
// Editor Includes
///////////////////////////////////////
//#include "nsEditorMode.h"
#include "nsEditorInterfaces.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMEventCapturer.h"
#include "nsString.h"
#include "nsIDOMText.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"

#include "nsIEditor.h"
#include "nsITextEditor.h"
#include "nsIHTMLEditor.h"
#include "nsEditorCID.h"

#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
///////////////////////////////////////

//#define NEW_CLIPBOARD_SUPPORT

#ifdef NEW_CLIPBOARD_SUPPORT
// Drag & Drop, Clipboard
#include "nsWidgetsCID.h"
#include "nsIClipboard.h"
#include "nsITransferable.h"
#include "nsIDataFlavor.h"
#include "nsISupportsArray.h"

// Drag & Drop, Clipboard Support
static NS_DEFINE_IID(kIClipboardIID,    NS_ICLIPBOARD_IID);
static NS_DEFINE_IID(kIDataFlavorIID,   NS_IDATAFLAVOR_IID);
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
#endif


/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,       NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kEditorAppCoreCID,         NS_EDITORAPPCORE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEditorAppCoreIID,         NS_IDOMEDITORAPPCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::GetIID());


static NS_DEFINE_IID(kINetSupportIID,            NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);

static NS_DEFINE_IID(kIWebShellWindowIID,        NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,        NS_IDOCUMENT_VIEWER_IID);

static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);

static NS_DEFINE_IID(kITextEditorIID, NS_ITEXTEDITOR_IID);
static NS_DEFINE_CID(kTextEditorCID, NS_TEXTEDITOR_CID);

#define APP_DEBUG 0 

/////////////////////////////////////////////////////////////////////////
// nsEditorAppCore
/////////////////////////////////////////////////////////////////////////

nsEditorAppCore::nsEditorAppCore()
{
#ifdef APP_DEBUG
  printf("Created nsEditorAppCore\n");
#endif

  mScriptObject         = nsnull;
  mToolbarWindow        = nsnull;
  mToolbarScriptContext = nsnull;
  mContentWindow        = nsnull;
  mContentScriptContext = nsnull;
  mWebShellWin          = nsnull;
  mWebShell             = nsnull;
  //mCurrentNode          = nsnull;
  //mDomDoc               = nsnull;
  mEditor               = nsnull;

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsEditorAppCore::~nsEditorAppCore()
{
  NS_IF_RELEASE(mToolbarWindow);
  NS_IF_RELEASE(mToolbarScriptContext);
  NS_IF_RELEASE(mContentWindow);
  NS_IF_RELEASE(mContentScriptContext);
  NS_IF_RELEASE(mWebShellWin);
  NS_IF_RELEASE(mWebShell);
  DecInstanceCount();  
}


NS_IMPL_ADDREF_INHERITED(nsEditorAppCore, nsBaseAppCore)
NS_IMPL_RELEASE_INHERITED(nsEditorAppCore, nsBaseAppCore)

NS_IMETHODIMP 
nsEditorAppCore::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if ( aIID.Equals(kIEditorAppCoreIID) ) {
    *aInstancePtr = (void*) ((nsIDOMEditorAppCore*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kINetSupportIID)) {
    *aInstancePtr = (void*) ((nsINetSupport*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtr = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
 

  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsEditorAppCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  NS_PRECONDITION(nsnull != aScriptObject, "null arg");
  nsresult res = NS_OK;
  if (nsnull == mScriptObject) 
  {
      res = NS_NewScriptEditorAppCore(aContext, 
                                (nsISupports *)(nsIDOMEditorAppCore*)this, 
                                nsnull, 
                                &mScriptObject);
  }

  *aScriptObject = mScriptObject;
  return res;
}

NS_IMETHODIMP    
nsEditorAppCore::Init(const nsString& aId)
{  
  nsBaseAppCore::Init(aId);

	nsAutoString		editorType = "html";			// default to creating HTML editor
	mEditorTypeString = editorType;
	mEditorTypeString.ToLowerCase();

  // register object into Service Manager
  static NS_DEFINE_IID(kIDOMAppCoresManagerIID, NS_IDOMAPPCORESMANAGER_IID);
  static NS_DEFINE_IID(kAppCoresManagerCID,  NS_APPCORESMANAGER_CID);

  nsIDOMAppCoresManager * appCoreManager;
  nsresult rv = nsServiceManager::GetService(kAppCoresManagerCID,
                                             kIDOMAppCoresManagerIID,
                                             (nsISupports**)&appCoreManager);
  if (NS_OK == rv) {
	  appCoreManager->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
  }
	return rv;
}

nsIPresShell*
nsEditorAppCore::GetPresShellFor(nsIWebShell* aWebShell)
{
  nsIPresShell* shell = nsnull;
  if (nsnull != aWebShell) {
    nsIContentViewer* cv = nsnull;
    aWebShell->GetContentViewer(&cv);
    if (nsnull != cv) {
      nsIDocumentViewer* docv = nsnull;
      cv->QueryInterface(kIDocumentViewerIID, (void**) &docv);
      if (nsnull != docv) {
        nsIPresContext* cx;
        docv->GetPresContext(cx);
	      if (nsnull != cx) {
	        cx->GetShell(&shell);
	        NS_RELEASE(cx);
	      }
        NS_RELEASE(docv);
      }
      NS_RELEASE(cv);
    }
  }
  return shell;
}


// tell the appcore what type of editor to instantiate
// this must be called before the editor has been instantiated,
// otherwise, an error is returned.
NS_METHOD
nsEditorAppCore::SetEditorType(const nsString& aEditorType)
{	
	if (mEditor != nsnull)
		return NS_ERROR_ALREADY_INITIALIZED;
		
	nsAutoString	theType = aEditorType;
	theType.ToLowerCase();
	if (theType == "text")
	{
		mEditorTypeString = theType;
		return NS_OK;
	}
	else if (theType == "html" || theType == "")
	{
		mEditorTypeString = theType;
		return NS_OK;
	}
	else
	{
		NS_WARNING("Editor type not recognized");
		return NS_ERROR_UNEXPECTED;
	}
}


NS_METHOD
nsEditorAppCore::InstantiateEditor(nsIDOMDocument *aDoc, nsIPresShell *aPresShell)
{
	NS_PRECONDITION(aDoc && aPresShell, "null ptr");
	if (!aDoc || !aPresShell)
	    return NS_ERROR_NULL_POINTER;

	nsresult err = NS_OK;
	
	if (mEditorTypeString == "text")
	{
		nsITextEditor *editor = nsnull;
		err = nsComponentManager::CreateInstance(kTextEditorCID, nsnull, kITextEditorIID, (void **)&editor);
		if(!editor)
			err = NS_ERROR_OUT_OF_MEMORY;
			
		if (NS_SUCCEEDED(err))
		{
			err = editor->Init(aDoc, aPresShell);
			if (NS_SUCCEEDED(err) && editor)
			{
				// The EditorAppCore "owns" the editor
				mEditor = editor;
				mEditorType = ePlainTextEditorType;
			}
		}
	}
	else if (mEditorTypeString == "html" || mEditorTypeString == "")	// empty string default to HTML editor
	{
		nsIHTMLEditor *editor = nsnull;
		err = nsComponentManager::CreateInstance(kHTMLEditorCID, nsnull, kIHTMLEditorIID, (void **)&editor);
		if(!editor)
			err = NS_ERROR_OUT_OF_MEMORY;
			
		if (NS_SUCCEEDED(err))
		{
			err = editor->Init(aDoc, aPresShell);
			if (NS_SUCCEEDED(err) && editor)
			{
				// The EditorAppCore "owns" the editor
				mEditor = editor;
				mEditorType = eHTMLTextEditorType;
			}
		}
	}
	else
	{
		err = NS_ERROR_INVALID_ARG;		// this is not an editor we know about
#if DEBUG
		nsString	errorMsg = "Failed to init editor. Unknown editor type \"";
		errorMsg += mEditorTypeString;
		errorMsg += "\"\n";
		char	*errorMsgCString = errorMsg.ToNewCString();
     	NS_WARNING(errorMsgCString);
     	delete [] errorMsgCString;
#endif
	}
	
	return err;
}


NS_METHOD
nsEditorAppCore::DoEditorMode(nsIWebShell *aWebShell)
{
	nsresult	err = NS_OK;
	
	NS_PRECONDITION(aWebShell, "null ptr");
	if (!aWebShell)
	    return NS_ERROR_NULL_POINTER;

	nsCOMPtr<nsIContentViewer> contViewer;
	aWebShell->GetContentViewer(getter_AddRefs(contViewer));
	if (contViewer)
	{
		nsCOMPtr<nsIDocumentViewer> docViewer;
		if (NS_SUCCEEDED(contViewer->QueryInterface(kIDocumentViewerIID, (void**)getter_AddRefs(docViewer))))
		{
			nsCOMPtr<nsIDocument> aDoc;
			docViewer->GetDocument(*getter_AddRefs(aDoc));
			if (aDoc)
			{
				nsCOMPtr<nsIDOMDocument> aDOMDoc;
				if (NS_SUCCEEDED(aDoc->QueryInterface(kIDOMDocumentIID, (void**)getter_AddRefs(aDOMDoc))))
				{
					nsIPresShell* presShell = GetPresShellFor(aWebShell);
					if( presShell )
					{
						err = InstantiateEditor(aDOMDoc, presShell);
					}
					NS_IF_RELEASE(presShell);
				}
			}
		}

#if 0    
// Not sure if this makes sense any more
    PRInt32 i, n;
    aWebShell->GetChildCount(n);
    for (i = 0; i < n; i++) {
      nsIWebShell* mChild;
      aWebShell->ChildAt(i, mChild);
      DoEditorMode(mChild);
      NS_RELEASE(mChild);
    }
#endif
  }
  
  return err;
}

// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
NS_IMETHODIMP    
nsEditorAppCore::SetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue)
{
	nsIAtom		*styleAtom = nsnull;
	nsresult	err = NS_NOINTERFACE;

	styleAtom = NS_NewAtom(aProp);			/// XXX Hack alert! Look in nsIEditProperty.h for this

	if (! styleAtom)
		return NS_ERROR_OUT_OF_MEMORY;

	// addref it while we're using it
	NS_ADDREF(styleAtom);

	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				// should we allow this?
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->SetTextProperty(styleAtom, &aAttr, &aValue);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->SetTextProperty(styleAtom, &aAttr, &aValue);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

	NS_RELEASE(styleAtom);
	return err;
}



NS_IMETHODIMP
nsEditorAppCore::RemoveOneProperty(const nsString& aProp, const nsString &aAttr)
{
	nsIAtom		*styleAtom = nsnull;
	nsresult	err = NS_NOINTERFACE;

	styleAtom = NS_NewAtom(aProp);			/// XXX Hack alert! Look in nsIEditProperty.h for this

	if (! styleAtom)
		return NS_ERROR_OUT_OF_MEMORY;

	// addref it while we're using it
	NS_ADDREF(styleAtom);

	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				// should we allow this?
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->RemoveTextProperty(styleAtom, &aAttr);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->RemoveTextProperty(styleAtom, &aAttr);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

	NS_RELEASE(styleAtom);
	return err;
}


// the name of the attribute here should be the contents of the appropriate
// tag, e.g. 'b' for bold, 'i' for italics.
NS_IMETHODIMP    
nsEditorAppCore::RemoveTextProperty(const nsString& aProp, const nsString& aAttr)
{
	// OK, I'm really hacking now. This is just so that we can accept 'all' as input.
	// this logic should live elsewhere.
	static const char*	sAllKnownStyles[] = {
		"b",
		"i",
		"u",
		nsnull			// this null is important
	};
	
	nsAutoString	allStr = aProp;
	allStr.ToLowerCase();
	PRBool		doingAll = (allStr == "all");
	nsresult	err = NS_OK;

	if (doingAll)
	{
		nsAutoString	thisAttr;
		const char		**tagName = sAllKnownStyles;
	
		while (*tagName)
		{
			thisAttr.Truncate(0);
			thisAttr += (char *)(*tagName);
		
			err = RemoveOneProperty(thisAttr, aAttr);

			tagName ++;
		}
	
	}
	else
	{
		err = RemoveOneProperty(aProp, aAttr);
	}
	
	return err;
}

NS_IMETHODIMP
nsEditorAppCore::GetTextProperty(const nsString& aProp, const nsString& aAttr, const nsString& aValue, 
                                 PRBool* aFirstHas, PRBool* aAnyHas, PRBool* aAllHas)
{
	nsIAtom		*styleAtom = nsnull;
	nsresult	err = NS_NOINTERFACE;

  PRBool		firstOfSelectionHasProp = PR_FALSE;
	PRBool		anyOfSelectionHasProp = PR_FALSE;
	PRBool		allOfSelectionHasProp = PR_FALSE;
	
	styleAtom = NS_NewAtom(aProp);			/// XXX Hack alert! Look in nsIEditProperty.h for this

	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				// should we allow this?
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->GetTextProperty(styleAtom, &aAttr, &aValue, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->GetTextProperty(styleAtom, &aAttr, &aValue, firstOfSelectionHasProp, anyOfSelectionHasProp, allOfSelectionHasProp);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  if (aFirstHas) *aFirstHas = firstOfSelectionHasProp;
	if (aAnyHas) *aAnyHas = anyOfSelectionHasProp;
	if (aAllHas) *aAllHas = allOfSelectionHasProp;
  	
	NS_RELEASE(styleAtom);
	return err;
}

NS_IMETHODIMP    
nsEditorAppCore::Back()
{
  ExecuteScript(mToolbarScriptContext, mDisableScript);
  ExecuteScript(mContentScriptContext, "window.back();");
  ExecuteScript(mToolbarScriptContext, mEnableScript);
	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Forward()
{
  ExecuteScript(mToolbarScriptContext, mDisableScript);
  ExecuteScript(mContentScriptContext, "window.forward();");
  ExecuteScript(mToolbarScriptContext, mEnableScript);
	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SetDisableCallback(const nsString& aScript)
{
  mDisableScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SetEnableCallback(const nsString& aScript)
{
  mEnableScript = aScript;
	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::LoadUrl(const nsString& aUrl)
{
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SetToolbarWindow(nsIDOMWindow* aWin)
{
	NS_PRECONDITION(aWin != nsnull, "null ptr");
	if (!aWin)
	    return NS_ERROR_NULL_POINTER;

  mToolbarWindow = aWin;
  NS_ADDREF(aWin);
  mToolbarScriptContext = GetScriptContext(aWin);

	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SetContentWindow(nsIDOMWindow* aWin)
{
	NS_PRECONDITION(aWin != nsnull, "null ptr");
	if (!aWin)
	    return NS_ERROR_NULL_POINTER;

  mContentWindow = aWin;
  NS_ADDREF(aWin);
  mContentScriptContext = GetScriptContext(aWin);
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(mContentWindow) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    DoEditorMode(webShell);
    NS_RELEASE(webShell);
  }

  return NS_OK;
}


NS_IMETHODIMP    
nsEditorAppCore::SetWebShellWindow(nsIDOMWindow* aWin)
{
	NS_PRECONDITION(aWin != nsnull, "null ptr");
	if (!aWin)
	    return NS_ERROR_NULL_POINTER;

  if (!mContentWindow) {
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsIScriptGlobalObject> globalObj( do_QueryInterface(aWin) );
  if (!globalObj) {
    return NS_ERROR_FAILURE;
  }

  nsIWebShell * webShell;
  globalObj->GetWebShell(&webShell);
  if (nsnull != webShell) {
    mWebShell = webShell;
    NS_ADDREF(mWebShell);
    const PRUnichar * name;
    webShell->GetName( &name);
    nsAutoString str(name);

#ifdef APP_DEBUG
    char* cstr = str.ToNewCString();
    printf("Attaching to WebShellWindow[%s]\n", str.ToNewCString());
    delete[] cstr;
#endif

    nsIWebShellContainer * webShellContainer;
    webShell->GetContainer(webShellContainer);
    if (nsnull != webShellContainer) {
      if (NS_OK == webShellContainer->QueryInterface(kIWebShellWindowIID, (void**) &mWebShellWin)) {
      }
      NS_RELEASE(webShellContainer);
    }
    NS_RELEASE(webShell);
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::NewWindow()
{  
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Undo()
{ 
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->Undo(1);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->Undo(1);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP    
nsEditorAppCore::Redo()
{  
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->Redo(1);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->Redo(1);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP    
nsEditorAppCore::Cut()
{  
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->Cut();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->Cut();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP    
nsEditorAppCore::Copy()
{  
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->Copy();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->Copy();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP    
nsEditorAppCore::Paste()
{  
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->Paste();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->Paste();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP    
nsEditorAppCore::SelectAll()
{  
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->SelectAll();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->SelectAll();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}


NS_IMETHODIMP
nsEditorAppCore::InsertText(const nsString& textToInsert)
{
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->InsertText(textToInsert);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->InsertText(textToInsert);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}


NS_IMETHODIMP
nsEditorAppCore::GetContentsAsText(nsString& aContentsAsText)
{
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->OutputText(aContentsAsText);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->OutputText(aContentsAsText);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP
nsEditorAppCore::GetContentsAsHTML(nsString& aContentsAsHTML)
{
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->OutputHTML(aContentsAsHTML);
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->OutputHTML(aContentsAsHTML);
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}


NS_IMETHODIMP
nsEditorAppCore::GetEditorDocument(nsIDOMDocument** aEditorDocument)
{

	if (mEditor)
	{
		nsCOMPtr<nsIEditor>	editor = do_QueryInterface(mEditor);
		if (editor)
		{
			return editor->GetDocument(aEditorDocument);
		}
	}
	
	return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsEditorAppCore::GetEditorSelection(nsIDOMSelection** aEditorSelection)
{

	if (mEditor)
	{
		nsCOMPtr<nsIEditor>	editor = do_QueryInterface(mEditor);
		if (editor)
		{
			return editor->GetSelection(aEditorSelection);
		}
	}
	
	return NS_NOINTERFACE;
}

// Pop up the link dialog once we have dialogs ...  for now, hardwire it
NS_IMETHODIMP
nsEditorAppCore::InsertLink()
{
	nsresult	err = NS_NOINTERFACE;
  nsString tmpString ("http://www.mozilla.org/editor/");

	switch (mEditorType)
	{
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->InsertLink(tmpString);
			}
			break;
		case ePlainTextEditorType:
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

// Pop up the image dialog once we have dialogs ...  for now, hardwire it
NS_IMETHODIMP
nsEditorAppCore::InsertImage()
{
	nsresult	err = NS_NOINTERFACE;
	
  nsString url ("http://www.mozilla.org/editor/images/pensplat.gif");
  nsString width("100");
  nsString height("138");
  nsString hspace("0");
  nsString border("1");
  nsString alt ("[pen splat]");
  nsString align ("left");
  
 	switch (mEditorType)
	{
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->InsertImage(url, width, height, hspace, hspace, border, alt, align);
			}
			break;
		case ePlainTextEditorType:
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}


NS_IMETHODIMP
nsEditorAppCore::BeginBatchChanges()
{
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->BeginTransaction();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->BeginTransaction();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

NS_IMETHODIMP
nsEditorAppCore::EndBatchChanges()
{
	nsresult	err = NS_NOINTERFACE;
	
	switch (mEditorType)
	{
		case ePlainTextEditorType:
			{
				nsCOMPtr<nsITextEditor>	textEditor = do_QueryInterface(mEditor);
				if (textEditor)
					err = textEditor->EndTransaction();
			}
			break;
		case eHTMLTextEditorType:
			{
				nsCOMPtr<nsIHTMLEditor>	htmlEditor = do_QueryInterface(mEditor);
				if (htmlEditor)
					err = htmlEditor->EndTransaction();
			}
			break;
		default:
			err = NS_ERROR_NOT_IMPLEMENTED;
	}

  return err;
}

static PRInt32 MakeNewWindow(char* urlName)
{
  nsresult rv;
  nsString controllerCID;

  char *  urlstr=nsnull;
  //char *   progname = nsnull;
  //char *   width=nsnull, *height=nsnull;
  //char *  iconic_state=nsnull;

  nsIAppShellService* appShell = nsnull;
  urlstr = urlName;

  /*
   * Create the Application Shell instance...
   */
  rv = nsServiceManager::GetService(kAppShellServiceCID,
                                    kIAppShellServiceIID,
                                    (nsISupports**)&appShell);
  if (!NS_SUCCEEDED(rv)) {
    goto done;
  }

  /*
   * Post an event to the shell instance to load the AppShell 
   * initialization routines...  
   * 
   * This allows the application to enter its event loop before having to 
   * deal with GUI initialization...
   */
  ///write me...
  nsIURL* url;
  nsIWebShellWindow* newWindow;
  
  rv = NS_NewURL(&url, urlstr);
  if (NS_FAILED(rv)) {
    goto done;
  }

  /*
   * XXX: Currently, the CID for the "controller" is passed in as an argument 
   *      to CreateTopLevelWindow(...).  Once XUL supports "controller" 
   *      components this will be specified in the XUL description...
   */
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  appShell->CreateTopLevelWindow(nsnull, url, controllerCID, newWindow,
              nsnull, nsnull, 615, 480);

  NS_RELEASE(url);
  
done:
  /* Release the shell... */
  if (nsnull != appShell) {
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  }

  return NS_OK;
}

/**
  * 
  * 
 */
static void GenerateBarItem(FILE * fd, char * aFileName, const nsString & aDesc, void * aData, PRUint32 aLen) 
{
	fprintf(fd, "<titledbutton src=\"resource:/res/toolbar/TB_PersonalIcon.gif\" align=\"right\" value=");
  char* desc_str= aDesc.ToNewCString();
  fprintf(fd, "\"%s\"", desc_str);
  delete[] desc_str;
  fprintf(fd, " onclick=\"LoadURL('%s')\"/>\n", aFileName);

  char name[256];
  sprintf(name, "res/samples/%s", aFileName);
  FILE * clipFD = fopen(name, "w");
  if (clipFD) {
    char * str = (char *)aData;
    PRUint32 i;
    for (i=0;i<aLen;i++) {
      fprintf(clipFD, "%c", (str[i] == 0?' ':str[i]));
    }
    fflush(clipFD);
    fclose(clipFD);
  }

}

NS_IMETHODIMP    
nsEditorAppCore::ShowClipboard()
{  

#ifdef NEW_CLIPBOARD_SUPPORT
  nsIClipboard* clipboard;
  nsresult rvv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&clipboard);
  FILE * fd = fopen("res/samples/ClipboardViewer.xul", "w");
  fprintf(fd, "<?xml version=\"1.0\"?> \n");
  fprintf(fd, "<?xml-stylesheet href=\"xul.css\" type=\"text/css\"?> \n"); 
  fprintf(fd, "<!DOCTYPE window> \n"); 
  fprintf(fd, "<window xmlns:html=\"http://www.w3.org/TR/REC-html40\"\n");
  fprintf(fd, "        xmlns=\"http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul\"\n");
  fprintf(fd, "        onload=\"Startup()\"> \n"); 
  fprintf(fd, "  <html:script> \n");
  fprintf(fd, "  function Startup() {\n");
  fprintf(fd, "    appCore = XPAppCoresManager.Find(\"BrowserAppCore\");  \n");
  fprintf(fd, "    if (appCore == null) {\n");
  fprintf(fd, "      appCore = new BrowserAppCore();\n");
  fprintf(fd, "      if (appCore != null) {\n");
  fprintf(fd, "        appCore.Init(\"BrowserAppCore\");\n");
  fprintf(fd, "	      appCore.setContentWindow(window.frames[0]);\n");
  fprintf(fd, "	      appCore.setWebShellWindow(window);\n");
  fprintf(fd, "	      appCore.setToolbarWindow(window);\n");
  fprintf(fd, "        XPAppCoresManager.Add(appCore);  \n");
  fprintf(fd, "      }\n");
  fprintf(fd, "    }\n");
  fprintf(fd, "  }\n");
  fprintf(fd, "  function LoadURL(url) {\n");
  fprintf(fd, "    appCore = XPAppCoresManager.Find(\"BrowserAppCore\");  \n");
  fprintf(fd, "    appCore.loadUrl(\"resource:/res/samples/\"+url);\n");
  fprintf(fd, "  }\n");
  fprintf(fd, "  function Exit() {\n");
  fprintf(fd, "    appCore = XPAppCoresManager.Find(\"BrowserAppCore\");  \n");
  fprintf(fd, "    if (appCore != null) {\n");
  fprintf(fd, "      appCore.close();\n");
  fprintf(fd, "    }\n");
  fprintf(fd, "  }\n");
  fprintf(fd, "  </html:script>\n");
  fprintf(fd, "  <toolbox>\n");
  fprintf(fd, "	  <toolbar id=\"tests\">\n");
  fprintf(fd, "	  <titledbutton src=\"resource:/res/toolbar/TB_NewStop.gif\" align=\"right\" value=\"Exit\" onclick=\"Exit()\"/>\n");

  char firstPage[256];
  strcpy(firstPage, "test0.html");

  if (NS_OK == rvv) {
    nsITransferable * trans;
    clipboard->GetData(trans);
    if (nsnull != trans) {

      // Get the transferable list of data flavors
      nsISupportsArray * dfList;
      trans->GetTransferDataFlavors(&dfList);

      // Walk through flavors and see which flavor is on the clipboard them on the native clipboard,
      PRUint32 i;
      for (i=0;i<dfList->Count();i++) {
        nsIDataFlavor * df;
        nsISupports * supports = dfList->ElementAt(i);
        if (NS_OK == supports->QueryInterface(kIDataFlavorIID, (void **)&df)) {
          nsString mime;
          df->GetMimeType(mime);

          void   * data;
          PRUint32 dataLen;

          trans->GetTransferData(df, &data, &dataLen);
          if (nsnull != data) {
            char clipFileName[256];
            sprintf(clipFileName, "clip%d.", i);
            if (mime.Equals(kTextMime)) {
              strcat(clipFileName, "txt");
            } else if (mime.Equals(kHTMLMime)) {
              // Generate html a "text" like in "view source"
              char htmlsrc[256];
              strcpy(htmlsrc, clipFileName);
              strcat(htmlsrc, "txt");
              GenerateBarItem(fd, htmlsrc, nsAutoString("HTML Src"), data, dataLen);
              strcat(clipFileName, "html");
            } else if (mime.Equals(kXIFMime)) {
              strcat(clipFileName, "txt");
            } else {
              strcat(clipFileName, "txt");
            }

            if (i == 0) {
              strcpy(firstPage, clipFileName);
            }
            nsAutoString desc;
            df->GetHumanPresentableName(desc);
            GenerateBarItem(fd, clipFileName, desc, data, dataLen);

          }
          NS_RELEASE(df);
        }
        NS_RELEASE(supports);
      }
      NS_RELEASE(trans);
    }
    NS_RELEASE(clipboard);
  }
  fprintf(fd, "	  </toolbar>\n");
  fprintf(fd, "  </toolbox>\n");
  fprintf(fd, "\n");

  char name[256];
  sprintf(name, "resource:/res/samples/%s", firstPage);
  fprintf(fd, " <html:iframe html:name=\"content\" html:src=\"%s\" html:width=\"100%c\" html:height=\"500px\"></html:iframe>\n", name, '%');
  fprintf(fd, "</window>\n");
  fclose(fd);

  MakeNewWindow("resource:/res/samples/ClipboardViewer.xul");

#endif

  return NS_OK;
}


//----------------------------------------
void nsEditorAppCore::SetButtonImage(nsIDOMNode * aParentNode, PRInt32 aBtnNum, const nsString &aResName)
{
  PRInt32 count = 0;
  nsCOMPtr<nsIDOMNode> button(FindNamedDOMNode(nsAutoString("button"), aParentNode, count, aBtnNum)); 
  count = 0;
  nsCOMPtr<nsIDOMNode> img(FindNamedDOMNode(nsAutoString("img"), button, count, 1)); 
  nsCOMPtr<nsIDOMHTMLImageElement> imgElement(do_QueryInterface(img));
  if (imgElement) {
    char * str = aResName.ToNewCString();
    imgElement->SetSrc(str);
    delete [] str;
  }

}


NS_IMETHODIMP    
nsEditorAppCore::PrintPreview()
{ 
  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Close()
{  
  return NS_OK;
}


NS_IMETHODIMP    
nsEditorAppCore::Exit()
{  
  nsIAppShellService* appShell = nsnull;

#ifdef NEW_CLIPBOARD_SUPPORT
  nsIClipboard* clipboard;
  nsresult rvv = nsServiceManager::GetService(kCClipboardCID,
                                             kIClipboardIID,
                                             (nsISupports **)&clipboard);

  if (NS_OK == rvv) {
    nsITransferable * trans = nsnull;			// XXX this needs fixin
    clipboard->GetData(trans);
    if (nsnull != trans) {
      if (PR_TRUE == trans->IsLargeDataSet()) {
        // XXX A Dialog goes here to see if they want to "force" a copy 
        // of the data to the clipboard 

        //if (status == IDYES) {
        //  clipboard->ForceDataToClipboard();
        //}

      }
      NS_RELEASE(trans);
    }
    NS_RELEASE(clipboard);
  }

#endif


  /*
   * Create the Application Shell instance...
   */
  nsresult rv = nsServiceManager::GetService(kAppShellServiceCID,
                                             kIAppShellServiceIID,
                                             (nsISupports**)&appShell);
  if (NS_SUCCEEDED(rv)) {
    appShell->Shutdown();
    nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);
  } 
  return NS_OK;
}


NS_IMETHODIMP    
nsEditorAppCore::ExecuteScript(nsIScriptContext * aContext, const nsString& aScript)
{
  if (nsnull != aContext) {
    const char* url = "";
    PRBool isUndefined = PR_FALSE;
    nsString rVal;

#ifdef APP_DEBUG
    char* script_str = aScript.ToNewCString();
    printf("Executing [%s]\n", aScript.ToNewCString());
    delete[] script_str;
#endif

    aContext->EvaluateString(aScript, url, 0, rVal, &isUndefined);
  } 
  return NS_OK;
}


//static nsIDOMDocument* mDomDoc;
//static nsIDOMNode* mCurrentNode;

//static nsIEditor *gEditor;

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kIDOMElementIID, NS_IDOMELEMENT_IID);
static NS_DEFINE_IID(kIEditorIID, NS_IEDITOR_IID);
static NS_DEFINE_CID(kEditorCID, NS_EDITOR_CID);

#ifdef XP_PC
#define EDITOR_DLL "ender.dll"
#else
#ifdef XP_MAC
#define EDITOR_DLL "ENDER_DLL"
#else // XP_UNIX
#define EDITOR_DLL "libender.so"
#endif
#endif

