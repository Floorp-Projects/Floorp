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


#include "nsAppCoresManager.h"
#include "nsAppShellCIDs.h"
#include "nsIAppShellService.h"
#include "nsIDOMBaseAppCore.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIURL.h"
#include "nsIWebShell.h"
#include "nsIWebShellWindow.h"
#include "nsIWidget.h"
#include "nsDOMPropsCore.h"

#include "nsIDOMNode.h"
#include "nsIDOMNamedNodeMap.h"
//#include "nsIDOMNode.h"
//#include <iostream.h>

// Stuff to implement properties dialog.
#include "nsIDOMXULDocument.h"
#include "nsIDocumentViewer.h"
#include "nsIDOMHTMLImageElement.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIDOMWindow.h"
#include "nsIXULWindowCallbacks.h"
#include "nsIDocumentObserver.h"
#include "nsINameSpaceManager.h"

class nsIScriptContext;

static NS_DEFINE_IID(kAppShellServiceCID, NS_APPSHELL_SERVICE_CID);
static NS_DEFINE_IID(kIAppShellServiceIID, NS_IAPPSHELL_SERVICE_IID);
static NS_DEFINE_IID(kIDOMBaseAppCoreIID, NS_IDOMBASEAPPCORE_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIDOMPropsCoreIID, NS_IDOMDOMPROPSCORE_IID);


/////////////////////////////////////////////////////////////////////////
// nsPropertiesDialog
/////////////////////////////////////////////////////////////////////////
// Note: This is only a temporary home for nsPropertiesDialog
// It will be moving to it's own component .h/.cpp file soon.
struct nsPropertiesDialog : public nsIXULWindowCallbacks,
                            nsIDocumentObserver {
  // Declare implementation of ISupports stuff.
  NS_DECL_ISUPPORTS

  // Declare implementations of nsIXULWindowCallbacks interface functions.
  NS_IMETHOD ConstructBeforeJavaScript(nsIWebShell *aWebShell);
  NS_IMETHOD ConstructAfterJavaScript(nsIWebShell *aWebShell) { return NS_OK; }

  // Declare implementations of nsIDocumentObserver functions.
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument)              { return NS_OK; }
  NS_IMETHOD EndUpdate(nsIDocument *aDocument)                { return NS_OK; }
  NS_IMETHOD BeginLoad(nsIDocument *aDocument)                { return NS_OK; }
  NS_IMETHOD EndLoad(nsIDocument *aDocument)                  { return NS_OK; }
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell) 
                                                              { return NS_OK; }
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell)
                                                              { return NS_OK; }
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent)         { return NS_OK; }
  NS_IMETHOD ContentStatesChanged(nsIDocument* aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2)        { return NS_OK; }
  // This one we care about; see implementation below.
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer)
                                                              { return NS_OK; }
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer)       { return NS_OK; }
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer)       { return NS_OK; }
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer)        { return NS_OK; }
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet)      { return NS_OK; }
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet)    { return NS_OK; }
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled) { return NS_OK; }
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint)                  { return NS_OK; }
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule)         { return NS_OK; }
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule)       { return NS_OK; }
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument) 
                                                              { return NS_OK; }

  // nsPropertiesDialog stuff
  nsPropertiesDialog( nsIDOMNode *aNode );
  virtual ~nsPropertiesDialog() {}
  //void SetWindow( nsIWebShellWindow *aWindow );

private:
  nsCOMPtr<nsIDOMNode> mNode;
  //nsCOMPtr<nsIWebShell> mWebShell;
  //nsCOMPtr<nsIWebShellWindow> mWindow;
  static nsIAtom *kIdAtom;
}; // nsPropertiesDialog

nsIAtom *nsPropertiesDialog::kIdAtom       = 0;

// Standard implementations of addref/release.
NS_IMPL_ADDREF( nsPropertiesDialog );
NS_IMPL_RELEASE( nsPropertiesDialog );

