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
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsObserver_h___
#define nsObserver_h___

#include "nsIObserver.h"
#include "nsAgg.h"

// {DB242E03-E4D9-11d2-9DDE-000064657374}
#define NS_OBSERVER_CID \
    { 0xdb242e03, 0xe4d9, 0x11d2, { 0x9d, 0xde, 0x0, 0x0, 0x64, 0x65, 0x73, 0x74 } }

class nsObserver : public nsIObserver {
public:

    NS_DEFINE_STATIC_CID_ACCESSOR( NS_OBSERVER_CID )
 
    NS_DECL_NSIOBSERVER
 
    nsObserver(nsISupports* outer);
    virtual ~nsObserver(void);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    NS_DECL_AGGREGATED

private:

};

extern NS_COM nsresult NS_NewObserver(nsIObserver** anObserver, nsISupports* outer = NULL);

#endif /* nsObserver_h___ */
