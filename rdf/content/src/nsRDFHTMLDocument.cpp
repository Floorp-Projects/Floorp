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
#include "nsINameSpaceManager.h"
#include "nsISupportsArray.h"
#include "nsRDFDocument.h"
#include "rdfutil.h"

////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIRDFResourceIID, NS_IRDFRESOURCE_IID);
static NS_DEFINE_IID(kIRDFLiteralIID,  NS_IRDFLITERAL_IID);

////////////////////////////////////////////////////////////////////////

class RDFHTMLDocumentImpl : public nsRDFDocument {
public:
    RDFHTMLDocumentImpl();
    virtual ~RDFHTMLDocumentImpl();

protected:
    nsresult AddTreeChild(nsIRDFContent* parent,
                          const nsString& tag,
                          nsIRDFResource* property,
                          nsIRDFResource* value);

    nsresult AddLeafChild(nsIRDFContent* parent,
                          const nsString& tag,
                          nsIRDFResource* property,
                          nsIRDFLiteral* value);

    virtual nsresult AddChild(nsIRDFContent* parent,
                              nsIRDFResource* property,
                              nsIRDFNode* value);
};

////////////////////////////////////////////////////////////////////////

static nsIAtom* kIdAtom;

RDFHTMLDocumentImpl::RDFHTMLDocumentImpl(void)
{
  if (nsnull == kIdAtom) {
    kIdAtom = NS_NewAtom("ID");
  }
  else {
    NS_ADDREF(kIdAtom);
  }
}

RDFHTMLDocumentImpl::~RDFHTMLDocumentImpl(void)
{
  nsrefcnt refcnt;
  NS_RELEASE2(kIdAtom, refcnt);
}

nsresult
RDFHTMLDocumentImpl::AddTreeChild(nsIRDFContent* parent,
                                  const nsString& tag,
                                  nsIRDFResource* property,
                                  nsIRDFResource* value)
{
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

    nsresult rv;
    const char* p;
    nsAutoString s;
    nsIRDFContent* child = nsnull;

    // PR_TRUE indicates that we want the child to dynamically
    // generate its own kids.
    if (NS_FAILED(rv = NewChild(tag, value, child, PR_TRUE)))
        goto done;

    if (NS_FAILED(rv = value->GetValue(&p)))
        goto done;

    s = p;

    if (NS_FAILED(rv = child->SetAttribute(kNameSpaceID_HTML, kIdAtom, s, PR_FALSE)))
        goto done;

    rv = parent->AppendChildTo(child, PR_TRUE);

done:
    NS_IF_RELEASE(child);
    return rv;
}


nsresult
RDFHTMLDocumentImpl::AddLeafChild(nsIRDFContent* parent,
                                  const nsString& tag,
                                  nsIRDFResource* property,
                                  nsIRDFLiteral* value)
{
    // Otherwise, it's not a tree property. So we'll just create a
    // new element for the property, and a simple text node for
    // its value; e.g.,
    //
    // <parent>
    //   <property>value</property>
    //   ...
    // </parent>

    nsresult rv;
    nsIRDFContent* child = nsnull;

    if (NS_FAILED(rv = NewChild(tag, property, child, PR_FALSE)))
        goto done;

    if (NS_FAILED(rv = AttachTextNode(child, value)))
        goto done;

    rv = parent->AppendChildTo(child, PR_TRUE);

done:
    NS_IF_RELEASE(child);
    return rv;
}

nsresult
RDFHTMLDocumentImpl::AddChild(nsIRDFContent* parent,
                              nsIRDFResource* property,
                              nsIRDFNode* value)
{
    nsresult rv;

    // The tag we'll use for the new child will be the string value of
    // the property.
    const char* s;
    if (NS_FAILED(rv = property->GetValue(&s)))
        return rv;

    nsAutoString tag = s;

    nsIRDFResource* valueResource;
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFResourceIID, (void**) &valueResource))) {
        if (IsTreeProperty(property) || rdf_IsContainer(mResourceMgr, mDB, valueResource)) {
            rv = AddTreeChild(parent, tag, property, valueResource);
            NS_RELEASE(valueResource);
            return rv;
        }
        NS_RELEASE(valueResource);
    }

    nsIRDFLiteral* valueLiteral;
    if (NS_SUCCEEDED(rv = value->QueryInterface(kIRDFLiteralIID, (void**) &valueLiteral))) {
        rv = AddLeafChild(parent, tag, property, valueLiteral);
        NS_RELEASE(valueLiteral);
    }
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
