/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Contributor(s):  Radha Kulkarni radha@netscape.com
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsISHEntry.h"
#include "nsISHTransaction.h"
#include "nsISHistory.h"

#include "nsSHEntry.h"
#include "nsSHistory.h"
#include "nsSHTransaction.h"

// Factory Constructors

NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHEntry)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHTransaction)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSHistory)


///////////////////////////////////////////////////////////////////////////////
// Module implementation for the history library

static nsModuleComponentInfo gSHistoryModuleInfo[] = 
{
   { "nsSHEntry", NS_SHENTRY_CID,
      NS_SHENTRY_CONTRACTID, nsSHEntryConstructor },
   { "nsSHTransaction", NS_SHTRANSACTION_CID,
      NS_SHTRANSACTION_CONTRACTID, nsSHTransactionConstructor },
   { "nsSHistory", NS_SHISTORY_CID,
      NS_SHISTORY_CONTRACTID, nsSHistoryConstructor }
};

NS_IMPL_NSGETMODULE("Session History Module", gSHistoryModuleInfo)
