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

#ifndef nsMenuBar_h__
#define nsMenuBar_h__

#include "nsIMenuBar.h"
#include "nsIMenuListener.h"
#include "nsIDocumentObserver.h"
#include "nsIChangeManager.h"
#include "nsPresContext.h"
#include "nsSupportsArray.h"
#include "nsVoidArray.h"
#include "nsHashtable.h"
#include "nsWeakReference.h"

#include <MacTypes.h>
#include <UnicodeConverter.h>

extern nsWeakPtr gMacMenubar;

class nsIWidget;
class nsIDocument;


namespace MenuHelpers
{
    // utility routine for getting a PresContext out of a webShell
  nsresult WebShellToPresContext ( nsIWebShell* inWebShell, nsPresContext** outContext ) ;

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
                                 void* aDOMNode, void * aWebShell);
  nsEventStatus MenuDestruct(const nsMenuEvent & aMenuEvent);
  nsEventStatus CheckRebuild(PRBool & aMenuEvent);
  nsEventStatus SetRebuild(PRBool aMenuEvent);

  // nsIDocumentObserver
  NS_DECL_NSIDOCUMENTOBSERVER
  
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
  nsCOMPtr<nsIContent>  mMenuBarContent;    // menubar content node, strong ref
  nsIWidget *           mParent;            // weak ref

  PRBool       mIsMenuBarAdded;
  
  nsWeakPtr   mWebShellWeakRef;    // weak ref to webshell
  nsIDocument *         mDocument;          // pointer to doc

  // Mac Specific
  Handle      mMacMBarHandle;
  Handle      mOriginalMacMBarHandle;
  UnicodeToTextRunInfo mUnicodeTextRunConverter;
  
};

#endif // nsMenuBar_h__

