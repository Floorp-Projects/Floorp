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

#ifndef	nsIRDFSearch_h__
#define	nsIRDFSearch_h__

#include "nscore.h"
#include "nsISupports.h"
#include "nsVoidArray.h"
#include "nsIRDFNode.h"
#include "nsIStreamListener.h"



#define NS_IRDFSEARCHDATASOURCECALLBACK_IID \
{ 0x88774583, 0x1edd, 0x11d3, { 0x98, 0x20, 0xbf, 0x1b, 0xe7, 0x7e, 0x61, 0xc4 } }

class nsIRDFSearchDataSourceCallback : public nsIStreamListener
{
public:
};



#define NS_IRDFSEARCHDATAOURCE_IID \
{ 0x6bd1d803, 0x1c67, 0x11d3, { 0x98, 0x20, 0xed, 0x1b, 0x35, 0x7e, 0xb3, 0xc4 } }

class nsIRDFSearchDataSource : public nsIRDFDataSource
{
public:
};



#define NS_IRDFSEARCH_IID \
{ 0x6bd1d805, 0x1c67, 0x11d3, { 0x98, 0x20, 0xed, 0x1b, 0x35, 0x7e, 0xb3, 0xc4 } }

class nsIRDFSearch : public nsIRDFResource
{
public:
	NS_IMETHOD	GetSearchEngineList(nsIRDFResource *source, nsVoidArray **array) const = 0;
	NS_IMETHOD	GetName(nsVoidArray **array) const = 0;
	NS_IMETHOD	GetURL(nsVoidArray **array) const = 0;
};



#endif // nsIRDFSearch_h__
