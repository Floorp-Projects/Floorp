/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A datasource that wraps an nsIRegstry object

*/

#include "nsRegistryDataSource.h"
#include "nsIFileSpec.h"
#include "nsIModule.h"
#include "nsIComponentManager.h"
#include "nsIGenericFactory.h"
#include "nsRDFCID.h"
#include "nsXPIDLString.h"
#include "plstr.h"
#include "rdf.h"
#include "nsCRT.h"

static NS_DEFINE_CID(kRDFServiceCID,       NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRegistryCID,         NS_REGISTRY_CID);

#define NS_REGISTRY_NAMESPACE_URI "urn:mozilla-registry:"
static const char kKeyPrefix[] = NS_REGISTRY_NAMESPACE_URI "key:";
static const char kValuePrefix[] = NS_REGISTRY_NAMESPACE_URI "value:";

//------------------------------------------------------------------------

nsrefcnt nsRegistryDataSource::gRefCnt = 0;
nsIRDFService* nsRegistryDataSource::gRDF;

nsIRDFResource* nsRegistryDataSource::kKeyRoot;
nsIRDFResource* nsRegistryDataSource::kSubkeys;
nsIRDFLiteral*  nsRegistryDataSource::kBinaryLiteral;


//------------------------------------------------------------------------
//
// Constructors, destructors.
//

nsRegistryDataSource::nsRegistryDataSource()
{
    NS_INIT_REFCNT();
}


nsRegistryDataSource::~nsRegistryDataSource()
{
    if  (--gRefCnt == 0) {
        if (gRDF) nsServiceManager::ReleaseService(kRDFServiceCID, gRDF);

        NS_IF_RELEASE(kKeyRoot);
        NS_IF_RELEASE(kSubkeys);
        NS_IF_RELEASE(kBinaryLiteral);
    }
}

//------------------------------------------------------------------------

NS_IMPL_ADDREF(nsRegistryDataSource);
NS_IMPL_RELEASE(nsRegistryDataSource);

NS_IMETHODIMP
nsRegistryDataSource::QueryInterface(const nsIID& aIID, void** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(NS_GET_IID(nsIRDFDataSource)) ||
        aIID.Equals(NS_GET_IID(nsISupports))) {
        *aResult = NS_STATIC_CAST(nsIRDFDataSource*, this);
    }
    else if (aIID.Equals(NS_GET_IID(nsIRegistryDataSource))) {
        *aResult = NS_STATIC_CAST(nsIRegistryDataSource*, this);
    }
    else {
        *aResult = nsnull;
        return NS_NOINTERFACE;
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*, *aResult));
    return NS_OK;
}

//------------------------------------------------------------------------

nsresult
nsRegistryDataSource::Init()
{
    if (gRefCnt++ == 0) {
        nsresult rv;
        rv = nsServiceManager::GetService(kRDFServiceCID,
                                          NS_GET_IID(nsIRDFService),
                                          (nsISupports**) &gRDF);
        if (NS_FAILED(rv)) return rv;

        rv = gRDF->GetResource(NS_REGISTRY_NAMESPACE_URI "key:/", &kKeyRoot);
        if (NS_FAILED(rv)) return rv;

        rv = gRDF->GetResource(NS_REGISTRY_NAMESPACE_URI "subkeys", &kSubkeys);
        if (NS_FAILED(rv)) return rv;

        rv = gRDF->GetLiteral(NS_LITERAL_STRING("[binary data]").get(), &kBinaryLiteral);
        if (NS_FAILED(rv)) return rv;
    }

    return NS_OK;
}


PRInt32
nsRegistryDataSource::GetKey(nsIRDFResource* aResource)
{
    // Quick check for common resources
    if (aResource == kKeyRoot) {
        return nsIRegistry::Common;
    }

    nsresult rv;
    const char* uri;
    rv = aResource->GetValueConst(&uri);
    if (NS_FAILED(rv)) return PR_FALSE;

    if (PL_strncmp(uri, kKeyPrefix, sizeof(kKeyPrefix) - 1) != 0)
        return -1;

    nsRegistryKey key;
    const char* path = uri + sizeof(kKeyPrefix); // one extra to skip initial '/'
    rv = mRegistry->GetSubtree(nsIRegistry::Common, path, &key);
    if (NS_FAILED(rv)) return -1;

    return key;
}




//------------------------------------------------------------------------
//
// nsIRegistryViewer interface
//

