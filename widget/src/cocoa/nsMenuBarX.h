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
#include "nsIMutationObserver.h"
#include "nsCOMArray.h"
#include "nsHashtable.h"
#include "nsTHashtable.h"
#include "nsWeakReference.h"
#include "nsIContent.h"

#import  <Carbon/Carbon.h>
#import  <Cocoa/Cocoa.h>

class nsIWidget;
class nsIDocument;
class nsIDOMNode;
class nsChangeObserver;

extern "C" MenuRef _NSGetCarbonMenu(NSMenu* aMenu);

PRBool NodeIsHiddenOrCollapsed(nsIContent* inContent);

namespace MenuHelpersX
{
  nsEventStatus DispatchCommandTo(nsIContent* aTargetContent);
  NSString* CreateTruncatedCocoaLabel(const nsString& itemLabel);
  PRUint8 GeckoModifiersForNodeAttribute(const nsString& modifiersAttribute);
  unsigned int MacModifiersForGeckoModifiers(PRUint8 geckoModifiers);
}


// Objective-C class used as action target for menu items
@interface NativeMenuItemTarget : NSObject
{
}
-(IBAction)menuItemHit:(id)sender;
@end


struct CocoaKeyEquivContainer {
  CocoaKeyEquivContainer(const unsigned int modifiers, const NSString* string)
  {
    mModifiers = modifiers;
    mString = [string retain];
  }
  
  ~CocoaKeyEquivContainer()
  {
    [mString release];
  }
  
  CocoaKeyEquivContainer(const CocoaKeyEquivContainer& other)
  {
    mModifiers = other.mModifiers;
    mString = [other.mString retain];
  }
  
  CocoaKeyEquivContainer& operator=(CocoaKeyEquivContainer& other)
  {
    mModifiers = other.mModifiers;
    if (mString)
      [mString release];
    mString = [other.mString retain];
    return *this;
  }
  
  unsigned int mModifiers;
  NSString* mString;
};


struct CocoaKeyEquivKey : public PLDHashEntryHdr {
  typedef const CocoaKeyEquivContainer& KeyType;
  typedef const CocoaKeyEquivContainer* KeyTypePointer;
  
  CocoaKeyEquivKey(KeyTypePointer aObj) : mObj(*aObj) { }
  CocoaKeyEquivKey(const CocoaKeyEquivKey& other) : mObj(other.mObj) { }
  ~CocoaKeyEquivKey() { }
  
  KeyType GetKey() const { return mObj; }
  
  PRBool KeyEquals(KeyTypePointer aKey) const {
    return aKey->mModifiers == mObj.mModifiers &&
    [aKey->mString isEqualToString:mObj.mString];
  }
  
  static KeyTypePointer KeyToPointer(KeyType aKey) { return &aKey; }
  static PLDHashNumber HashKey(KeyTypePointer aKey) {
    return [aKey->mString hash] ^ aKey->mModifiers;
  }
  enum { ALLOW_MEMMOVE = PR_FALSE };
private:
  const CocoaKeyEquivContainer mObj;
};



//
// Native menu bar wrapper
//

class nsMenuBarX : public nsIMenuBar,
                   public nsIMutationObserver,
                   public nsSupportsWeakReference
{
public:
    nsMenuBarX();
    virtual ~nsMenuBarX();

    // |NSMenuItem|s target Objective-C objects
    static NativeMenuItemTarget* sNativeEventTarget;
    
    static NSWindow* sEventTargetWindow;
    
    NS_DECL_ISUPPORTS

    // nsIMutationObserver
    NS_DECL_NSIMUTATIONOBSERVER

    // nsIMenuBar Methods
    NS_IMETHOD Create(nsIWidget * aParent);
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
    NS_IMETHOD MenuConstruct(const nsMenuEvent & aMenuEvent, nsIWidget * aParentWindow, void * aMenuNode);
    PRBool ContainsKeyEquiv(unsigned int modifiers, NSString* string);

    PRUint32 RegisterForCommand(nsIMenuItem* aItem);
    void UnregisterCommand(PRUint32 aCommandID);

    void RegisterForContentChanges(nsIContent* aContent, nsChangeObserver* aMenuObject);
    void UnregisterForContentChanges(nsIContent* aContent);
    nsChangeObserver* LookupContentChangeObserver(nsIContent* aContent);

    void RegisterKeyEquivalent(unsigned int modifiers, NSString* string);
    void UnregisterKeyEquivalent(unsigned int modifiers, NSString* string);
protected:
    // Make our menubar conform to Aqua UI guidelines
    void AquifyMenuBar();
    void HideItem(nsIDOMDocument* inDoc, const nsAString & inID, nsIContent** outHiddenNode);
    OSStatus InstallCommandEventHandler();

    // command handler for some special menu items (prefs/quit/etc)
    pascal static OSStatus CommandEventHandler(EventHandlerCallRef inHandlerChain, 
                                               EventRef inEvent, void* userData);
    nsEventStatus ExecuteCommand(nsIContent* inDispatchTo);
    
    // build the Application menu shared by all menu bars.
    NSMenuItem* CreateNativeAppMenuItem(nsIMenu* inMenu, const nsAString& nodeID, SEL action,
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

    nsIDocument*            mDocument;            // pointer to document

    NSMenu*                 mRootMenu;            // root menu, representing entire menu bar

    nsTHashtable<CocoaKeyEquivKey> mKeyEquivTable;

    static EventHandlerUPP  sCommandEventHandler; // carbon event handler for commands, shared
};

#endif // nsMenuBarX_h_
