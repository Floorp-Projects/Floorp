/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenuBarX_h__
#define nsMenuBarX_h__

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsIDocumentObserver.h"
#include "nsIChangeManager.h"
#include "nsIMenuCommandDispatcher.h"
#include "nsIPresContext.h"
#include "nsSupportsArray.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsWeakReference.h"
#include "nsIContent.h"

#include <MacTypes.h>
#include <UnicodeConverter.h>
#include <Menus.h>
#include <CarbonEvents.h>

extern nsWeakPtr gMacMenubarX;

class nsIWidget;
class nsIDocument;
class nsIDOMNode;

namespace MenuHelpersX
{
    // utility routine for getting a PresContext out of a webShell
  nsresult WebShellToPresContext ( nsIWebShell* inWebShell, nsIPresContext** outContext ) ;

}


/**
 * Native Mac MenuBar wrapper
 */

class nsMenuBarX :  public nsIMenuBar,
                    public nsIMenuListener,
                    public nsIDocumentObserver,
                    public nsIChangeManager,
                    public nsIMenuCommandDispatcher,
                    public nsSupportsWeakReference
{
public:
    nsMenuBarX();
    virtual ~nsMenuBarX();

    enum { kAppleMenuID = 1 } ;
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANGEMANAGER
    NS_DECL_NSIMENUCOMMANDDISPATCHER

    // nsIMenuListener interface
    nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                void * menuNode, void * aWebShell);
    nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
    nsEventStatus CheckRebuild(PRBool & aMenuEvent);
    nsEventStatus SetRebuild(PRBool aMenuEvent);

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
                                PRInt32      aModType, 
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
    
      // Make our menubar conform to Aqua UI guidelines
    void AquifyMenuBar ( ) ;
    void HideItem ( nsIDOMDocument* inDoc, nsAReadableString & inID, nsIContent** outHiddenNode ) ;
    OSStatus InstallCommandEventHandler ( ) ;

      // command handler for some special menu items (prefs/quit/etc)
    pascal static OSStatus CommandEventHandler ( EventHandlerCallRef inHandlerChain, 
                                                  EventRef inEvent, void* userData ) ;
    nsEventStatus ExecuteCommand ( nsIContent* inDispatchTo ) ;
    
      // build the Apple menu shared by all menu bars.
    nsresult CreateAppleMenu ( nsIMenu* inMenu ) ;

    nsHashtable             mObserverTable;     // stores observers for content change notification
    nsHashtable             mCommandMapTable;   // maps CommandIDs to content nodes for CarbonEvent item selection
    PRUint32                mCurrentCommandID;  // unique command id (per menu-bar) to give to next item that asks

    PRUint32                mNumMenus;
    nsSupportsArray         mMenusArray;        // holds refs
    nsCOMPtr<nsIContent>    mMenuBarContent;    // menubar content node, strong ref
    nsCOMPtr<nsIContent>    mPrefItemContent;   // on X, holds the content node for the prefs item that has
                                                // been removed from the menubar
    nsCOMPtr<nsIContent>    mQuitItemContent;   // as above, but for quit
    nsIWidget*              mParent;            // weak ref

    PRBool                  mIsMenuBarAdded;

    nsWeakPtr               mWebShellWeakRef;   // weak ref to webshell
    nsIDocument*            mDocument;          // pointer to document

    MenuRef                 mRootMenu;          // root menu, representing entire menu bar.
    
    static MenuRef          sAppleMenu;         // AppleMenu shared by all menubars
 
    static EventHandlerUPP  sCommandEventHandler;   // carbon event handler for commands, shared
};

#endif // nsMenuBarX_h__
