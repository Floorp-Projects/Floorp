/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsIObserverList_h__
#define nsIObserverList_h__

#include "nsISupports.h"
#include "nsIObserver.h"
#include "nsIEnumerator.h"
#include "nscore.h"


// {E777D482-E6E3-11d2-8ACD-00105A1B8860}
#define NS_IOBSERVERLIST_IID \
{ 0xe777d482, 0xe6e3, 0x11d2, { 0x8a, 0xcd, 0x0, 0x10, 0x5a, 0x1b, 0x88, 0x60 } }

class nsIObserverList : public nsISupports {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IOBSERVERLIST_IID; return iid; }
    
    NS_IMETHOD AddObserver(nsIObserver* anObserver) = 0;
    NS_IMETHOD RemoveObserver(nsIObserver* anObserver) = 0;
	NS_IMETHOD EnumerateObserverList(nsIEnumerator** anEnumerator) = 0;
   
};

extern NS_COM nsresult NS_NewObserverList(nsIObserverList** anObserverList);

#define NS_OBSERVERLIST_CONTRACTID "@mozilla.org/xpcom/observer-list;1"
#define NS_OBSERVERLIST_CLASSNAME "Observer List"

#endif /* nsIObserverList_h__ */
