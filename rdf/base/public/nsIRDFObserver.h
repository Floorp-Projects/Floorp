/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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

  The RDF data source observer interface. Data source observers are
  notified when the contents of the graph change.

 */

#ifndef nsIRDFObserver_h__
#define nsIRDFObserver_h__

#if 1 //defined(USE_XPIDL_INTERFACES)
#include "nsRDFInterfaces.h"
#else

#include "nsISupports.h"

class nsIRDFDataSource;
class nsIRDFResource;
class nsIRDFNode;

/**
 * An observer on an nsIRDFDataSource.
 */

// 3cc75360-484a-11d2-bc16-00805f912fe7
#define NS_IRDFOBSERVER_IID \
{ 0x3cc75360, 0x484a, 0x11d2, { 0xbc, 0x16, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } }

class nsIRDFObserver : public nsISupports
{
public:
    static const nsIID& GetIID() { static nsIID iid = NS_IRDFOBSERVER_IID; return iid; }

    /**
     * Called whenever a new assertion is made in the data source.
     */
    NS_IMETHOD OnAssert(nsIRDFResource* subject,
                        nsIRDFResource* predicate,
                        nsIRDFNode* object) = 0;

    /**
     * Called whenever an assertion is removed from the data source.
     */
    NS_IMETHOD OnUnassert(nsIRDFResource* subject,
                          nsIRDFResource* predicate,
                          nsIRDFNode* object) = 0;
};

#endif

#endif /* nsIRDFObserver_h__ */
