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

#include "nsIRDFContent.h"
#include "nsIRDFCursor.h"
#include "nsIRDFDataBase.h"
#include "nsIRDFNode.h"
#include "nsIRDFResourceManager.h"
#include "nsIServiceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDocument.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

class RDFTreeDocumentImpl : public nsRDFDocument {
public:
    RDFTreeDocumentImpl();
    virtual ~RDFTreeDocumentImpl();

protected:
    virtual nsresult CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result);
};

////////////////////////////////////////////////////////////////////////

RDFTreeDocumentImpl::RDFTreeDocumentImpl(void)
{
}

RDFTreeDocumentImpl::~RDFTreeDocumentImpl(void)
{
}

nsresult
RDFTreeDocumentImpl::CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result)
{
    static const char* kTRTag = "TR";
    static const char* kTDTag = "TD";

    nsresult rv;
    nsIRDFContent* child = nsnull;

    if (IsTreeProperty(property) || rdf_IsContainer(mResourceMgr, mDB, value)) {
        // If it's a tree property, then create a child element whose
        // value is the value of the property. We'll also attach an "ID="
        // attribute to the new child; e.g.,
        //
        // <parent>
        //   <tr id="value">
        //      <!-- recursively generated -->
        //   </tr>
        //   ...
        // </parent>

        nsAutoString s;

        // PR_TRUE indicates that we want the child to dynamically
        // generate its own kids.
        if (NS_FAILED(rv = NewChild(kTRTag, value, child, PR_TRUE)))
            goto done;

        if (NS_FAILED(rv = value->GetStringValue(s)))
            goto done;

        if (NS_FAILED(rv = child->SetAttribute("ID", s, PR_FALSE)))
            goto done;

        result = child;
        NS_ADDREF(result);
    }
    else {
        // Otherwise, it's not a tree property. So we'll just create a
        // new element for the property, and a simple text node for
        // its value; e.g.,
        //
        // <parent>
        //   <td>value</td>
        //   ...
        // </parent>

        if (NS_FAILED(rv = NewChild(kTDTag, property, child, PR_FALSE)))
            goto done;

        if (NS_FAILED(rv = AttachTextNode(child, value)))
            goto done;

        result = child;
        NS_ADDREF(result);
    }

done:
    NS_IF_RELEASE(child);
    return rv;
}


////////////////////////////////////////////////////////////////////////

nsresult NS_NewRDFTreeDocument(nsIRDFDocument** result)
{
    nsIRDFDocument* doc = new RDFTreeDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);
    *result = doc;
    return NS_OK;
}