NS_IMETHODIMP 
nsPropertiesDialog::QueryInterface(REFNSIID aIID,void** aInstancePtr)
{
  if (aInstancePtr == NULL) {
    return NS_ERROR_NULL_POINTER;
  }

  // Always NULL result, in case of failure
  *aInstancePtr = NULL;

  if (aIID.Equals(nsIDocumentObserver::GetIID())) {
    *aInstancePtr = (void*) ((nsIDocumentObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(nsIXULWindowCallbacks::GetIID())) {
    *aInstancePtr = (void*) ((nsIXULWindowCallbacks*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  return NS_ERROR_NO_INTERFACE;
}

// ctor
nsPropertiesDialog::nsPropertiesDialog( nsIDOMNode *aNode )
        : mNode( nsDontQueryInterface<nsIDOMNode>(aNode) ) {

  // Initialize ref count.
  NS_INIT_REFCNT();

  // Initialize static atoms.
  static PRBool initialized = 0;
  if ( !initialized ) {
    kIdAtom       = NS_NewAtom("id");
    initialized = 1;
  }
}

static nsresult setAttribute( nsIWebShell *shell,
                              const char *id,
                              const nsString &name,
                              const nsString &value ) {
    nsresult rv = NS_OK;
  
    nsCOMPtr<nsIContentViewer> cv;
    rv = shell->GetContentViewer(getter_AddRefs(cv));
    if ( cv ) {
        // Up-cast.
        nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
        if ( docv ) {
            // Get the document from the doc viewer.
            nsCOMPtr<nsIDocument> doc;
            rv = docv->GetDocument(*getter_AddRefs(doc));
            if ( doc ) {
                // Up-cast.
                nsCOMPtr<nsIDOMXULDocument> xulDoc( do_QueryInterface(doc) );
                if ( xulDoc ) {
                    // Find specified element.
                    nsCOMPtr<nsIDOMElement> elem;
                    rv = xulDoc->GetElementById( id, getter_AddRefs(elem) );
                    if ( elem ) {
                        // Set the text attribute.
                        rv = elem->SetAttribute( name, value );
                        if ( rv != NS_OK ) {
                        }
                    }
                }
            }
        }
    }
    return rv;
}

// Do startup stuff from C++ side.
NS_IMETHODIMP
nsPropertiesDialog::ConstructBeforeJavaScript(nsIWebShell *aWebShell) {
  nsresult rv = NS_OK;

  // Save web shell pointer.
  //mWebShell = nsDontQueryInterface<nsIWebShell>( aWebShell );

  // Store attributes of propeties node into dialog's DOM.


  nsIDOMNamedNodeMap* map;
  nsresult result = mNode->GetAttributes(&map);
               
  if (NS_OK == result) {
    nsIDOMNode *node;
    PRUint32 attr_count;

    result = map->GetLength(&attr_count);
    if (NS_OK == result) {
      for (PRUint32 ii=0; ii<attr_count; ii++) {
        result = map->Item(ii,&node);
        if (NS_OK == result) {
          nsString name;
          result = node->GetNodeName(name);
          if (NS_OK == result) {
            nsString value;
            result = node->GetNodeValue(value);
            if (NS_OK == result) {
              if (name == "id") {
                name = "url";
              }
              setAttribute( aWebShell, "properties_node", name, value );
              //cout << "BM Props: " << "name=" << name << " value=" << value << endl;
            }
          }
          NS_RELEASE(node);
        }
      }
    }
    NS_RELEASE(map);
  }
 
    

  // Add as observer of the xul document.
  nsCOMPtr<nsIContentViewer> cv;
  rv = aWebShell->GetContentViewer(getter_AddRefs(cv));
  if ( cv ) {
    // Up-cast.
    nsCOMPtr<nsIDocumentViewer> docv(do_QueryInterface(cv));
    if ( docv ) {
      // Get the document from the doc viewer.
      nsCOMPtr<nsIDocument> doc;
      rv = docv->GetDocument(*getter_AddRefs(doc));
      if ( doc ) {
        doc->AddObserver( this );
      }
    }
  }
  
  return rv;
}

// Handle attribute changing; we only care about the element "data.execute"
// which is used to signal command execution from the UI.
NS_IMETHODIMP
nsPropertiesDialog::AttributeChanged( nsIDocument *aDocument,
                                      nsIContent*  aContent,
                                      nsIAtom*     aAttribute,
                                      PRInt32      aHint ) {
  nsresult rv = NS_OK;

  nsString id;
  aContent->GetAttribute( kNameSpaceID_None, kIdAtom, id );
  if ( id == "properties_node" ) {
    //cout << "BM properties node changed" << endl;

    nsIDOMNamedNodeMap* map;
    nsresult result = mNode->GetAttributes(&map);
    
    if (NS_OK == result) {
      nsIDOMNode *attr_node;
      PRUint32 attr_count;
      
      result = map->GetLength(&attr_count);
      if (NS_OK == result) {
        for (PRUint32 ii=0; ii<attr_count; ii++) {
          result = map->Item(ii,&attr_node);
          if (NS_OK == result) {
            nsString name;
            result = attr_node->GetNodeName(name);
            if (NS_OK == result && name != "id") {
              nsString attr;
              nsIAtom *atom = NS_NewAtom(name);
              aContent->GetAttribute( kNameSpaceID_None, atom, attr );
              //cerr << "BM Props: name=" << name << " value=" << attr << " (setting)" << endl;
              attr_node->SetNodeValue(attr);
              //attr_node->SetNodeValue("something");
              NS_RELEASE(atom);
            }
            NS_RELEASE(attr_node);
          }
        }
      }
      NS_RELEASE(map);
    }
  }
  
  return rv;
}

/////////////////////////////////////////////////////////////////////////
// nsDOMPropsCore
/////////////////////////////////////////////////////////////////////////

nsDOMPropsCore::nsDOMPropsCore() {

  printf("Created nsDOMPropsCore\n");

  IncInstanceCount();
  NS_INIT_REFCNT();
}

nsDOMPropsCore::~nsDOMPropsCore() {
  DecInstanceCount();  
}


NS_IMPL_ADDREF_INHERITED(nsDOMPropsCore, nsBaseAppCore)
NS_IMPL_RELEASE_INHERITED(nsDOMPropsCore, nsBaseAppCore)


NS_IMETHODIMP 
nsDOMPropsCore::QueryInterface(REFNSIID aIID, void** aInstancePtr) {

  if (aInstancePtr == NULL)
    return NS_ERROR_NULL_POINTER;

  *aInstancePtr = NULL;

  if (aIID.Equals(kIDOMPropsCoreIID)) {
    *aInstancePtr = (void*) ((nsIDOMDOMPropsCore*) this);
    AddRef();
    return NS_OK;
  }
 
  return nsBaseAppCore::QueryInterface(aIID, aInstancePtr);
}


NS_IMETHODIMP 
nsDOMPropsCore::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject) {
  nsresult rv = NS_OK;

  NS_PRECONDITION(aScriptObject != nsnull, "null arg");
  if (mScriptObject == nsnull) {
      nsISupports *core;
      rv = QueryInterface(kISupportsIID, (void **)&core);
      if (NS_SUCCEEDED(rv)) {
        rv = NS_NewScriptDOMPropsCore(aContext, 
                                     (nsISupports *) core,
                                     nsnull, 
                                     &mScriptObject);
        NS_RELEASE(core);
      }
  }

  *aScriptObject = mScriptObject;
  return rv;
}


NS_IMETHODIMP    
nsDOMPropsCore::Init(const nsString& aId) {

  nsresult rv;

  nsBaseAppCore::Init(aId);

  nsIDOMBaseAppCore *core;
  rv = QueryInterface(kIDOMBaseAppCoreIID, (void **)&core);
  if (NS_SUCCEEDED(rv)) {
    nsAppCoresManager* sdm = new nsAppCoresManager();
    if (sdm) {
      sdm->Add(core);
      delete sdm;
      return NS_OK;
    } else
      rv = NS_ERROR_OUT_OF_MEMORY;
    NS_RELEASE(core);
  }
  return rv;
}

NS_IMETHODIMP
nsDOMPropsCore::GetNode(nsIDOMNode** aNode) {
	nsresult	err = NS_OK;
	return err;
}

NS_IMETHODIMP
nsDOMPropsCore::SetNode(nsIDOMNode* aNode) {
	nsresult	err = NS_OK;
	return err;
}

NS_IMETHODIMP
nsDOMPropsCore::ShowProperties(const nsString& aUrl, nsIDOMWindow* aParent, nsIDOMNode* aNode) {

  nsresult           rv;
  nsString           controllerCID;
  nsIAppShellService *appShell;

  nsCOMPtr<nsIURL> urlObj;
  rv = NS_NewURL(getter_AddRefs(urlObj), aUrl);
  if (NS_FAILED(rv))
    return rv;

  rv = nsServiceManager::GetService(kAppShellServiceCID, kIAppShellServiceIID,
                                    (nsISupports**) &appShell);
  if (NS_FAILED(rv))
    return rv;

  // hardwired temporary hack.  See nsAppRunner.cpp at main()
  controllerCID = "43147b80-8a39-11d2-9938-0080c7cb1081";
  nsIWebShellWindow *newWindow;

  nsPropertiesDialog *dialog = new nsPropertiesDialog(aNode);

  nsCOMPtr<nsIWebShellWindow> parent = DOMWindowToWebShellWindow(aParent);

  rv = appShell->CreateTopLevelWindow(parent, urlObj, controllerCID, newWindow,
                                      nsnull, dialog, 450, 240);

  nsServiceManager::ReleaseService(kAppShellServiceCID, appShell);

  return rv;
}

NS_IMETHODIMP
nsDOMPropsCore::Commit() {
	nsresult rv = NS_OK;
  return rv;
}


NS_IMETHODIMP
nsDOMPropsCore::Cancel() {
	nsresult rv = NS_OK;
  return rv;
}

// horribly complicated routine to simply convert from one to the other
nsCOMPtr<nsIWebShellWindow>
nsDOMPropsCore::DOMWindowToWebShellWindow(nsIDOMWindow *DOMWindow) const {

  nsCOMPtr<nsIWebShellWindow> webWindow;

  nsCOMPtr<nsIScriptGlobalObject> globalScript(do_QueryInterface(DOMWindow));
  nsCOMPtr<nsIWebShell> webshell;
  if (globalScript)
    globalScript->GetWebShell(getter_AddRefs(webshell));
  if (webshell) {
    nsCOMPtr<nsIWebShellContainer> webshellContainer;
    webshell->GetContainer(*getter_AddRefs(webshellContainer));
    webWindow = do_QueryInterface(webshellContainer);
  }
  return webWindow;
}
