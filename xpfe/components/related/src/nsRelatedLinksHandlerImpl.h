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

#ifndef relatedlinkshandler____h____
#define relatedlinkshandler____h____

#include "nsString.h"
#include "nsIRDFService.h"
#include "nsIRelatedLinksHandler.h"
#include "nsIRDFResource.h"
#include "nsCOMPtr.h"
#include "nsIRDFDataSource.h"

////////////////////////////////////////////////////////////////////////
// RelatedLinksHanlderImpl

class RelatedLinksHandlerImpl : public nsIRelatedLinksHandler,
				public nsIRDFDataSource
{
private:
	char			*mRelatedLinksURL;
	static nsString		mRLServerURL;

   // pseudo-constants
	static PRInt32		gRefCnt;
	static nsIRDFService    *gRDFService;
	static nsIRDFResource	*kNC_RelatedLinksRoot;
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kRDF_type;
	static nsIRDFResource	*kNC_RelatedLinksTopic;

	nsCOMPtr<nsIRDFDataSource> mInner;
public:

				RelatedLinksHandlerImpl();
	virtual		~RelatedLinksHandlerImpl();
	nsresult	Init();


	NS_DECL_ISUPPORTS
	NS_DECL_NSIRELATEDLINKSHANDLER
	NS_DECL_NSIRDFDATASOURCE
};

#endif // relatedlinkshandler____h____
