/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

#include "nsStreamConverterService.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsString2.h"
#include "nsIAtom.h"
#include "nsDeque.h"
#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamConverter.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////
// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS(nsStreamConverterService, NS_GET_IID(nsIStreamConverterService));


////////////////////////////////////////////////////////////
// nsIStreamConverterService methods

////////////////////////////////////////////////////////////
// nsStreamConverterService methods
nsStreamConverterService::nsStreamConverterService() {
    NS_INIT_ISUPPORTS();
    mAdjacencyList = nsnull;
}

// Delete all the entries in the adjacency list
static PRBool PR_CALLBACK DeleteAdjacencyEntry(nsHashKey *aKey, void *aData, void* closure) {
    SCTableData *entry = (SCTableData*)aData;
    NS_ASSERTION(entry->key && entry->keyString && entry->data, "malformed adjacency list entry");
    delete entry->key;
    delete entry->keyString;

    // clear out the edges
    nsVoidArray *edges = (nsVoidArray*)entry->data;
    nsIAtom *vertex;
    while ( (vertex = (nsIAtom*)edges->ElementAt(0)) ) {
        NS_RELEASE(vertex);
        edges->RemoveElementAt(0);
    }
    delete (nsVoidArray *)entry->data;
    delete entry;
    return PR_TRUE;   
};

nsStreamConverterService::~nsStreamConverterService() {
    // Clean up the adjacency list table.
    mAdjacencyList->Reset(DeleteAdjacencyEntry, nsnull);
    delete mAdjacencyList;
}

