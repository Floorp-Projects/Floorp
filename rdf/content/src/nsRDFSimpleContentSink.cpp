/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#include "nsRDFContentSink.h"
static NS_DEFINE_IID(kIRDFContentSinkIID, NS_IRDFCONTENTSINK_IID);

////////////////////////////////////////////////////////////////////////

class nsRDFSimpleContentSink : public nsRDFContentSink {
public:
    nsRDFSimpleContentSink(void) {};
    virtual ~nsRDFSimpleContentSink(void) {};
};


////////////////////////////////////////////////////////////////////////

nsresult
NS_NewRDFSimpleContentSink(nsIRDFContentSink** aResult,
                           nsIURL* aURL,
                           nsINameSpaceManager* aNameSpaceManager)
{
    NS_PRECONDITION(aResult, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsRDFSimpleContentSink* it;
    NS_NEWXPCOM(it, nsRDFSimpleContentSink);
    if (! it)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = it->Init(aURL, aNameSpaceManager);
    if (NS_FAILED(rv)) {
        delete it;
        return rv;
    }
    return it->QueryInterface(kIRDFContentSinkIID, (void **)aResult);
}


