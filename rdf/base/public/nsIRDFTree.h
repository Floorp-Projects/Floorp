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

#ifndef nsIRDFTree_h__
#define nsIRDFTree_h__

#include "nscore.h"
#include "nsISupports.h"
#include "prtypes.h"
#include "rdf.h" // for error codes

/*
  An interface for presenting a tree view of an rdf datasource (or
  database).

  Any datasource can also support nsIRDFTree. The interface is purely
  syntactic sugar for traversing simple tree where the child relation
  corresponds to the property type nc:child (nc = http://home.netscape.com/NC-rdf#).
  Each leaf is assumed to have a certain predefined set of properties
  such as creationDate, size, lastModificationDate, lastVisitDate, etc.

  This interface is substantially less general than nsIRDFDataSource,
  but is adequate for bookmarks, the file system, history and a few
  other very commonly used data sources.

 */

// {7D7EEBD1-AA41-11d2-80B7-006097B76B8E}
#define NS_IRDFTREE_IID \
{ 0x7d7eebd1, 0xaa41, 0x11d2, { 0x80, 0xb7, 0x0, 0x60, 0x97, 0xb7, 0x6b, 0x8e } };

class nsIRDFTree : public nsISupports {
public:

  NS_IMETHOD ListChildren (nsIRDFResource* folder, nsVoidArray** result);
 // XXX should define something called nsResourceArray and use that

  NS_IMETHOD IsFolder (nsIRDFResource* node, PRBool* result);

  NS_IMETHOD GetProperty (nsIRDFResource* node, nsIRDFResource* property, 
			  nsIRDFNode** result);

}

#endif
