
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

#include "nsEditorAppCore.h"
#include "nsIBrowserWindow.h"
#include "nsIWebShell.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsAppCores.h"
#include "nsAppCoresCIDs.h"
#include "nsAppCoresManager.h"

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

// Drag & Drop, Clipboard Support
static NS_DEFINE_IID(kIClipboardIID,    NS_ICLIPBOARD_IID);
static NS_DEFINE_CID(kCClipboardCID,    NS_CLIPBOARD_CID);
#endif


/* Define Class IDs */
static NS_DEFINE_IID(kAppShellServiceCID,        NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kEditorAppCoreCID,         NS_EDITORAPPCORE_CID);

/* Define Interface IDs */
static NS_DEFINE_IID(kIAppShellServiceIID,       NS_IAPPSHELL_SERVICE_IID);

static NS_DEFINE_IID(kISupportsIID,              NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIEditorAppCoreIID,        NS_IDOMEDITORAPPCORE_IID);

static NS_DEFINE_IID(kIDOMDocumentIID,           nsIDOMDocument::GetIID());
static NS_DEFINE_IID(kIDocumentIID,              nsIDocument::GetIID());


static NS_DEFINE_IID(kINetSupportIID,            NS_INETSUPPORT_IID);
static NS_DEFINE_IID(kIStreamObserverIID,        NS_ISTREAMOBSERVER_IID);

static NS_DEFINE_IID(kIWebShellWindowIID,        NS_IWEBSHELL_WINDOW_IID);
static NS_DEFINE_IID(kIDocumentViewerIID,        NS_IDOCUMENT_VIEWER_IID);

static NS_DEFINE_IID(kIHTMLEditorIID, NS_IHTMLEDITOR_IID);
static NS_DEFINE_CID(kHTMLEditorCID, NS_HTMLEDITOR_CID);

#define APP_DEBUG 0 

/////////////////////////////////////////////////////////////////////////
// nsEditorAppCore
/////////////////////////////////////////////////////////////////////////

nsEditorAppCore::nsEditorAppCore()
{
  if (APP_DEBUG) printf("Created nsEditorAppCore\n");

  mScriptObject         = nsnull;
  mToolbarWindow        = nsnull;
  mToolbarScriptContext = nsnull;
  mContentWindow        = nsnull;
  mContentScriptContext = nsnull;
  mWebShellWin          = nsnull;
  mWebShell             = nsnull;
  mCurrentNode          = nsnull;
  mDomDoc               = nsnull;
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


NS_IMPL_ADDREF(nsEditorAppCore)
NS_IMPL_RELEASE(nsEditorAppCore)


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

  // XXX This is lame and needs to be changed
	nsAppCoresManager* sdm = new nsAppCoresManager();
	sdm->Add((nsIDOMBaseAppCore *)(nsBaseAppCore *)this);
	delete sdm;

	return NS_OK;
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

void
nsEditorAppCore::DoEditorMode(nsIWebShell *aWebShell)
{
  if (nsnull != aWebShell) {
    nsIContentViewer* mCViewer;
    aWebShell->GetContentViewer(&mCViewer);
    if (nsnull != mCViewer) {
      nsIDocumentViewer* mDViewer;
      if (NS_OK == mCViewer->QueryInterface(kIDocumentViewerIID, (void**) &mDViewer)) 
      {
	      nsIDocument* mDoc;
	      mDViewer->GetDocument(mDoc);
	      if (nsnull != mDoc) {
	        nsIDOMDocument* mDOMDoc;
	        if (NS_OK == mDoc->QueryInterface(kIDOMDocumentIID, (void**) &mDOMDoc)) 
          {
            nsIPresShell* presShell = GetPresShellFor(aWebShell);
            if( presShell )
            {
              nsIHTMLEditor *editor = nsnull;
              nsresult result = nsComponentManager::CreateInstance(kHTMLEditorCID, nsnull,
                                                    kIHTMLEditorIID, (void **)&editor);
              if(!editor)
                result = NS_ERROR_OUT_OF_MEMORY;
              if (NS_SUCCEEDED(result))
              {
                result = editor->Init(mDOMDoc, presShell);
                if (NS_SUCCEEDED(result) && editor)
                {
                  // The EditorAppCore "owns" the editor
                  mEditor = editor;
                }
#ifdef DEBUG
                else
                {
                  printf("Failed to init editor\n");
                }
#endif
              }
            }
	          NS_RELEASE(mDOMDoc);
            NS_IF_RELEASE(presShell);
	        }
	        NS_RELEASE(mDoc);
	      }
	      NS_RELEASE(mDViewer);
      }
      NS_RELEASE(mCViewer);
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
}


NS_IMETHODIMP    
nsEditorAppCore::SetAttribute(const nsString& aAttr, const nsString& aValue)
{

	return NS_ERROR_NOT_IMPLEMENTED;
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
  mToolbarWindow = aWin;
  NS_ADDREF(aWin);
  mToolbarScriptContext = GetScriptContext(aWin);

	return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SetContentWindow(nsIDOMWindow* aWin)
{
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

    if (APP_DEBUG) printf("Attaching to WebShellWindow[%s]\n", str.ToNewCString());

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
  if (mEditor) {
    mEditor->Undo(1);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Redo()
{  
  if (mEditor) {
    mEditor->Redo(1);
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Cut()
{  
  if (mEditor) {
    mEditor->Cut();
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Copy()
{  
  if (mEditor) {
    mEditor->Copy();
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::Paste()
{  
  if (mEditor) {
    mEditor->Paste();
  }

  return NS_OK;
}

NS_IMETHODIMP    
nsEditorAppCore::SelectAll()
{  
  if (mEditor) {
    mEditor->SelectAll();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsEditorAppCore::InsertText(const nsString& textToInsert)
{
	 nsresult	err = NS_OK;
	 
  if (mEditor)
  {
   	err = mEditor->InsertText(textToInsert);
  }

  return err;
}


NS_IMETHODIMP
nsEditorAppCore::GetContentsAsText(nsString& aContentsAsText)
{
	 nsresult	err = NS_OK;
	 
  if (mEditor)
  {
   	err = mEditor->OutputText(aContentsAsText);
  }

  return err;
}

NS_IMETHODIMP
nsEditorAppCore::GetContentsAsHTML(nsString& aContentsAsHTML)
{
	 nsresult	err = NS_OK;
	 
  if (mEditor)
  {
   	err = mEditor->OutputHTML(aContentsAsHTML);
  }

  return err;
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
    nsITransferable * trans;
    clipboard->GetData(&trans);
    if (nsnull != trans) {
      if (NS_OK == trans->IsLargeDataSet()) {
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
    if (APP_DEBUG) printf("Executing [%s]\n", aScript.ToNewCString());
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