NS_IMETHODIMP
nsRegistryDataSource::Open(const char* aPlatformFileName)
{
    NS_PRECONDITION(aPlatformFileName != nsnull, "null ptr");
    if (! aPlatformFileName)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            NS_GET_IID(nsIRegistry),
                                            getter_AddRefs(mRegistry));
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->Open(aPlatformFileName);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::OpenWellKnownRegistry(PRInt32 aID)
{
    nsresult rv;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            NS_GET_IID(nsIRegistry),
                                            getter_AddRefs(mRegistry));
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->OpenWellKnownRegistry(aID);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::OpenDefaultRegistry()
{
    nsresult rv;
    rv = nsComponentManager::CreateInstance(kRegistryCID,
                                            nsnull,
                                            NS_GET_IID(nsIRegistry),
                                            getter_AddRefs(mRegistry));
    if (NS_FAILED(rv)) return rv;

    rv = mRegistry->OpenWellKnownRegistry(nsIRegistry::ApplicationRegistry);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

//------------------------------------------------------------------------
//
// nsIRDFDataSource interface
//

NS_IMETHODIMP
nsRegistryDataSource::GetURI(char * *aURI)
{
    *aURI = nsCRT::strdup("rdf:registry");
    return *aURI ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}


NS_IMETHODIMP
nsRegistryDataSource::GetSource(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsIRDFResource **_retval)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRegistryDataSource::GetSources(nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRegistryDataSource::GetTarget(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsIRDFNode **_retval)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    PRInt32 key;
    if (aTruthValue && ((key = GetKey(aSource)) != -1)) {
        nsresult rv;

        if (aProperty == kSubkeys) {
            nsCOMPtr<nsISimpleEnumerator> results;
            rv = GetTargets(aSource, aProperty, aTruthValue, getter_AddRefs(results));
            if (NS_FAILED(rv)) return rv;

            PRBool hasMore;
            rv = results->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (hasMore) {
                nsCOMPtr<nsISupports> isupports;
                rv = results->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                return isupports->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) _retval);
            }
        }
        else {
            const char* uri;
            rv = aProperty->GetValueConst(&uri);
            if (NS_FAILED(rv)) return rv;

            if (PL_strncmp(uri, kValuePrefix, sizeof(kValuePrefix) -1) == 0) {
                const char* path = uri + sizeof(kValuePrefix) - 1;

                PRUint32 type;
                rv = mRegistry->GetValueType(key, path, &type);
                if (NS_FAILED(rv)) return rv;

                switch (type) {
                case nsIRegistry::String: {
                    nsXPIDLCString value;
                    rv = mRegistry->GetStringUTF8(key, path, getter_Copies(value));
                    if (NS_FAILED(rv)) return rv;

                    nsCOMPtr<nsIRDFLiteral> literal;
                    rv = gRDF->GetLiteral(NS_ConvertASCIItoUCS2(value).get(), getter_AddRefs(literal));
                    if (NS_FAILED(rv)) return rv;

                    return literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) _retval);
                }

                case nsIRegistry::Int32: {
                    PRInt32 value;
                    rv = mRegistry->GetInt(key, path, &value);
                    if (NS_FAILED(rv)) return rv;

                    nsCOMPtr<nsIRDFInt> literal;
                    rv = gRDF->GetIntLiteral(value, getter_AddRefs(literal));
                    if (NS_FAILED(rv)) return rv;

                    return literal->QueryInterface(NS_GET_IID(nsIRDFNode), (void**) _retval);
                }

                case nsIRegistry::Bytes:
                case nsIRegistry::File:
                default:
                    *_retval = kBinaryLiteral;
                    NS_ADDREF(*_retval);
                    return NS_OK;
                }
            }
        }
    }

    *_retval = nsnull;
    return NS_RDF_NO_VALUE;
}


NS_IMETHODIMP
nsRegistryDataSource::GetTargets(nsIRDFResource *aSource, nsIRDFResource *aProperty, PRBool aTruthValue, nsISimpleEnumerator **_retval)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    do {
        if (! aTruthValue)
            break;

        if (aProperty == kSubkeys) {
            return SubkeyEnumerator::Create(this, aSource, _retval);
        }
        else {
            nsresult rv;
            nsCOMPtr<nsIRDFNode> node;
            rv = GetTarget(aSource, aProperty, aTruthValue, getter_AddRefs(node));
            if (NS_FAILED(rv)) return rv;

            if (node) {
                return NS_NewSingletonEnumerator(_retval, node);
            }
        }
    } while (0);

    return NS_NewEmptyEnumerator(_retval);
}


