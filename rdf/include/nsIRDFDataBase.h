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

#ifndef nsIRDFDataBase_h__
#define nsIRDFDataBase_h__

/*
  This file contains the interface definition for an RDF database.

  RDF databases aggregate RDF data sources (see nsIRDFDataSource.h)
*/

#include "nsISupports.h"
#include "nsIRDFDataSource.h"
#include "rdf.h"


// 96343820-307c-11d2-bc15-00805f912fe7
#define NS_IRDFDATABASE_IID \
{ \
  0x96343820, \
  0x307c, \
  0x11d2, \
  { 0xb, 0xc15, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } \
}


class nsIRDFDataBase : public nsIRDFDataSource {
public:

#ifdef RDF_NOT_IMPLEMENTED
  NS_IMETHOD Initialize(nsIRDFResourceManager* r) = 0;
#endif


#ifdef RDF_NOT_IMPLEMENTED
  /*
    Add a data source for the specified URL to the database.

    Parameters:
      dataSource -- a ptr to the data source to add

    Returns:
  */
  NS_IMETHOD AddDataSource(nsIRDFDataSource* dataSource) = 0;

  NS_IMETHOD RemoveDataSource(nsIRDFDataSource* dataSource) = 0;

  NS_IMETHOD GetDataSource(RDF_String url,
                           nsIRDFDataSource **source /* out */ ) = 0;
#endif

  // XXX move these to datasource?
  NS_IMETHOD DeleteAllArcs(RDF_Resource resource) = 0;

};


#endif /* nsIRDFDataBase_h__ */
