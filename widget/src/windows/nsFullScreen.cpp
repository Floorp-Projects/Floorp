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

#include "nsFullScreen.h"
#include "nsIDOMElement.h"
#include "nsIDOMDocument.h"
#include "nsIContent.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIPresContext.h"
#include "nsIView.h"
#include "nsIWidget.h"
#include "nsISupportsArray.h"
#include "nsEnumeratorUtils.h"

/* Implementation file */
NS_IMPL_ISUPPORTS1(nsFullScreen, nsIFullScreen)

nsFullScreen::nsFullScreen()
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */
}

nsFullScreen::~nsFullScreen()
{
  PRUint32 count;
  if (mOSChromeItems) {
    mOSChromeItems->Count(&count);
    for (PRUint32 i = 0; i < count; ++i) {
      nsISupports* supp = mOSChromeItems->ElementAt(i);
      NS_RELEASE(supp);
    }
    mOSChromeItems->Clear();
  }
}

/* void hideAllOSChrome (); */
NS_IMETHODIMP nsFullScreen::HideAllOSChrome()
{
  HideAllOSChrome(PR_TRUE);
  return NS_OK;
}

/* void showAllOSChrome (); */
NS_IMETHODIMP nsFullScreen::ShowAllOSChrome()
{
  HideAllOSChrome(PR_FALSE);
  return NS_OK;
}

void 
nsFullScreen::HideAllOSChrome(PRBool aVisibility)
{
  nsCOMPtr<nsISimpleEnumerator> items;
  GetChromeItems(getter_AddRefs(items));

  PRBool hasMore;
  items->HasMoreElements(&hasMore);
  while (hasMore) {
    nsCOMPtr<nsISupports> supp;
    items->GetNext(getter_AddRefs(supp));

    nsCOMPtr<nsIOSChromeItem> item(do_QueryInterface(supp));
    item->SetHidden(aVisibility);

    items->HasMoreElements(&hasMore);
  }
}

/* nsISimpleEnumerator getChromeItems (); */
NS_IMETHODIMP 
nsFullScreen::GetChromeItems(nsISimpleEnumerator **aResult)
{
  nsresult rv;

  if (!mOSChromeItems) {
    rv = NS_NewISupportsArray(getter_AddRefs(mOSChromeItems));
    if (NS_FAILED(rv)) return rv;

    nsOSChromeItem* item = new nsOSChromeItem("Shell_TrayWnd");
    if (!item) return NS_ERROR_OUT_OF_MEMORY;

    mOSChromeItems->AppendElement(item);
  }

  return NS_NewArrayEnumerator(aResult, mOSChromeItems);
}

///////////////////////////////////////////////////////////////////////////////
// nsIOSChromeItem

NS_IMPL_ISUPPORTS1(nsOSChromeItem, nsIOSChromeItem)

nsOSChromeItem::nsOSChromeItem(char* aName) : mIsHidden(PR_FALSE)
{
  NS_INIT_ISUPPORTS();
  /* member initializers and constructor code */

  mName.Assign(aName);

  HWND itemHandle = ::FindWindow(mName.get(), NULL);
  ::GetWindowRect(itemHandle, &mItemRect);
}

nsOSChromeItem::~nsOSChromeItem()
{
  /* destructor code */
}

NS_IMETHODIMP 
nsOSChromeItem::GetName(char** aName)
{
  if (!aName) return NS_ERROR_NULL_POINTER;
  
  *aName = nsCRT::strdup(mName.get());
  if (!(*aName)) return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP 
nsOSChromeItem::GetHidden(PRBool *aHidden)
{
  *aHidden = mIsHidden;
  return NS_OK;
}

NS_IMETHODIMP 
nsOSChromeItem::SetHidden(PRBool aHidden)
{
  if (mIsHidden == aHidden) 
    return NS_OK;

  HWND itemHandle = ::FindWindow(mName.get(), NULL);

  ::ShowWindow(itemHandle, aHidden ? SW_HIDE : SW_SHOWNA);

  mIsHidden = aHidden;

  return NS_OK;
}

/* readonly attribute short x; */
NS_IMETHODIMP 
nsOSChromeItem::GetX(PRInt32 *aX)
{
  *aX = mItemRect.left;
  return NS_OK;
}

/* readonly attribute short y; */
NS_IMETHODIMP 
nsOSChromeItem::GetY(PRInt32 *aY)
{
  *aY = mItemRect.top;
  return NS_OK;
}

/* readonly attribute short width; */
NS_IMETHODIMP 
nsOSChromeItem::GetWidth(PRInt32 *aWidth)
{
  *aWidth = mItemRect.right - mItemRect.left;
  return NS_OK;
}

/* readonly attribute short height; */
NS_IMETHODIMP 
nsOSChromeItem::GetHeight(PRInt32 *aHeight)
{
  *aHeight = mItemRect.bottom - mItemRect.top;
  return NS_OK;
}

