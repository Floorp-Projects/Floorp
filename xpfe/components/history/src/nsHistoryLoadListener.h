/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author(s):
 *   Alec Flett <alecf@netscape.com>
 *
 * Contributor(s):
 */


#include "nsIWebProgressListener.h"
#include "nsIBrowserHistory.h"
#include "nsCOMPtr.h"
#include "nsIObserver.h"
#include "nsIComponentManager.h"
#include "nsWeakReference.h"
#include "nsIGenericFactory.h"

class nsHistoryLoadListener : public nsIWebProgressListener,
                              public nsIObserver,
                              public nsSupportsWeakReference
{
 public:
    nsHistoryLoadListener(nsIBrowserHistory *);
    virtual ~nsHistoryLoadListener();

    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIOBSERVER
        
    static nsresult registerSelf(nsIComponentManager*, nsIFile*,
                                 const char *, const char *,
                                 const nsModuleComponentInfo* info);
    static nsresult unregisterSelf(nsIComponentManager*, nsIFile*,
                                   const char*,
                                   const nsModuleComponentInfo* info);
 protected:
    nsCOMPtr<nsIBrowserHistory> mHistory;

};

