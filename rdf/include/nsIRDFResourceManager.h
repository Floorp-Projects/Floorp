/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsIRDFResourceManager_h__
#define nsIRDFResourceManager_h__

#include "nscore.h"
#include "nsISupports.h"
class nsIRDFResource;
class nsIRDFLiteral;

/**
 * An RDF resource manager. This should be a singleton object, obtained
 * from the <tt>nsServiceManager</tt>.
 */
class nsIRDFResourceManager : public nsISupports {
public:
    /**
     * Construct an RDF resource from a single-byte URI.
     */
    NS_IMETHOD GetResource(const char* uri, nsIRDFResource** resource) = 0;

    /**
     * Construct an RDF resource from a Unicode URI. This is provided
     * as a convenience method, allowing automatic, in-line C++
     * conversion from <tt>nsString</tt> objects. The <tt>uri</tt> will
     * be converted to a single-byte representation internally.
     */
    NS_IMETHOD GetUnicodeResource(const PRUnichar* uri, nsIRDFResource** resource) = 0;

    /**
     * Construct an RDF literal from a Unicode string.
     */
    NS_IMETHOD GetLiteral(const PRUnichar* value, nsIRDFLiteral** literal) = 0;
};

// {BFD05261-834C-11d2-8EAC-00805F29F370}
#define NS_IRDFRESOURCEMANAGER_IID \
{ 0xbfd05261, 0x834c, 0x11d2, { 0x8e, 0xac, 0x0, 0x80, 0x5f, 0x29, 0xf3, 0x70 } }


extern nsresult
NS_NewRDFResourceManager(nsIRDFResourceManager** result);

#endif // nsIRDFResourceManager_h__
