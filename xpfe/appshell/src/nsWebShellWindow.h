/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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
#include "nsITimer.h"

#include "nsIPrompt.h"
// can't use forward class decl's because of template bugs on Solaris 
#include "nsIDOMDocument.h"
#include "nsIDOMNode.h"

#include "nsCOMPtr.h"
#include "nsXULWindow.h"

/* Forward declarations.... */
struct PLEvent;

class nsIURI;
class nsIAppShell;
class nsIContent;
class nsIDocument;
class nsIDOMCharacterData;
class nsIDOMElement;
class nsIDOMWindowInternal;
class nsIDOMHTMLImageElement;
class nsIDOMHTMLInputElement;
class nsIStreamObserver;
class nsIWidget;
class nsVoidArray;

class nsWebShellWindow : public nsXULWindow,
                         public nsIWebShellWindow,
                         public nsIWebShellContainer,
                         public nsIDocumentLoaderObserver,
                         public nsIDocumentObserver

{
public:
  nsWebShellWindow();

  // nsISupports interface...
  NS_DECL_ISUPPORTS

  NS_IMETHOD GetContentShellById(const nsString& anID, nsIWebShell** aResult);
  NS_IMETHOD LockUntilChromeLoad() { mLockedUntilChromeLoad = PR_TRUE; return NS_OK; }
  NS_IMETHOD GetLockedState(PRBool& aResult) { aResult = mLockedUntilChromeLoad; return NS_OK; }

  NS_IMETHOD ShouldLoadDefaultPage(PRBool *aYes)
               { *aYes = mLoadDefaultPage; return NS_OK; }

  // nsIWebShellWindow methods...
  NS_IMETHOD Show(PRBool aShow);
  NS_IMETHOD ShowModal();
  NS_IMETHOD ShowModally(PRBool aPrepare);
  NS_IMETHOD Close();
  NS_IMETHOD GetWebShell(nsIWebShell *& aWebShell);
  NS_IMETHOD GetContentWebShell(nsIWebShell **aResult);
  NS_IMETHOD GetWidget(nsIWidget *& aWidget);
  NS_IMETHOD GetDOMWindow(nsIDOMWindowInternal** aDOMWindow);
  NS_IMETHOD ConvertWebShellToDOMWindow(nsIWebShell* aShell, nsIDOMWindowInternal** aDOMWindow);
  NS_IMETHOD GetPrompter(nsIPrompt* *result);
  // nsWebShellWindow methods...
  nsresult Initialize(nsIXULWindow * aParent, nsIAppShell* aShell, nsIURI* aUrl,
                      PRBool aCreatedVisible, PRBool aLoadDefaultPage,
                      PRBool aContentScrollbars, PRUint32 aZlevel,
                      PRInt32 aInitialWidth, PRInt32 aInitialHeight,
                      nsWidgetInitData& widgetInitData);
  nsIWidget* GetWidget(void) { return mWindow; }

  void DoContextMenu(
	  nsMenuEvent * aMenuEvent,
	  nsIDOMNode  * aMenuNode, 
	  nsIWidget   * aParentWindow,
	  PRInt32       aX,
	  PRInt32       aY,
    const nsString& aPopupAlignment,
    const nsString& aAnchorAlignment);
  
  // nsIDocumentLoaderObserver
   NS_DECL_NSIDOCUMENTLOADEROBSERVER
  
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
  NS_IMETHOD ContentStatesChanged(nsIDocument *aDocument,
                                  nsIContent* aContent1,
                                  nsIContent* aContent2);
  NS_IMETHOD AttributeChanged(nsIDocument *aDocument,
                              nsIContent*  aContent,
                              PRInt32      aNameSpaceID,
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

  // nsINetSupport
  // nsIBaseWindow
  NS_IMETHOD Destroy();

protected:
  
  nsCOMPtr<nsIDOMNode>     FindNamedDOMNode(const nsString &aName, nsIDOMNode * aParent, PRInt32 & aCount, PRInt32 aEndCount);
  nsCOMPtr<nsIDOMDocument> GetNamedDOMDoc(const nsString & aWebShellName);
  void DynamicLoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow);
#if 0
  void LoadMenus(nsIDOMDocument * aDOMDoc, nsIWidget * aParentWindow);
  NS_IMETHOD               CreateMenu(nsIMenuBar * aMenuBar, nsIDOMNode * aMenuNode, nsString & aMenuName);
  void LoadSubMenu(nsIMenu * pParentMenu, nsIDOMElement * menuElement,nsIDOMNode * menuNode);
  NS_IMETHOD LoadMenuItem(nsIMenu * pParentMenu, nsIDOMElement * menuitemElement, nsIDOMNode * menuitemNode);
#endif

  nsCOMPtr<nsIDOMNode>     GetDOMNodeFromWebShell(nsIWebShell *aShell);
  void                     ExecuteStartupCode();
  void                     StoreBoundsToXUL(PRBool aPosition, PRBool aSize, PRBool aSizeMode);
  void                     KillPersistentSize();
  void                     LoadContentAreas();
  PRBool                   ExecuteCloseHandler();

  virtual ~nsWebShellWindow();

  static nsEventStatus PR_CALLBACK HandleEvent(nsGUIEvent *aEvent);

  nsresult                NotifyObservers( const nsString &aTopic, const nsString &someData );

  nsIWebShell*            mWebShell;
  PRBool                  mLockedUntilChromeLoad;
  PRBool                  mLoadDefaultPage;
  PRBool                  mKillScrollbarsAfterLoad;

  nsVoidArray mMenuDelegates;

  nsIDOMNode * contextMenuTest;

  nsCOMPtr<nsITimer>      mSPTimer;
  PRBool                  mSPTimerSize, mSPTimerPosition;
  PRLock *                mSPTimerLock;
  nsCOMPtr<nsIPrompt>     mPrompter;

  void        SetPersistenceTimer(PRBool aSize, PRBool aPosition);
  static void FirePersistenceTimer(nsITimer *aTimer, void *aClosure);

private:

  static void * HandleModalDialogEvent(PLEvent *aEvent);
  static void DestroyModalDialogEvent(PLEvent *aEvent);
};


#endif /* nsWebShellWindow_h__ */
