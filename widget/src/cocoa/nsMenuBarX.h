/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *  Josh Aas <josh@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsMenuBarX_h_
#define nsMenuBarX_h_

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsIMutationObserver.h"
#include "nsIChangeManager.h"
#include "nsIMenuCommandDispatcher.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsWeakReference.h"
#include "nsIContent.h"

#import  <Carbon/Carbon.h>
#import  <Cocoa/Cocoa.h>

class nsIWidget;
class nsIDocument;
class nsIDOMNode;

extern "C" MenuRef _NSGetCarbonMenu(NSMenu* aMenu);

PRBool NodeIsHiddenOrCollapsed(nsIContent* inContent);

namespace MenuHelpersX
{
  nsEventStatus DispatchCommandTo(nsIWeakReference* aDocShellWeakRef,
                                  nsIContent* aTargetContent);
  NSString* CreateTruncatedCocoaLabel(nsString itemLabel);
  PRUint8 GeckoModifiersForNodeAttribute(char* modifiersAttribute);
  unsigned int MacModifiersForGeckoModifiers(PRUint8 geckoModifiers);
}


// Objective-C class used as action target for menu items
@interface NativeMenuItemTarget : NSObject
{
}
-(IBAction)menuItemHit:(id)sender;
@end

//
// Native Mac menu bar wrapper
//

class nsMenuBarX : public nsIMenuBar,
                   public nsIMenuListener,
                   public nsIMutationObserver,
                   public nsIChangeManager,
                   public nsIMenuCommandDispatcher,
                   public nsSupportsWeakReference
{
public:
    nsMenuBarX();
    virtual ~nsMenuBarX();
    
    enum {kApplicationMenuID = 1};
    
    // |NSMenuItem|s target Objective-C objects
    static NativeMenuItemTarget* sNativeEventTarget;
    
    static NSWindow* sEventTargetWindow;
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSICHANGEMANAGER
    NS_DECL_NSIMENUCOMMANDDISPATCHER

    // nsIMenuListener interface
    nsEventStatus MenuItemSelected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuSelected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuDeselected(const nsMenuEvent & aMenuEvent);
    nsEventStatus MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, 
                                void * menuNode, void * aDocShell);
    nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
    nsEventStatus CheckRebuild(PRBool & aMenuEvent);
    nsEventStatus SetRebuild(PRBool aMenuEvent);

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER

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

    void GetDocument(nsIDocShell* inDocShell, nsIDocument** outDocument) ;
    void RegisterAsDocumentObserver(nsIDocShell* inDocShell);
    
    // Make our menubar conform to Aqua UI guidelines
    void AquifyMenuBar();
    void HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode);
    OSStatus InstallCommandEventHandler();

    // command handler for some special menu items (prefs/quit/etc)
    pascal static OSStatus CommandEventHandler(EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData);
    nsEventStatus ExecuteCommand(nsIContent* inDispatchTo);
    
    // build the Application menu shared by all menu bars.
    NSMenuItem* nsMenuBarX::CreateNativeAppMenuItem(nsIMenu* inMenu, const nsAString& nodeID, SEL action,
                                                    int tag, NativeMenuItemTarget* target);
    nsresult CreateApplicationMenu(nsIMenu* inMenu);

    nsHashtable             mObserverTable;       // stores observers for content change notification

    nsCOMArray<nsIMenu>     mMenusArray;          // holds refs
    nsCOMPtr<nsIContent>    mMenuBarContent;      // menubar content node, strong ref
    nsCOMPtr<nsIContent>    mAboutItemContent;    // holds the content node for the about item that has
                                                  //   been removed from the menubar
    nsCOMPtr<nsIContent>    mPrefItemContent;     // as above, but for prefs
    nsCOMPtr<nsIContent>    mQuitItemContent;     // as above, but for quit
    nsIWidget*              mParent;              // weak ref
    PRBool                  mIsMenuBarAdded;
    PRUint32                mCurrentCommandID;    // unique command id (per menu-bar) to give to next item that asks


    nsWeakPtr               mDocShellWeakRef;     // weak ref to docshell
    nsIDocument*            mDocument;            // pointer to document

    NSMenu*                 mRootMenu;            // root menu, representing entire menu bar
 
    static EventHandlerUPP  sCommandEventHandler; // carbon event handler for commands, shared
};

#endif // nsMenuBarX_h_
