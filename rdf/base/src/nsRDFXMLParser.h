/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "MPL"); you may not use this file except in
 * compliance with the MPL.  You may obtain a copy of the MPL at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the MPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the MPL
 * for the specific language governing rights and limitations under the
 * MPL.
 *
 * The Initial Developer of this code under the MPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributor(s): 
 *   Chris Waterson <waterson@netscape.com>
 */

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
    static NS_IMETHODIMP
    Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIRDFXMLPARSER

protected:
    nsRDFXMLParser();
    virtual ~nsRDFXMLParser();
};

#endif // nsRDFParser_h__
