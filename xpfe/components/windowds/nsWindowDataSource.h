/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 200
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Dan Matejka <danm@netscape.com>
 *   Robert Churchill <rjc@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsIRDFDataSource.h"
#include "nsIWindowMediatorListener.h"
#include "nsIWindowDataSource.h"
#include "nsIObserver.h"

#include "nsIRDFService.h"
#include "nsIRDFContainer.h"
#include "nsHashtable.h"
#include "nsCycleCollectionParticipant.h"

// {C744CA3D-840B-460a-8D70-7CE63C51C958}
#define NS_WINDOWDATASOURCE_CID \
{ 0xc744ca3d, 0x840b, 0x460a, \
 { 0x8d, 0x70, 0x7c, 0xe6, 0x3c, 0x51, 0xc9, 0x58 } }


class nsWindowDataSource : public nsIRDFDataSource,
                           public nsIObserver,
                           public nsIWindowMediatorListener,
                           public nsIWindowDataSource
{
 public:
    nsWindowDataSource() { }
    virtual ~nsWindowDataSource();

    nsresult Init();
    
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsWindowDataSource,
                                             nsIRDFDataSource)
    NS_DECL_NSIOBSERVER
    NS_DECL_NSIWINDOWMEDIATORLISTENER
    NS_DECL_NSIWINDOWDATASOURCE
    NS_DECL_NSIRDFDATASOURCE

 private:

    // mapping of window -> RDF resource
    nsSupportsHashtable mWindowResources;

    static PRUint32 windowCount;
    static PRUint32 gRefCnt;
    
    nsCOMPtr<nsIRDFDataSource> mInner;
    nsCOMPtr<nsIRDFContainer> mContainer;

    static nsIRDFResource* kNC_Name;
    static nsIRDFResource* kNC_KeyIndex;
    static nsIRDFResource* kNC_WindowRoot;
    static nsIRDFService* gRDFService;
};

             
