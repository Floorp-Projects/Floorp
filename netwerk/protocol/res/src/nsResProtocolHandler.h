/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 *   Darin Fisher <darin@netscape.com>
 */

#ifndef nsResProtocolHandler_h___
#define nsResProtocolHandler_h___

#include "nsIResProtocolHandler.h"
#include "nsHashtable.h"
#include "nsISupportsArray.h"
#include "nsIIOService.h"
#include "nsWeakReference.h"

class nsResProtocolHandler : public nsIResProtocolHandler, public nsSupportsWeakReference
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIPROTOCOLHANDLER
    NS_DECL_NSIRESPROTOCOLHANDLER

    nsResProtocolHandler();
    virtual ~nsResProtocolHandler();

    nsresult Init();

private:
    nsresult SetSpecialDir(const char* rootName, const char* specialDir);

    nsSupportsHashtable    mSubstitutions;
    nsCOMPtr<nsIIOService> mIOService;
};

#endif /* nsResProtocolHandler_h___ */
