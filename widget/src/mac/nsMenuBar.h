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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsMenuBar_h__
#define nsMenuBar_h__

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsIDocumentObserver.h"
#include "nsIChangeManager.h"
#include "nsSupportsArray.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsWeakReference.h"

#include <MacTypes.h>
#include <UnicodeConverter.h>

extern nsWeakPtr gMacMenubar;

class nsIWidget;


namespace MenuHelpers
{
    // utility routine for getting a PresContext out of a webShell
  nsresult WebShellToPresContext ( nsIWebShell* inWebShell, nsIPresContext** outContext ) ;

    // utility routine for handling unicode->OS text conversions for setting the item
    // text in a menu.
  void SetMenuItemText ( MenuHandle macMenuHandle, short menuItem, const nsString& text,
                         const UnicodeToTextRunInfo converter ) ;
                                
    // Converts unicode to a single script run and extract the relevant font information. The
    // caller is responsible for deleting the memory allocated by this call with |nsMemory::Free()|.
    // Returns |nsnull| if an error occurred.
  char* ConvertToScriptRun ( const nsString & inStr, const UnicodeToTextRunInfo inConverter,
                               short* outFontID, SInt16* outFontSize, Style* outFontStyle ) ;
}


/**
 * Native Mac MenuBar wrapper
 */

class nsMenuBar : public nsIMenuBar,
                  public nsIMenuListener,
                  public nsIDocumentObserver,
                  public nsIChangeManager,
                  public nsSupportsWeakReference
{
public:
  
  nsMenuBar();
  virtual ~nsMenuBar();

  NS_DECL_ISUPPORTS
  NS_DECL_NSICHANGEMANAGER
  
  // nsIMenuListener interface
  nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
  nsEventStatus MenuConstruct( const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                  void * menuNode, void * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
  nsEventStatus CheckRebuild(PRBool & aMenuEvent);
  nsEventStatus SetRebuild(PRBool & aMenuEvent);

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
  
  NS_IMETHOD Create(nsIWidget * aParent);

  // nsIMenuBar Methods
  NS_IMETHOD GetParent(nsIWidget *&aParent);
  NS_IMETHOD SetParent(nsIWidget * aParent);
  NS_IMETHOD AddMenu(nsIMenu * aMenu);
  NS_IMETHOD GetMenuCount(PRUint32 &aCount);
  NS_IMETHOD GetMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD InsertMenuAt(const PRUint32 aCount, nsIMenu *& aMenu);
  NS_IMETHOD RemoveMenu(const PRUint32 aCount);
  NS_IMETHOD RemoveAll();
  NS_IMETHOD GetNativeData(void*& aData);
  NS_IMETHOD Paint();
  NS_IMETHOD SetNativeData(void* aData);
    
protected:

  void GetDocument ( nsIWebShell* inWebShell, nsIDocument** outDocument ) ;
  void RegisterAsDocumentObserver ( nsIWebShell* inWebShell ) ;

  nsHashtable           mObserverTable;   // stores observers for content change notification
  
  PRUint32              mNumMenus;
  nsSupportsArray       mMenusArray;        // holds refs
  nsCOMPtr<nsIDOMNode>  mDOMNode;
  nsIWidget *           mParent;            // weak ref

  PRBool       mIsMenuBarAdded;
  
  nsWeakPtr   mWebShellWeakRef;    // weak ref to webshell

  // Mac Specific
  Handle      mMacMBarHandle;
  Handle      mOriginalMacMBarHandle;
  UnicodeToTextRunInfo mUnicodeTextRunConverter;
  
};

#endif // nsMenuBar_h__

