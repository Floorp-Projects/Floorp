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

#ifndef nsWebShellWindow_h__
#define nsWebShellWindow_h__

#include "nsISupports.h"
#include "nsIWebShellWindow.h"
#include "nsGUIEvent.h"
#include "nsIWebShell.h"  
#include "nsIDocumentLoaderObserver.h"
#include "nsIDocumentObserver.h"
#include "nsVoidArray.h"
#include "nsIMenu.h"

// can't use forward class decl's because of template bugs on Solaris 
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"

#include "nsCOMPtr.h"

/* Forward declarations.... */
struct PLEvent;

class nsIURL;
class nsIAppShell;
class nsIContent;
class nsIDocument;
class nsIDOMCharacterData;
class nsIDOMElement;
class nsIDOMHTMLImageElement;
class nsIDOMHTMLInputElement;
class nsIStreamObserver;
class nsIWidget;
class nsIWidgetController;
class nsIXULWindowCallbacks;

class nsWebShellWindow : public nsIWebShellWindow,
                         public nsIWebShellContainer,
                         public nsIDocumentLoaderObserver,
                         public nsIDocumentObserver
{
public:
  nsWebShellWindow();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  // nsIWebShellContainer interface...
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

  NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
                         PRBool aVisible,
                         nsIWebShell *&aNewWebShell);

  NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
                                  nsIWebShell*& aResult);

  NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell);


  // nsIWebShellWindow methods...
  NS_IMETHOD Show(PRBool aShow);
  NS_IMETHOD ShowModal();
  NS_IMETHOD Close();
  NS_IMETHOD GetWebShell(nsIWebShell *& aWebShell);
  NS_IMETHOD GetWidget(nsIWidget *& aWidget);

  // nsWebShellWindow methods...
  nsresult Initialize(nsIWebShellWindow * aParent, nsIAppShell* aShell, nsIURL* aUrl,
                      nsString& aControllerIID, nsIStreamObserver* anObserver,
                      nsIXULWindowCallbacks *aCallbacks,
                      PRInt32 aInitialWidth, PRInt32 aInitialHeight);
  nsIWidget* GetWidget(void) { return mWindow; }

  // nsIDocumentLoaderObserver
  NS_IMETHOD OnStartDocumentLoad(nsIURL* aURL, const char* aCommand);
  NS_IMETHOD OnEndDocumentLoad(nsIURL *aUrl, PRInt32 aStatus);
  NS_IMETHOD OnStartURLLoad(nsIURL* aURL, const char* aContentType, nsIContentViewer* aViewer);
  NS_IMETHOD OnProgressURLLoad(nsIURL* aURL, PRUint32 aProgress, 
                               PRUint32 aProgressMax);
  NS_IMETHOD OnStatusURLLoad(nsIURL* aURL, nsString& aMsg);
  NS_IMETHOD OnEndURLLoad(nsIURL* aURL, PRInt32 aStatus);

  NS_IMETHOD OnConnectionsComplete();

  // nsIDocumentObserver
  NS_IMETHOD BeginUpdate(nsIDocument *aDocument);
  NS_IMETHOD EndUpdate(nsIDocument *aDocument);
  NS_IMETHOD BeginLoad(nsIDocument *aDocument);
  NS_IMETHOD EndLoad(nsIDocument *aDocument);
  NS_IMETHOD BeginReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD EndReflow(nsIDocument *aDocument, nsIPresShell* aShell);
  NS_IMETHOD ContentChanged(nsIDocument *aDocument,
                            nsIContent* aContent,
                            nsISupports* aSubContent);
  NS_IMETHOD ContentStateChanged(nsIDocument *aDocument,
                                 nsIContent* aContent);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              nsIAtom*     aAttribute,
                              PRInt32      aHint);
  NS_IMETHOD ContentAppended(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             PRInt32     aNewIndexInContainer);
  NS_IMETHOD ContentInserted(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentReplaced(nsIDocument *aDocument,
                             nsIContent* aContainer,
                             nsIContent* aOldChild,
                             nsIContent* aNewChild,
                             PRInt32 aIndexInContainer);
  NS_IMETHOD ContentRemoved(nsIDocument *aDocument,
                            nsIContent* aContainer,
                            nsIContent* aChild,
                            PRInt32 aIndexInContainer);
  NS_IMETHOD StyleSheetAdded(nsIDocument *aDocument,
                             nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetRemoved(nsIDocument *aDocument,
                               nsIStyleSheet* aStyleSheet);
  NS_IMETHOD StyleSheetDisabledStateChanged(nsIDocument *aDocument,
                                            nsIStyleSheet* aStyleSheet,
                                            PRBool aDisabled);
  NS_IMETHOD StyleRuleChanged(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule,
                              PRInt32 aHint);
  NS_IMETHOD StyleRuleAdded(nsIDocument *aDocument,
                            nsIStyleSheet* aStyleSheet,
                            nsIStyleRule* aStyleRule);
  NS_IMETHOD StyleRuleRemoved(nsIDocument *aDocument,
                              nsIStyleSheet* aStyleSheet,
                              nsIStyleRule* aStyleRule);
  NS_IMETHOD DocumentWillBeDestroyed(nsIDocument *aDocument);

protected:
  void ExecuteJavaScriptString(nsString& aJavaScript);

  PRInt32 GetDocHeight(nsIDocument * aDoc);
 
  void LoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow);
  nsCOMPtr<nsIDOMNode>     FindNamedParentFromDoc(nsIDOMDocument * aDomDoc, const nsString &aName);
  nsCOMPtr<nsIDOMNode>     FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount);
  nsCOMPtr<nsIDOMDocument> GetNamedDOMDoc(const nsString & aWebShellName);
  nsCOMPtr<nsIDOMNode>     GetParentNodeFromDOMDoc(nsIDOMDocument * aDOMDoc);
  NS_IMETHOD               CreateMenu(nsIMenuBar * aMenuBar, nsIDOMNode * aMenuNode, nsString & aMenuName);
  void LoadSubMenu(nsIMenu * pParentMenu, nsIDOMElement * menuElement,nsIDOMNode * menuNode);
  NS_IMETHOD LoadMenuItem(nsIMenu * pParentMenu, nsIDOMElement * menuitemElement, nsIDOMNode * menuitemNode);

  nsCOMPtr<nsIDOMNode>     GetDOMNodeFromWebShell(nsIWebShell *aShell);
  void                     ExecuteStartupCode();
  

  virtual ~nsWebShellWindow();

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);
  NS_IMETHODIMP           ShowModalInternal();
  void                    ExitModalLoop() { mContinueModalLoop = PR_FALSE; }

  nsIWidget*              mWindow;
  nsIWebShell*            mWebShell;
  nsIWidgetController*    mController;
  nsIXULWindowCallbacks*  mCallbacks;
  PRBool                  mContinueModalLoop;

  nsVoidArray mMenuDelegates;

private:

  static void * HandleModalDialogEvent(PLEvent *aEvent);
  static void DestroyModalDialogEvent(PLEvent *aEvent);
};


#endif /* nsWebShellWindow_h__ */
