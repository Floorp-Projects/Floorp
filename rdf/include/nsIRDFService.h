/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef nsIRDFService_h__
#define nsIRDFService_h__

/*
  This file defines the interface for the RDF singleton,
  which maintains various pieces of pieces of information global
  to all RDF data sources.

  In particular, it provides the interface for mapping rdf URL types
  to nsIRDFDataSource implementors for that type of content.
*/

#include "nsISupports.h"
#include "nsIFactory.h" /* nsCID typedef, for consistency */
#include "rdf.h"

class nsIRDFDataSource;
class nsIRDFDataBase;

// 6edf3660-32f0-11d2-9abe-00600866748f
#define NS_IRDFSERVICE_IID \
{ \
  0x6edf3660, \
  0x32f0, \
  0x11d2, \
  { 0x9a, 0xbe, 0x00, 0x60, 0x08, 0x66, 0x74, 0x8f } \
}


class nsIRDFService : public nsISupports {
public:

#ifdef RDF_NOT_IMPLEMENTED
  NS_IMETHOD RegisterHandler(RDF_String url_selector, const nsCID& clsid) = 0;

  NS_IMETHOD RemoveHandler(RDF_String url_selector, const nsCID& clsid) = 0;

  NS_IMETHOD CreateDataSource(RDF_String url,
                              nsIRDFDataSource **source /* out */) = 0;
#endif /* RDF_NOT_IMPLEMENTED */

  NS_IMETHOD CreateDatabase(const RDF_String* url_ary,
                            nsIRDFDataBase **db /* out */) = 0;

};

PR_PUBLIC_API(nsresult) NS_GetRDFService(nsIRDFService **);

#endif /* nsIRDFService_h__ */
