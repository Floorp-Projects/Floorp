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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 */

/*

  Shared structure for sort state.

*/

#ifndef nsRDFSort_h__
#define nsRDFSort_h__

#include "nsIRDFResource.h"
#include "nsIRDFDataSource.h"
#include "nsIContent.h"
#include "nsString.h"
#include "prtypes.h"

class nsRDFSortState
{
public:
	// state match strings
	nsAutoString				sortResource, sortResource2;

	// state variables
	nsCOMPtr<nsIRDFDataSource>		mCache;
	nsCOMPtr<nsIRDFResource>		sortProperty, sortProperty2;
	nsCOMPtr<nsIRDFResource>		sortPropertyColl, sortPropertyColl2;
	nsCOMPtr<nsIRDFResource>		sortPropertySort, sortPropertySort2;

	nsCOMPtr<nsIContent>			lastContainer;
	PRBool					lastWasFirst, lastWasLast;
};


#endif // nsRDFSort_h__

