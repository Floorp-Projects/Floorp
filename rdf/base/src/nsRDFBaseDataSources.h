/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*

  This header file just contains prototypes for the factory methods
  for "builtin" data sources that are included in rdf.dll.

  Each of these data sources is exposed to the external world via its
  CID in ../include/nsRDFCID.h.

*/

#ifndef nsBaseDataSources_h__
#define nsBaseDataSources_h__

#include "nsError.h"
class nsIRDFDataSource;

// in nsInMemoryDataSource.cpp
NS_IMETHODIMP
NS_NewRDFInMemoryDataSource(nsISupports* aOuter, const nsIID& aIID, void** aResult);

// in nsRDFXMLDataSource.cpp
extern nsresult
NS_NewRDFXMLDataSource(nsIRDFDataSource** aResult);

#endif // nsBaseDataSources_h__