NS_IMETHODIMP
nsRegistryDataSource::Assert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue)
{
    return NS_RDF_ASSERTION_REJECTED;
}


NS_IMETHODIMP
nsRegistryDataSource::Unassert(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_ASSERTION_REJECTED;
}


NS_IMETHODIMP
nsRegistryDataSource::Change(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aOldTarget, nsIRDFNode *aNewTarget)
{
    return NS_RDF_ASSERTION_REJECTED;
}


NS_IMETHODIMP
nsRegistryDataSource::Move(nsIRDFResource *aOldSource, nsIRDFResource *aNewSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget)
{
    return NS_RDF_ASSERTION_REJECTED;
}


NS_IMETHODIMP
nsRegistryDataSource::HasAssertion(nsIRDFResource *aSource, nsIRDFResource *aProperty, nsIRDFNode *aTarget, PRBool aTruthValue, PRBool *_retval)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aProperty != nsnull, "null ptr");
    if (! aProperty)
        return NS_ERROR_NULL_POINTER;

    NS_PRECONDITION(aTarget != nsnull, "null ptr");
    if (! aTarget)
        return NS_ERROR_NULL_POINTER;

    PRInt32 key;
    if (aTruthValue && ((key = GetKey(aSource)) != -1)) {
        nsresult rv;

        if (aProperty == kSubkeys) {
            nsCOMPtr<nsISimpleEnumerator> results;
            rv = GetTargets(aSource, aProperty, aTruthValue, getter_AddRefs(results));
            if (NS_FAILED(rv)) return rv;

            do {
                PRBool hasMore;
                rv = results->HasMoreElements(&hasMore);
                if (NS_FAILED(rv)) return rv;

                if (! hasMore)
                    break;

                nsCOMPtr<nsISupports> isupports;
                rv = results->GetNext(getter_AddRefs(isupports));
                if (NS_FAILED(rv)) return rv;

                nsCOMPtr<nsIRDFNode> node = do_QueryInterface(isupports);
                NS_ASSERTION(node != nsnull, "not an nsIRDFNode");
                if (! node)
                    return NS_ERROR_UNEXPECTED;

                if (node.get() == aTarget) {
                    *_retval = PR_TRUE;
                    return NS_OK;
                }
            } while (0);
        }
        else {
            nsCOMPtr<nsIRDFNode> node;
            rv = GetTarget(aSource, aProperty, aTruthValue, getter_AddRefs(node));
            if (NS_FAILED(rv)) return rv;

            if (node.get() == aTarget) {
                *_retval = PR_TRUE;
                return NS_OK;
            }
        }
    }

    *_retval = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::AddObserver(nsIRDFObserver *aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (! mObservers) {
        nsresult rv;
        rv = NS_NewISupportsArray(getter_AddRefs(mObservers));
        if (NS_FAILED(rv)) return rv;
    }

    mObservers->AppendElement(aObserver);
    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::RemoveObserver(nsIRDFObserver *aObserver)
{
    NS_PRECONDITION(aObserver != nsnull, "null ptr");
    if (! aObserver)
        return NS_ERROR_NULL_POINTER;

    if (mObservers) {
        mObservers->RemoveElement(aObserver);
    }

    return NS_OK;
}


NS_IMETHODIMP 
nsRegistryDataSource::HasArcIn(nsIRDFNode *aNode, nsIRDFResource *aArc, PRBool *result)
{
    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP 
nsRegistryDataSource::HasArcOut(nsIRDFResource *aSource, nsIRDFResource *aArc, PRBool *result)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    PRInt32 key = GetKey(aSource);
    if (key == -1) {
        *result = PR_FALSE;
        return NS_OK;
    }

    nsresult rv;

    if (aArc == kSubkeys) {
        *result = PR_TRUE;
        return NS_OK;
    }

    if (key != nsIRegistry::Common) {
        // XXX In hopes that we'll all be using nsISimpleEnumerator someday
        nsCOMPtr<nsIEnumerator> values0;
        rv = mRegistry->EnumerateValues(key, getter_AddRefs(values0));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsISimpleEnumerator> values;
        rv = NS_NewAdapterEnumerator(getter_AddRefs(values), values0);
        if (NS_FAILED(rv)) return rv;

        do {
            PRBool hasMore;
            rv = values->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = values->GetNext(getter_AddRefs(isupports));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRegistryValue> value = do_QueryInterface(isupports);
            NS_ASSERTION(value != nsnull, "not a registry value");
            if (! value)
                return NS_ERROR_UNEXPECTED;

            nsXPIDLCString valueStr;
            rv = value->GetNameUTF8(getter_Copies(valueStr));
            if (NS_FAILED(rv)) return rv;

            nsCAutoString propertyStr(kValuePrefix);
            propertyStr += (const char*) valueStr;
        
            nsCOMPtr<nsIRDFResource> property;
            rv = gRDF->GetResource(propertyStr.get(), getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            if (aArc == property.get()) {
                *result = PR_TRUE;
                return NS_OK;
            }
        } while (1);
    }

    *result = PR_FALSE;
    return NS_OK;
}

NS_IMETHODIMP
nsRegistryDataSource::ArcLabelsIn(nsIRDFNode *aNode, nsISimpleEnumerator **_retval)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRegistryDataSource::ArcLabelsOut(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    NS_PRECONDITION(aSource != nsnull, "null ptr");
    if (! aSource)
        return NS_ERROR_NULL_POINTER;

    PRInt32 key = GetKey(aSource);
    if (key == -1)
        return NS_NewEmptyEnumerator(_retval);

    nsresult rv;

    nsCOMPtr<nsISupportsArray> array;
    rv = NS_NewISupportsArray(getter_AddRefs(array));
    if (NS_FAILED(rv)) return rv;

    array->AppendElement(kSubkeys);

    if (key != nsIRegistry::Common) {
        // XXX In hopes that we'll all be using nsISimpleEnumerator someday
        nsCOMPtr<nsIEnumerator> values0;
        rv = mRegistry->EnumerateValues(key, getter_AddRefs(values0));
        if (NS_FAILED(rv)) return rv;

        nsCOMPtr<nsISimpleEnumerator> values;
        rv = NS_NewAdapterEnumerator(getter_AddRefs(values), values0);
        if (NS_FAILED(rv)) return rv;

        do {
            PRBool hasMore;
            rv = values->HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            if (! hasMore)
                break;

            nsCOMPtr<nsISupports> isupports;
            rv = values->GetNext(getter_AddRefs(isupports));
            if (NS_FAILED(rv)) return rv;

            nsCOMPtr<nsIRegistryValue> value = do_QueryInterface(isupports);
            NS_ASSERTION(value != nsnull, "not a registry value");
            if (! value)
                return NS_ERROR_UNEXPECTED;

            nsXPIDLCString valueStr;
            rv = value->GetNameUTF8(getter_Copies(valueStr));
            if (NS_FAILED(rv)) return rv;

            nsCAutoString propertyStr(kValuePrefix);
            propertyStr += (const char*) valueStr;
        
            nsCOMPtr<nsIRDFResource> property;
            rv = gRDF->GetResource(propertyStr.get(), getter_AddRefs(property));
            if (NS_FAILED(rv)) return rv;

            array->AppendElement(property);
        } while (1);
    }

    return NS_NewArrayEnumerator(_retval, array);
}


NS_IMETHODIMP
nsRegistryDataSource::GetAllResources(nsISimpleEnumerator **_retval)
{
    return NS_NewEmptyEnumerator(_retval);
}


NS_IMETHODIMP
nsRegistryDataSource::GetAllCommands(nsIRDFResource *aSource, nsIEnumerator **_retval)
{
    NS_NOTYETIMPLEMENTED("write me");
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsRegistryDataSource::IsCommandEnabled(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments, PRBool *_retval)
{
    *_retval = PR_FALSE;
    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::DoCommand(nsISupportsArray *aSources, nsIRDFResource *aCommand, nsISupportsArray *aArguments)
{
    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::GetAllCmds(nsIRDFResource *aSource, nsISimpleEnumerator **_retval)
{
    return NS_NewEmptyEnumerator(_retval);
}


//------------------------------------------------------------------------
//
// nsSubkeyEnumerator
//

nsRegistryDataSource::SubkeyEnumerator::SubkeyEnumerator(nsRegistryDataSource* aViewer, nsIRDFResource* aRootKey)
    : mViewer(aViewer),
      mRootKey(aRootKey),
      mStarted(PR_FALSE)
{
    NS_INIT_REFCNT();
    NS_ADDREF(mViewer);
}

nsRegistryDataSource::SubkeyEnumerator::~SubkeyEnumerator()
{
    NS_RELEASE(mViewer);
}

nsresult
nsRegistryDataSource::SubkeyEnumerator::Init()
{
    NS_PRECONDITION(mViewer->mRegistry != nsnull, "null ptr");
    if (! mViewer->mRegistry)
        return NS_ERROR_NULL_POINTER;

    nsresult rv;

    PRInt32 key = mViewer->GetKey(mRootKey);
    if (key == -1)
        return NS_ERROR_UNEXPECTED;

    rv = mViewer->mRegistry->EnumerateSubtrees(key, getter_AddRefs(mEnum));
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}


nsresult
nsRegistryDataSource::SubkeyEnumerator::Create(nsRegistryDataSource* aViewer,
                                           nsIRDFResource* aRootKey,
                                           nsISimpleEnumerator** aResult)
{
    SubkeyEnumerator* result = new SubkeyEnumerator(aViewer, aRootKey);
    if (! result)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    rv = result->Init();
    if (NS_FAILED(rv)) {
        delete result;
        return rv;
    }

    *aResult = result;
    NS_ADDREF(*aResult);
    return NS_OK;
}


nsresult
nsRegistryDataSource::SubkeyEnumerator::ConvertRegistryNodeToResource(nsISupports* aRegistryNode,
                                                                      nsIRDFResource** aResult)
{
    nsCOMPtr<nsIRegistryNode> node = do_QueryInterface(aRegistryNode);
    NS_ASSERTION(node != nsnull, "not a registry node");
    if (! node)
        return NS_ERROR_UNEXPECTED;

    nsresult rv;

    const char* rootURI;
    rv = mRootKey->GetValueConst(&rootURI);
    if (NS_FAILED(rv)) return rv;

    nsXPIDLCString path;
    rv = node->GetNameUTF8(getter_Copies(path));
    if (NS_FAILED(rv)) return rv;

    nsCAutoString newURI(rootURI);
    if (newURI.Last() != '/') newURI += '/';
    newURI.Append(path);

    rv = gRDF->GetResource(newURI.get(), aResult);
    if (NS_FAILED(rv)) return rv;

    return NS_OK;
}

NS_IMPL_ISUPPORTS1(nsRegistryDataSource::SubkeyEnumerator, nsISimpleEnumerator)

NS_IMETHODIMP
nsRegistryDataSource::SubkeyEnumerator::HasMoreElements(PRBool* _retval)
{
    nsresult rv;

    if (mCurrent) {
        *_retval = PR_TRUE;
        return NS_OK;
    }

    if (! mStarted) {
        mStarted = PR_TRUE;
        rv = mEnum->First();
        if (rv == NS_OK) {
            nsCOMPtr<nsISupports> isupports;
            mEnum->CurrentItem(getter_AddRefs(isupports));

            rv = ConvertRegistryNodeToResource(isupports, getter_AddRefs(mCurrent));
            if (NS_FAILED(rv)) return rv;

            *_retval = PR_TRUE;
        }
        else {
            *_retval = PR_FALSE;
        }
    }
    else {
        *_retval = PR_FALSE;

        rv = mEnum->IsDone();
        if (rv != NS_OK) {
            // We're not done. Advance to the next one.
            rv = mEnum->Next();
            if (rv == NS_OK) {
                nsCOMPtr<nsISupports> isupports;
                mEnum->CurrentItem(getter_AddRefs(isupports));

                rv = ConvertRegistryNodeToResource(isupports, getter_AddRefs(mCurrent));
                if (NS_FAILED(rv)) return rv;

                *_retval = PR_TRUE;
            }
        }
    }
    return NS_OK;
}


NS_IMETHODIMP
nsRegistryDataSource::SubkeyEnumerator::GetNext(nsISupports** _retval)
{
    nsresult rv;

    PRBool hasMore;
    rv = HasMoreElements(&hasMore);
    if (NS_FAILED(rv)) return rv;

    if (! hasMore)
        return NS_ERROR_UNEXPECTED;

    *_retval = mCurrent;
    NS_ADDREF(*_retval);
    mCurrent = nsnull;
    return NS_OK;
}


//------------------------------------------------------------------------
//
// Module implementation
//

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsRegistryDataSource, Init)

// The list of components we register
static nsModuleComponentInfo components[] = {
    { "Registry Viewer", NS_REGISTRYVIEWER_CID,
      "@mozilla.org/registry-viewer;1", nsRegistryDataSourceConstructor,
    },
};

NS_IMPL_NSGETMODULE(nsRegistryViewerModule, components)