nsresult
nsStreamConverterService::Init() {
    mAdjacencyList = new nsHashtable();
    if (!mAdjacencyList) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

// Builds the graph represented as an adjacency list (and built up in 
// memory using an nsHashtable and nsVoidArray combination).
//
// :BuildGraph() consults the registry for all stream converter CONTRACTIDS then fills the
// adjacency list with edges.
// An edge in this case is comprised of a FROM and TO MIME type combination.
// 
// CONTRACTID format:
// @mozilla.org/streamconv;1?from=text/html&to=text/plain
// XXX curently we only handle a single from and to combo, we should repeat the 
// XXX registration process for any series of from-to combos.
// XXX can use nsTokenizer for this.
//

nsresult
nsStreamConverterService::BuildGraph() {

    nsresult rv;
    // enumerate the registry subkeys
    nsIRegistry *registry = nsnull;
    nsRegistryKey key;
    nsIEnumerator *components = nsnull;
    rv = nsServiceManager::GetService(NS_REGISTRY_CONTRACTID,
                                      NS_GET_IID(nsIRegistry),
                                      (nsISupports**)&registry);
    if (NS_FAILED(rv)) return rv;

    rv = registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
    if (NS_FAILED(rv)) return rv;

    rv = registry->GetSubtree(nsIRegistry::Common,
                              NS_ISTREAMCONVERTER_KEY,
                              &key);
    if (NS_FAILED(rv)) return rv;

    rv = registry->EnumerateSubtrees(key, &components);
    if (NS_FAILED(rv)) return rv;

    // go ahead and enumerate through.
    rv = components->First();
    while (NS_SUCCEEDED(rv) && (NS_OK != components->IsDone())) {
        nsISupports *base = nsnull;

        rv = components->CurrentItem(&base);
        if (NS_FAILED(rv)) return rv;

        nsIRegistryNode *node = nsnull;
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        rv = base->QueryInterface(nodeIID, (void**)&node);
        if (NS_FAILED(rv)) return rv;

        char *name = nsnull;
        rv = node->GetNameUTF8(&name);
        if (NS_FAILED(rv)) return rv;

        nsCString actualContractID(NS_ISTREAMCONVERTER_KEY);
        actualContractID.Append(name);

        // now we've got the CONTRACTID, let's parse it up.
        rv = AddAdjacency(actualContractID.GetBuffer());

        // cleanup
        nsCRT::free(name);
        NS_RELEASE(node);
        NS_RELEASE(base);
        rv = components->Next();
    }

    NS_IF_RELEASE( components );
    nsServiceManager::ReleaseService( NS_REGISTRY_CONTRACTID, registry );

    return NS_OK;
}


// XXX currently you can not add the same adjacency (i.e. you can't have multiple
// XXX stream converters registering to handle the same from-to combination. It's
// XXX not programatically prohibited, it's just that results are un-predictable
// XXX right now.
nsresult
nsStreamConverterService::AddAdjacency(const char *aContractID) {
    nsresult rv;
    // first parse out the FROM and TO MIME-types.

    nsCString fromStr, toStr;
    rv = ParseFromTo(aContractID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // Each MIME-type is a vertex in the graph, so first lets make sure
    // each MIME-type is represented as a key in our hashtable.

    PRBool delFrom = PR_TRUE, delTo = PR_TRUE;
    nsCStringKey *fromKey = new nsCStringKey(fromStr.GetBuffer());
    if (!fromKey) return NS_ERROR_OUT_OF_MEMORY;
    nsCStringKey *toKey = new nsCStringKey(toStr.GetBuffer());
    if (!toKey) {
        delete fromKey;
        return NS_ERROR_OUT_OF_MEMORY;
    }

    SCTableData *edges = (SCTableData*)mAdjacencyList->Get(fromKey);
    if (!edges) {
        // There is no fromStr vertex, create one.
        SCTableData *data = new SCTableData;
        if (!data) {
            delete fromKey;
            delete toKey;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        data->key = fromKey;
        delFrom = PR_FALSE;
        data->keyString = new nsCString(fromStr.GetBuffer());
        if (!data->keyString) {
            delete fromKey;
            delete toKey;
            delete data;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        data->data = new nsVoidArray();
        if (!data->data) {
            delete data->keyString;
            delete fromKey;
            delete toKey;
            delete data;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mAdjacencyList->Put(fromKey, data);
    }

    edges = (SCTableData*)mAdjacencyList->Get(toKey);
    if (!edges) {
        // There is no toStr vertex, create one.
        SCTableData *data = new SCTableData;
        if (!data) {
            delete fromKey;
            delete toKey;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        data->key = toKey;
        delTo = PR_FALSE;
        data->keyString = new nsCString(toStr.GetBuffer());
        if (!data->keyString) {
            delete fromKey;
            delete toKey;
            delete data;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        data->data = new nsVoidArray();
        if (!data->data) {
            delete data->keyString;
            delete fromKey;
            delete toKey;
            delete data;
            return NS_ERROR_OUT_OF_MEMORY;
        }
        mAdjacencyList->Put(toKey, data);
    }
    if (delTo)
        delete toKey;
    
    // Now we know the FROM and TO types are represented as keys in the hashtable.
    // Let's "connect" the verticies, making an edge.

    edges = (SCTableData*)mAdjacencyList->Get(fromKey);
    if (delFrom)
        delete fromKey;
    NS_ASSERTION(edges, "something wrong in adjacency list construction");
    const char *toCStr = toStr.GetBuffer();
    nsIAtom *vertex = NS_NewAtom(toCStr); 
    if (!vertex) return NS_ERROR_OUT_OF_MEMORY;
    nsVoidArray *adjacencyList = (nsVoidArray*)edges->data;
    rv = adjacencyList->AppendElement(vertex) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
    return rv;
}

nsresult
nsStreamConverterService::ParseFromTo(const char *aContractID, nsCString &aFromRes, nsCString &aToRes) {

    nsCString ContractIDStr(aContractID);

    PRInt32 fromLoc = ContractIDStr.Find("from=") + 5;
    PRInt32 toLoc   = ContractIDStr.Find("to=") + 3;
    if (-1 == fromLoc || -1 == toLoc ) return NS_ERROR_FAILURE;

    nsCString fromStr, toStr;

    ContractIDStr.Mid(fromStr, fromLoc, toLoc - 4 - fromLoc);
    ContractIDStr.Mid(toStr, toLoc, ContractIDStr.Length() - toLoc);

    aFromRes.Assign(fromStr);
    aToRes.Assign(toStr);

    return NS_OK;
}

// nsHashtable enumerator functions.

// Initializes the BFS state table.
static PRBool PR_CALLBACK InitBFSTable(nsHashKey *aKey, void *aData, void* closure) {
    nsHashtable *BFSTable = (nsHashtable*)closure;
    if (!BFSTable) return PR_FALSE;

    BFSState *state = new BFSState;
    if (!state) return NS_ERROR_OUT_OF_MEMORY;

    state->color = white;
    state->distance = -1;
    state->predecessor = nsnull;

    SCTableData *data = new SCTableData;
    if (!data) return NS_ERROR_OUT_OF_MEMORY;
    data->key = aKey->Clone();

    SCTableData *origData = (SCTableData*)aData;
    NS_ASSERTION(origData, "no data in the table enumeration");
    data->keyString = new nsCString(*origData->keyString);
    data->data = state;

    BFSTable->Put(aKey, data);
    return PR_TRUE;   
};

// cleans up the BFS state table
static PRBool PR_CALLBACK DeleteBFSEntry(nsHashKey *aKey, void *aData, void *closure) {
    SCTableData *data = (SCTableData*)aData;
    delete data->key;
    delete data->keyString;
    BFSState *state = (BFSState*)data->data;
    if (state->predecessor) // there might not be a predecessor depending on the graph
        delete state->predecessor;
    delete state;
    delete data;
    return PR_TRUE;
}

// walks the graph using a breadth-first-search algorithm which generates a discovered
// verticies tree. This tree is then walked up (from destination vertex, to origin vertex)
// and each link in the chain is added to an nsStringArray. A direct lookup for the given
// CONTRACTID should be made prior to calling this method in an attempt to find a direct
// converter rather than walking the graph.
nsresult
nsStreamConverterService::FindConverter(const char *aContractID, nsCStringArray **aEdgeList) {
    nsresult rv;
    if (!aEdgeList) return NS_ERROR_NULL_POINTER;

    // walk the graph in search of the appropriate converter.

    PRInt32 vertexCount = mAdjacencyList->Count();
    if (0 >= vertexCount)
        return NS_ERROR_FAILURE;

    // Create a corresponding color table for each vertex in the graph.
    nsHashtable lBFSTable;
    mAdjacencyList->Enumerate(InitBFSTable, &lBFSTable);

    NS_ASSERTION(lBFSTable.Count() == vertexCount, "strmconv BFS table init problem");

    // This is our source vertex; our starting point.
    nsCString fromC, toC;
    rv = ParseFromTo(aContractID, fromC, toC);
    if (NS_FAILED(rv)) return rv;

    nsCStringKey *source = new nsCStringKey(fromC.GetBuffer());
    if (!source) return NS_ERROR_OUT_OF_MEMORY;

    SCTableData *data = (SCTableData*)lBFSTable.Get(source);
    if (!data) return NS_ERROR_FAILURE;

    BFSState *state = (BFSState*)data->data;

    state->color = gray;
    state->distance = 0;
    nsDeque *grayQ = new nsDeque(0);

    // Now generate the shortest path tree.
    grayQ->Push(source);
    while (0 < grayQ->GetSize()) {
        nsHashKey *currentHead = (nsHashKey*)grayQ->PeekFront();
        SCTableData *data2 = (SCTableData*)mAdjacencyList->Get(currentHead);
        if (!data2) return NS_ERROR_FAILURE;
        nsVoidArray *edges = (nsVoidArray*)data2->data;
        NS_ASSERTION(edges, "something went wrong with BFS strmconv algorithm");

        // Get the state of the current head to calculate the distance of each
        // reachable vertex in the loop.
        data2 = (SCTableData*)lBFSTable.Get(currentHead);
        BFSState *headVertexState = (BFSState*)data2->data;
        NS_ASSERTION(headVertexState, "problem with the BFS strmconv algorithm");

        PRInt32 edgeCount = edges->Count();
        for (int i = 0; i < edgeCount; i++) {
            
            nsIAtom *curVertexAtom = (nsIAtom*)edges->ElementAt(i);
            nsString2 curVertexStr;
            nsStr::Initialize(curVertexStr, eOneByte);
            curVertexAtom->ToString(curVertexStr);
            char * curVertexCString = curVertexStr.ToNewCString();
            nsCStringKey *curVertex = new nsCStringKey(curVertexCString);
            nsMemory::Free(curVertexCString);

            SCTableData *data3 = (SCTableData*)lBFSTable.Get(curVertex);
            BFSState *curVertexState = (BFSState*)data3->data;
            NS_ASSERTION(curVertexState, "something went wrong with the BFS strmconv algorithm");
            if (white == curVertexState->color) {
                curVertexState->color = gray;
                curVertexState->distance = headVertexState->distance + 1;
                curVertexState->predecessor = currentHead->Clone();
                grayQ->Push(curVertex);
            } else {
                delete curVertex; // if this vertex has already been discovered, we don't want
                                  // to leak it. (non-discovered vertex's get cleaned up when
                                  // they're popped).
            }
        }
        headVertexState->color = black;
        nsCStringKey *cur = (nsCStringKey*)grayQ->PopFront();
        delete cur;
        cur = nsnull;
    }
    delete grayQ;
    // The shortest path (if any) has been generated and is represetned by the chain of 
    // BFSState->predecessor keys. Start at the bottom and work our way up.

    // first parse out the FROM and TO MIME-types being registered.

    nsCString fromStr, toStr;
    rv = ParseFromTo(aContractID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // get the root CONTRACTID
    nsCString ContractIDPrefix(NS_ISTREAMCONVERTER_KEY);
    nsCStringArray *shortestPath = new nsCStringArray();
    //nsVoidArray *shortestPath = new nsVoidArray();
    nsCStringKey *toMIMEType = new nsCStringKey(toStr);
    data = (SCTableData*)lBFSTable.Get(toMIMEType);
    delete toMIMEType;

    if (!data) {
        // If this vertex isn't in the BFSTable, then no-one has registered for it,
        // therefore we can't do the conversion.
        *aEdgeList = nsnull;
        return NS_ERROR_FAILURE;
    }

    while (data) {
        BFSState *curState = (BFSState*)data->data;

        if (data->keyString->Equals(fromStr)) {
            // found it. We're done here.
            *aEdgeList = shortestPath;
            lBFSTable.Reset(DeleteBFSEntry, nsnull);
            return NS_OK;
        }

        // reconstruct the CONTRACTID.
        // Get the predecessor.
        SCTableData *predecessorData = (SCTableData*)lBFSTable.Get(curState->predecessor);
        if (!predecessorData) break; // no predecessor, chain doesn't exist.

        // build out the CONTRACTID.
        nsCString *newContractID = new nsCString(ContractIDPrefix);
        newContractID->Append("?from=");

        newContractID->Append(predecessorData->keyString->GetBuffer());

        newContractID->Append("&to=");
        newContractID->Append(data->keyString->GetBuffer());
    
        // Add this CONTRACTID to the chain.
        rv = shortestPath->AppendCString(*newContractID) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");

        // move up the tree.
        data = predecessorData;
    }

    lBFSTable.Reset(DeleteBFSEntry, nsnull);
    *aEdgeList = nsnull;
    return NS_ERROR_FAILURE; // couldn't find a stream converter or chain.

}


/////////////////////////////////////////////////////
// nsIStreamConverter methods
NS_IMETHODIMP
nsStreamConverterService::Convert(nsIInputStream *aFromStream,
                                  const PRUnichar *aFromType, 
                                  const PRUnichar *aToType,
                                  nsISupports *aContext,
                                  nsIInputStream **_retval) {
    if (!aFromStream || !aFromType || !aToType || !_retval) return NS_ERROR_NULL_POINTER;
    nsresult rv;

    // first determine whether we can even handle this covnversion
    // build a CONTRACTID
    nsCString contractID(NS_ISTREAMCONVERTER_KEY);
    contractID.Append("?from=");
    contractID.AppendWithConversion(aFromType);
    contractID.Append("&to=");
    contractID.AppendWithConversion(aToType);
    const char *cContractID = contractID.GetBuffer();

    nsIComponentManager *comMgr;
    rv = NS_GetGlobalComponentManager(&comMgr);
    if (NS_FAILED(rv)) return rv;

    nsISupports *converter = nsnull;
    rv = comMgr->CreateInstanceByContractID(cContractID, nsnull,
                                        NS_GET_IID(nsIStreamConverter),
                                        (void**)&converter);
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) return rv;

        nsCStringArray *converterChain = nsnull;

        rv = FindConverter(cContractID, &converterChain);
        if (NS_FAILED(rv)) {
            // can't make this conversion.
            // XXX should have a more descriptive error code.
            return NS_ERROR_FAILURE;
        }

        PRInt32 edgeCount = converterChain->Count();
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");


        // convert the stream using each edge of the graph as a step.
        // this is our stream conversion traversal.
        nsIInputStream *dataToConvert = aFromStream;
        nsIInputStream *convertedData = nsnull;
        NS_ADDREF(dataToConvert);

        for (PRInt32 i = edgeCount-1; i >= 0; i--) {
            nsCString *contractIDStr = converterChain->CStringAt(i);
            if (!contractIDStr) {
                delete converterChain;
                return NS_ERROR_FAILURE;
            }
            const char *lContractID = contractIDStr->GetBuffer();

            rv = comMgr->CreateInstanceByContractID(lContractID, nsnull,
                                                NS_GET_IID(nsIStreamConverter),
                                                (void**)&converter);

            if (NS_FAILED(rv)) {
                delete converterChain;                
                return rv;
            }

            nsCString fromStr, toStr;
            rv = ParseFromTo(lContractID, fromStr, toStr);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            nsIStreamConverter *conv = nsnull;
            rv = converter->QueryInterface(NS_GET_IID(nsIStreamConverter), (void**)&conv);
            NS_RELEASE(converter);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            PRUnichar *fromUni = fromStr.ToNewUnicode();
            PRUnichar *toUni   = toStr.ToNewUnicode();
            rv = conv->Convert(dataToConvert, fromUni, toUni, aContext, &convertedData);
            nsMemory::Free(fromUni);
            nsMemory::Free(toUni);
            NS_RELEASE(conv);
            NS_RELEASE(dataToConvert);
            dataToConvert = convertedData;
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }
        }

        delete converterChain;
        *_retval = convertedData;

    } else {
        // we're going direct.
        nsIStreamConverter *conv = nsnull;
        rv = converter->QueryInterface(NS_GET_IID(nsIStreamConverter), (void**)&conv);
        NS_RELEASE(converter);
        if (NS_FAILED(rv)) return rv;
        rv = conv->Convert(aFromStream, aFromType, aToType, aContext, _retval);
        NS_RELEASE(conv);
    }
    
    return rv;
}


NS_IMETHODIMP
nsStreamConverterService::AsyncConvertData(const PRUnichar *aFromType, 
                                           const PRUnichar *aToType, 
                                           nsIStreamListener *aListener,
                                           nsISupports *aContext,
                                           nsIStreamListener **_retval) {
    if (!aFromType || !aToType || !aListener || !_retval) return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // first determine whether we can even handle this covnversion
    // build a CONTRACTID
    nsCString contractID(NS_ISTREAMCONVERTER_KEY);
    contractID.Append("?from=");
    contractID.AppendWithConversion(aFromType);
    contractID.Append("&to=");
    contractID.AppendWithConversion(aToType);
    const char *cContractID = contractID.GetBuffer();

    nsIComponentManager *comMgr;
    rv = NS_GetGlobalComponentManager(&comMgr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamConverter> listener;
    rv = comMgr->CreateInstanceByContractID(cContractID, nsnull,
                                        NS_GET_IID(nsIStreamConverter),
                                        getter_AddRefs(listener));
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) return rv;

        nsCStringArray *converterChain = nsnull;

        rv = FindConverter(cContractID, &converterChain);
        if (NS_FAILED(rv)) {
            // can't make this conversion.
            // XXX should have a more descriptive error code.
            return NS_ERROR_FAILURE;
        }

        PRInt32 edgeCount = converterChain->Count();
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");

        // convert the stream using each edge of the graph as a step.
        // this is our stream conversion traversal.

        nsCOMPtr<nsIStreamListener> forwardListener = aListener;
        nsCOMPtr<nsIStreamListener> fromListener;

        for (int i = 0; i < edgeCount; i++) {
            nsCString *contractIDStr = converterChain->CStringAt(i);
            if (!contractIDStr) {
                delete converterChain;
                return NS_ERROR_FAILURE;
            }
            const char *lContractID = contractIDStr->GetBuffer();

            nsCOMPtr<nsIStreamConverter> converter;
            rv = comMgr->CreateInstanceByContractID(lContractID, nsnull,
                                                NS_GET_IID(nsIStreamConverter),
                                                getter_AddRefs(converter));
            NS_ASSERTION(NS_SUCCEEDED(rv), "graph construction problem, built a contractid that wasn't registered");

            nsCString fromStr, toStr;
            rv = ParseFromTo(lContractID, fromStr, toStr);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            PRUnichar *fromStrUni = fromStr.ToNewUnicode();
            PRUnichar *toStrUni   = toStr.ToNewUnicode();

            rv = converter->AsyncConvertData(fromStrUni, toStrUni, forwardListener, aContext);
            nsMemory::Free(fromStrUni);
            nsMemory::Free(toStrUni);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            nsCOMPtr<nsIStreamListener> chainListener(do_QueryInterface(converter, &rv));
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            // store the listener of the first converter in the chain.
            if (!fromListener)
                fromListener = chainListener;

            forwardListener = chainListener;
        }
        delete converterChain;
        // return the first listener in the chain.
        *_retval = fromListener;
        NS_ADDREF(*_retval);

    } else {
        // we're going direct.
        *_retval = listener;
        NS_ADDREF(*_retval);

        nsCOMPtr<nsIStreamConverter> conv(do_QueryInterface(listener, &rv));
        if (NS_FAILED(rv)) return rv;

        rv = conv->AsyncConvertData(aFromType, aToType, aListener, aContext);
    }
    
    return rv;

}

nsresult
NS_NewStreamConv(nsStreamConverterService** aStreamConv)
{
    NS_PRECONDITION(aStreamConv != nsnull, "null ptr");
    if (! aStreamConv)
        return NS_ERROR_NULL_POINTER;

    *aStreamConv = new nsStreamConverterService();
    if (! *aStreamConv)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aStreamConv);
    (*aStreamConv)->Init();

    return NS_OK;
}
