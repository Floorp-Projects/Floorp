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

/*

  RDF composite data source interface. A composite data source
  aggregates individual RDF data sources.

 */

#ifndef nsIRDFCompositeDataSource_h__
#define nsIRDFCompositeDataSource_h__

#if 1 //defined(USE_XPIDL_INTERFACES)
#include "nsRDFInterfaces.h"
#else

#include "nsISupports.h"
#include "nsIRDFDataSource.h"

class nsIRDFDataSource;

// 96343820-307c-11d2-bc15-00805f912fe7
#define NS_IRDFCOMPOSITEDATASOURCE_IID \
{ 0x96343820, 0x307c, 0x11d2, { 0xb, 0x15, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } }

/**
 * An <tt>nsIRDFCompositeDataSource</tt> composes individual data sources, providing
 * the illusion of a single, coherent RDF graph.
 */
class nsIRDFCompositeDataSource : public nsIRDFDataSource {
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFCOMPOSITEDATASOURCE_IID; return iid; }

    /**
     * Add a datasource the the database.
     */
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source) = 0;

    /**
     * Remove a datasource from the database
     */
    NS_IMETHOD RemoveDataSource(nsIRDFDataSource* source) = 0;
};

#endif

extern nsresult
NS_NewRDFCompositeDataSource(nsIRDFCompositeDataSource** result);

#endif /* nsIRDFCompositeDataSource_h__ */
