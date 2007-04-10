/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
	const char		*token;
	nsString	    value;
} findTokenStruct, *findTokenPtr;


class LocalSearchDataSource : public nsIRDFDataSource
{
private:
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

    // matching functions, based on type
	PRBool doMatch(nsIRDFLiteral  *literal,
                   const nsAString& matchMethod,
                   const nsString& matchText);
    PRBool matchNode(nsIRDFNode *aNode,
                     const nsAString& matchMethod,
                     const nsString& matchText);
    
	PRBool doDateMatch(nsIRDFDate *literal,
                       const nsAString& matchMethod,
                       const nsAString& matchText);
	PRBool doIntMatch (nsIRDFInt  *literal,
                       const nsAString& matchMethod,
                       const nsString& matchText);

    PRBool dateMatches(nsIRDFDate *literal,
                       const nsAString& method,
                       const PRInt64& matchDate);
    
    NS_METHOD   parseDate(const nsAString& aDate, PRInt64* aResult);
    
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
