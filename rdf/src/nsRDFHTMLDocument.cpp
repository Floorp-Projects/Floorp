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

/*

  This builds an HTML-like model, complete with text nodes, that can
  be displayed in a vanilla HTML content viewer. You can apply CSS2
  styles to the text, etc.

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

class RDFHTMLDocumentImpl : public nsRDFDocument {
public:
    RDFHTMLDocumentImpl();
    virtual ~RDFHTMLDocumentImpl();

protected:
    virtual nsresult CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result);
};

////////////////////////////////////////////////////////////////////////

RDFHTMLDocumentImpl::RDFHTMLDocumentImpl(void)
{
}

RDFHTMLDocumentImpl::~RDFHTMLDocumentImpl(void)
{
}

nsresult
RDFHTMLDocumentImpl::CreateChild(nsIRDFNode* property,
                                 nsIRDFNode* value,
                                 nsIRDFContent*& result)
{
    nsresult rv;
    nsIRDFContent* child = nsnull;

    // The tag we'll use for the new child will be the string value of
    // the property.
    nsAutoString tag;
    if (NS_FAILED(rv = property->GetStringValue(tag)))
        goto done;

    if (IsTreeProperty(property) || rdf_IsContainer(mResourceMgr, mDB, value)) {
        // If it's a tree property, then create a child element whose
        // value is the value of the property. We'll also attach an "ID="
        // attribute to the new child; e.g.,
        //
        // <parent>
        //   <property id="value">
        //      <!-- recursively generated -->
        //   </property>
        //   ...
        // </parent>

        nsAutoString s;

        // PR_TRUE indicates that we want the child to dynamically
        // generate its own kids.
        if (NS_FAILED(rv = NewChild(tag, value, child, PR_TRUE)))
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
        //   <property>value</property>
        //   ...
        // </parent>

        if (NS_FAILED(rv = NewChild(tag, property, child, PR_FALSE)))
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

nsresult NS_NewRDFHTMLDocument(nsIRDFDocument** result)
{
    nsIRDFDocument* doc = new RDFHTMLDocumentImpl();
    if (! doc)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(doc);
    *result = doc;
    return NS_OK;
}
