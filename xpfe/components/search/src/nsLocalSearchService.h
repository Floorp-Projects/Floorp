/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef localsearchdb___h_____
#define localsearchdb___h_____

#include "nsISupportsArray.h"
#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"
#include "nsString.h"
#include "nsIRDFNode.h"
#include "nsIRDFService.h"
#include "nsISearchService.h"

typedef	struct	_findTokenStruct
{
	char			*token;
	nsString	    value;
} findTokenStruct, *findTokenPtr;


class LocalSearchDataSource : public nsIRDFDataSource
{
private:
	nsCOMPtr<nsISupportsArray> mObservers;

	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_FindObject;
	static nsIRDFResource	*kNC_pulse;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

protected:

	NS_METHOD	getFindResults(nsIRDFResource *source, nsISimpleEnumerator** aResult);
	NS_METHOD	getFindName(nsIRDFResource *source, nsIRDFLiteral** aResult);
	NS_METHOD	parseResourceIntoFindTokens(nsIRDFResource *u, findTokenPtr tokens);
	NS_METHOD	doMatch(nsIRDFLiteral *literal, const nsString &matchMethod, const nsString &matchText);
	NS_METHOD	parseFindURL(nsIRDFResource *u, nsISupportsArray *array);

public:
	LocalSearchDataSource(void);
	virtual		~LocalSearchDataSource(void);
	nsresult	Init();

	NS_DECL_ISUPPORTS
	NS_DECL_NSILOCALSEARCHSERVICE
    NS_DECL_NSIRDFDATASOURCE
};

#endif // localsearchdb___h_____
