/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  This header file just contains prototypes for the factory methods
  for "builtin" data sources that are included in rdf.dll.

  Each of these data sources is exposed to the external world via its
  CID in ../include/nsRDFCID.h.

 */

#ifndef nsBuiltinDataSources_h__
#define nsBuiltinDataSources_h__

#include "nsError.h"

class nsIRDFDataSource;

// in nsFileSystemDataSource.cpp
nsresult NS_NewRDFFileSystemDataSource(nsIRDFDataSource** result);

#endif // nsBuiltinDataSources_h__

