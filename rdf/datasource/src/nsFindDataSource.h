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

#ifndef nsFindDataSource_h__
#define nsFindDataSource_h__


////////////////////////////////////////////////////////////////////////
// NS_DECL_IRDFRESOURCE, NS_IMPL_IRDFRESOURCE
//
//   Convenience macros for implementing the RDF resource interface.
//
//   XXX It might make sense to move these macros to nsIRDFResource.h?

#define NS_DECL_IRDFRESOURCE \
    NS_IMETHOD EqualsNode(nsIRDFNode* node, PRBool* result) const;\
    NS_IMETHOD GetValue(const char* *uri) const;\
    NS_IMETHOD EqualsResource(const nsIRDFResource* resource, PRBool* result) const;\
    NS_IMETHOD EqualsString(const char* uri, PRBool* result) const;


#define NS_IMPL_IRDFRESOURCE(__class) \
NS_IMETHODIMP \
__class::EqualsNode(nsIRDFNode* node, PRBool* result) const {\
    nsresult rv;\
    nsIRDFResource* resource;\
    if (NS_SUCCEEDED(node->QueryInterface(kIRDFResourceIID, (void**) &resource))) {\
        rv = EqualsResource(resource, result);\
        NS_RELEASE(resource);\
    }\
    else {\
        *result = PR_FALSE;\
        rv = NS_OK;\
    }\
    return rv;\
}\
NS_IMETHODIMP \
__class::GetValue(const char* *uri) const{\
    if (!uri)\
        return NS_ERROR_NULL_POINTER;\
    *uri = mURI;\
    return NS_OK;\
}\
NS_IMETHODIMP \
__class::EqualsResource(const nsIRDFResource* resource, PRBool* result) const {\
    if (!resource || !result)  return NS_ERROR_NULL_POINTER;\
    *result = (resource == (nsIRDFResource*) this);\
    return NS_OK;\
}\
NS_IMETHODIMP \
__class::EqualsString(const char* uri, PRBool* result) const {\
    if (!uri || !result)  return NS_ERROR_NULL_POINTER;\
    *result = (PL_strcmp(uri, mURI) == 0);\
    return NS_OK;\
}



typedef	struct	_findTokenStruct
{
	char			*token;
	char			*value;
} findTokenStruct, *findTokenPtr;



class FindDataSource : public nsIRDFFindDataSource
{
private:
	char			*mURI;
	nsVoidArray		*mObservers;

	static PRInt32		gRefCnt;

    // pseudo-constants
	static nsIRDFResource	*kNC_Child;
	static nsIRDFResource	*kNC_Name;
	static nsIRDFResource	*kNC_URL;
	static nsIRDFResource	*kNC_FindObject;
	static nsIRDFResource	*kRDF_InstanceOf;
	static nsIRDFResource	*kRDF_type;

	NS_METHOD	getFindResults(nsIRDFResource *source,
				nsVoidArray **array /* out */);
	NS_METHOD	getFindName(nsIRDFResource *source,
				nsVoidArray **array /* out */);
	NS_METHOD	parseResourceIntoFindTokens(nsIRDFResource *u,
				findTokenPtr tokens);
	NS_METHOD	parseFindURL(nsIRDFResource *u,
				nsVoidArray *array);

public:

	NS_DECL_ISUPPORTS

			FindDataSource(void);
	virtual		~FindDataSource(void);

	// nsIRDFDataSource methods

	NS_IMETHOD	Init(const char *uri);
	NS_IMETHOD	GetURI(const char **uri) const;
	NS_IMETHOD	GetSource(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsIRDFResource **source /* out */);
	NS_IMETHOD	GetSources(nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				nsIRDFAssertionCursor **sources /* out */);
	NS_IMETHOD	GetTarget(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsIRDFNode **target /* out */);
	NS_IMETHOD	GetTargets(nsIRDFResource *source,
				nsIRDFResource *property,
				PRBool tv,
				nsIRDFAssertionCursor **targets /* out */);
	NS_IMETHOD	Assert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv);
	NS_IMETHOD	Unassert(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target);
	NS_IMETHOD	HasAssertion(nsIRDFResource *source,
				nsIRDFResource *property,
				nsIRDFNode *target,
				PRBool tv,
				PRBool *hasAssertion /* out */);
	NS_IMETHOD	ArcLabelsIn(nsIRDFNode *node,
				nsIRDFArcsInCursor **labels /* out */);
	NS_IMETHOD	ArcLabelsOut(nsIRDFResource *source,
				nsIRDFArcsOutCursor **labels /* out */);
	NS_IMETHOD	GetAllResources(nsIRDFResourceCursor** aCursor);
	NS_IMETHOD	AddObserver(nsIRDFObserver *n);
	NS_IMETHOD	RemoveObserver(nsIRDFObserver *n);
	NS_IMETHOD	Flush();
	NS_IMETHOD	GetAllCommands(nsIRDFResource* source,
				nsIEnumerator/*<nsIRDFResource>*/** commands);
	NS_IMETHOD	IsCommandEnabled(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments);
	NS_IMETHOD	DoCommand(nsISupportsArray/*<nsIRDFResource>*/* aSources,
				nsIRDFResource*   aCommand,
				nsISupportsArray/*<nsIRDFResource>*/* aArguments);
};



class FindCursor : public nsIRDFAssertionCursor, public nsIRDFArcsOutCursor
{
private:
	nsIRDFNode	*mValue;
	nsIRDFResource	*mSource;
	nsIRDFResource	*mProperty;
	nsIRDFNode	*mTarget;
	int		mCount;
	nsVoidArray	*mArray;
	PRBool		mArcsOut;

public:
			FindCursor(nsIRDFResource *source, nsIRDFResource *property, PRBool isArcsOut, nsVoidArray *array);
	virtual		~FindCursor(void);

	NS_DECL_ISUPPORTS

	NS_IMETHOD	Advance(void);
	NS_IMETHOD	GetValue(nsIRDFNode **aValue);
	NS_IMETHOD	GetDataSource(nsIRDFDataSource **aDataSource);
	NS_IMETHOD	GetSubject(nsIRDFResource **aResource);
	NS_IMETHOD	GetPredicate(nsIRDFResource **aPredicate);
	NS_IMETHOD	GetObject(nsIRDFNode **aObject);
	NS_IMETHOD	GetTruthValue(PRBool *aTruthValue);
};



#endif // nsFindDataSource_h__
