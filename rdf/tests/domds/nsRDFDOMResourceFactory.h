/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

#ifndef __nsRDFDOMResourceFactory_h
#define __nsRDFDOMResourceFactory_h

#include "nsIRDFService.h"

/* {84a87046-57f4-11d3-9061-00a0c900d445} */
#define NS_RDF_DOMRESOURCEFACTORY_CID \
  {0x84a87046, 0x57f4, 0x11d3, \
    { 0x90, 0x61, 0x0, 0xa0, 0xc9, 0x0, 0xd4, 0x45 }}
  

nsresult
NS_NewRDFDOMResourceFactory(nsISupports* aOuter,
                            const nsIID& iid,
                            void **result);

#endif
