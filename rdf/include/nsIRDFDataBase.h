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

#ifndef nsIRDFDataBase_h__
#define nsIRDFDataBase_h__

/*

  RDF databases aggregate RDF data sources (see nsIRDFDataSource.h)

  XXX This needs to be thought about real hard. It seems really wrong
  to me to have to hard code the data sources that a database knows
  about.

*/

#include "nsISupports.h"
#include "nsIRDFDataSource.h"

class nsIRDFDataSource;

// 96343820-307c-11d2-bc15-00805f912fe7
#define NS_IRDFDATABASE_IID \
{ 0x96343820, 0x307c, 0x11d2, { 0xb, 0x15, 0x00, 0x80, 0x5f, 0x91, 0x2f, 0xe7 } }

class nsIRDFDataBase : public nsIRDFDataSource {
public:
    // XXX This is really a hack. I wish that the database was just a
    // plain old data source that was smart import the data sources
    // that it needed.
    NS_IMETHOD AddDataSource(nsIRDFDataSource* source) = 0;
};


#endif /* nsIRDFDataBase_h__ */
