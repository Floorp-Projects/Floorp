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

#ifndef nsIRDFRegistry_h__
#define nsIRDFRegistry_h__

#include "nsISupports.h"
class nsString;
class nsIRDFDataSource;

// {E638D762-8687-11d2-B530-000000000000}
#define NS_IRDFREGISTRY_IID \
{ 0xe638d762, 0x8687, 0x11d2, { 0xb5, 0x30, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0 } }

/**
 * The RDF registry manages the RDF data sources.
 */
class nsIRDFRegistry : public nsISupports {
public:
    /**
     * Registers the specified prefix as being handled by the
     * data source. Note that the data source will <em>not</em>
     * be reference counted.
     */
    NS_IMETHOD Register(const nsString& prefix, nsIRDFDataSource* dataSource) = 0;

    /**
     * Unregisters the data source from the registry.
     */
    NS_IMETHOD Unregister(const nsIRDFDataSource* dataSource) = 0;

    /**
     * Looks for a data source that will handle the specified URI.
     */
    NS_IMETHOD Find(const nsString& uri, nsIRDFDataSource*& result) = 0;
};


#endif nsIRDFRegistry_h__



