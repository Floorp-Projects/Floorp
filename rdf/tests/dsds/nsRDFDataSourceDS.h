/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
