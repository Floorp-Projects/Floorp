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

#ifndef __nsRDFDataSourceDS_h
#define __nsRDFDataSourceDS_h


/* {aa1b3f18-1aad-11d3-84bf-006008948010} */
#define NS_RDFDATASOURCEDATASOURCE_CID \
  {0xaa1b3f18, 0x1aad, 0x11d3, \
    {0x84, 0xbf, 0x00, 0x60, 0x08, 0x94, 0x80, 0x10}}

nsresult
NS_NewRDFDataSourceDataSource(nsISupports* aOuter,
                              const nsIID& iid, void **result);

#endif
