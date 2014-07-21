/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsRDFParser_h__
#define nsRDFParser_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIRDFXMLParser.h"
#include "nsIRDFDataSource.h"

/**
 * A helper class that is used to parse RDF/XML.
 */
class nsRDFXMLParser : public nsIRDFXMLParser {
public:
    static nsresult
    Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFXMLPARSER

protected:
    nsRDFXMLParser();
    virtual ~nsRDFXMLParser();
};

#endif // nsRDFParser_h__
