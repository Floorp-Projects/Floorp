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
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsIAtom.h"
#include "nsDeque.h"
#include "nsIInputStream.h"
#include "nsIOutputStream.h"
#include "nsIStreamConverter.h"
#include "nsICategoryManager.h"
#include "nsISupportsPrimitives.h"
#include "nsXPIDLString.h"

////////////////////////////////////////////////////////////
// nsISupports methods
NS_IMPL_THREADSAFE_ISUPPORTS1(nsStreamConverterService, nsIStreamConverterService);


////////////////////////////////////////////////////////////
// nsIStreamConverterService methods

////////////////////////////////////////////////////////////
// nsStreamConverterService methods
nsStreamConverterService::nsStreamConverterService() : mAdjacencyList(nsnull) {
    NS_INIT_ISUPPORTS();
}

nsStreamConverterService::~nsStreamConverterService() {
    NS_ASSERTION(mAdjacencyList, "init wasn't called, or the retval was ignored");
    delete mAdjacencyList;
}

// Delete all the entries in the adjacency list
static PRBool PR_CALLBACK DeleteAdjacencyEntry(nsHashKey *aKey, void *aData, void* closure) {
    SCTableData *entry = (SCTableData*)aData;
    NS_ASSERTION(entry->key && entry->data.edges, "malformed adjacency list entry");
    delete entry->key;
    NS_RELEASE(entry->data.edges);
    delete entry;
    return PR_TRUE;   
};

nsresult
nsStreamConverterService::Init() {
    mAdjacencyList = new nsObjectHashtable(nsnull, nsnull,
                                           DeleteAdjacencyEntry, nsnull);
    if (!mAdjacencyList) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}

// Builds the graph represented as an adjacency list (and built up in 
// memory using an nsObjectHashtable and nsISupportsArray combination).
//
// :BuildGraph() consults the category manager for all stream converter 
// CONTRACTIDS then fills the adjacency list with edges.
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

    nsCOMPtr<nsICategoryManager> catmgr(do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsISimpleEnumerator> entries;
    rv = catmgr->EnumerateCategory(NS_ISTREAMCONVERTER_KEY, getter_AddRefs(entries));
    if (NS_FAILED(rv)) return rv;

    // go through each entry to build the graph
    nsCOMPtr<nsISupportsString> entry;
    rv = entries->GetNext(getter_AddRefs(entry));
    while (NS_SUCCEEDED(rv)) {

        // get the entry string
        nsXPIDLCString entryString;
        rv = entry->GetData(getter_Copies(entryString));
        if (NS_FAILED(rv)) return rv;
        
        // cobble the entry string w/ the converter key to produce a full contractID.
        nsCString contractID(NS_ISTREAMCONVERTER_KEY);
        contractID.Append((const char *)entryString);

        // now we've got the CONTRACTID, let's parse it up.
        rv = AddAdjacency(contractID.get());
        if (NS_FAILED(rv)) return rv;

        rv = entries->GetNext(getter_AddRefs(entry));
    }

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

    nsCStringKey *fromKey = new nsCStringKey(ToNewCString(fromStr), fromStr.Length(), nsCStringKey::OWN);
    if (!fromKey) return NS_ERROR_OUT_OF_MEMORY;

    SCTableData *fromEdges = (SCTableData*)mAdjacencyList->Get(fromKey);
    if (!fromEdges) {
        // There is no fromStr vertex, create one.
        SCTableData *data = new SCTableData;
        if (!data) {
            delete fromKey;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        data->key = fromKey;
        nsCOMPtr<nsISupportsArray> edgeArray;
        rv = NS_NewISupportsArray(getter_AddRefs(edgeArray));
        if (NS_FAILED(rv)) {
            delete fromKey;
            return rv;
        }
        NS_ADDREF(data->data.edges = edgeArray.get());

        mAdjacencyList->Put(fromKey, data);
        fromEdges = data;
    } else {
        delete fromKey;
        fromKey = nsnull;
    }

    nsCStringKey *toKey = new nsCStringKey(ToNewCString(toStr), toStr.Length(), nsCStringKey::OWN);
    if (!toKey) return NS_ERROR_OUT_OF_MEMORY;

    if (!mAdjacencyList->Get(toKey)) {
        // There is no toStr vertex, create one.
        SCTableData *data = new SCTableData;
        if (!data) {
            delete toKey;
            return NS_ERROR_OUT_OF_MEMORY;
        }

        data->key = toKey;
        nsCOMPtr<nsISupportsArray> edgeArray;
        rv = NS_NewISupportsArray(getter_AddRefs(edgeArray));
        if (NS_FAILED(rv)) {
            delete toKey;
            return rv;
        }
        NS_ADDREF(data->data.edges = edgeArray.get());

        mAdjacencyList->Put(toKey, data);
    } else {
        delete toKey;
        toKey = nsnull;
    }
    
    // Now we know the FROM and TO types are represented as keys in the hashtable.
    // Let's "connect" the verticies, making an edge.

    nsIAtom *vertex = NS_NewAtom(toStr.get()); 
    if (!vertex) return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(fromEdges, "something wrong in adjacency list construction");
    nsCOMPtr<nsISupportsArray> adjacencyList = fromEdges->data.edges;
    return adjacencyList->AppendElement(vertex) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
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

// nsObjectHashtable enumerator functions.

// Initializes the BFS state table.
static PRBool PR_CALLBACK InitBFSTable(nsHashKey *aKey, void *aData, void* closure) {
    NS_ASSERTION((SCTableData*)aData, "no data in the table enumeration");
    
    nsHashtable *BFSTable = (nsHashtable*)closure;
    if (!BFSTable) return PR_FALSE;

    BFSState *state = new BFSState;
    if (!state) return NS_ERROR_OUT_OF_MEMORY;

    state->color = white;
    state->distance = -1;
    state->predecessor = nsnull;

    SCTableData *data = new SCTableData;
    if (!data) return NS_ERROR_OUT_OF_MEMORY;
    data->key = (nsCStringKey*)aKey->Clone();
    data->data.state = state;

    BFSTable->Put(aKey, data);
    return PR_TRUE;   
};

// cleans up the BFS state table
static PRBool PR_CALLBACK DeleteBFSEntry(nsHashKey *aKey, void *aData, void *closure) {
    SCTableData *data = (SCTableData*)aData;
    delete data->key;
    BFSState *state = data->data.state;
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
    if (0 >= vertexCount) return NS_ERROR_FAILURE;

    // Create a corresponding color table for each vertex in the graph.
    nsObjectHashtable lBFSTable(nsnull, nsnull, DeleteBFSEntry, nsnull);
    mAdjacencyList->Enumerate(InitBFSTable, &lBFSTable);

    NS_ASSERTION(lBFSTable.Count() == vertexCount, "strmconv BFS table init problem");

    // This is our source vertex; our starting point.
    nsCString fromC, toC;
    rv = ParseFromTo(aContractID, fromC, toC);
    if (NS_FAILED(rv)) return rv;

    nsCStringKey *source = new nsCStringKey(fromC.get());

    SCTableData *data = (SCTableData*)lBFSTable.Get(source);
    if (!data) return NS_ERROR_FAILURE;

    BFSState *state = data->data.state;

    state->color = gray;
    state->distance = 0;
    nsDeque grayQ(0);

    // Now generate the shortest path tree.
    grayQ.Push(source);
    while (0 < grayQ.GetSize()) {
        nsHashKey *currentHead = (nsHashKey*)grayQ.PeekFront();
        SCTableData *data2 = (SCTableData*)mAdjacencyList->Get(currentHead);
        if (!data2) return NS_ERROR_FAILURE;
        nsCOMPtr<nsISupportsArray> edges = data2->data.edges;
        NS_ASSERTION(edges, "something went wrong with BFS strmconv algorithm");

        // Get the state of the current head to calculate the distance of each
        // reachable vertex in the loop.
        data2 = (SCTableData*)lBFSTable.Get(currentHead);
        BFSState *headVertexState = data2->data.state;
        NS_ASSERTION(headVertexState, "problem with the BFS strmconv algorithm");

        PRUint32 edgeCount = 0;
        rv = edges->Count(&edgeCount);
        if (NS_FAILED(rv)) return rv;

        for (PRUint32 i = 0; i < edgeCount; i++) {
            
            nsIAtom *curVertexAtom = (nsIAtom*)edges->ElementAt(i);
            nsAutoString curVertexStr;
            curVertexAtom->ToString(curVertexStr);
            nsCStringKey *curVertex = new nsCStringKey(ToNewCString(curVertexStr), 
                                        curVertexStr.Length(), nsCStringKey::OWN);

            SCTableData *data3 = (SCTableData*)lBFSTable.Get(curVertex);
            BFSState *curVertexState = data3->data.state;
            NS_ASSERTION(curVertexState, "something went wrong with the BFS strmconv algorithm");
            if (white == curVertexState->color) {
                curVertexState->color = gray;
                curVertexState->distance = headVertexState->distance + 1;
                curVertexState->predecessor = currentHead->Clone();
                grayQ.Push(curVertex);
            } else {
                delete curVertex; // if this vertex has already been discovered, we don't want
                                  // to leak it. (non-discovered vertex's get cleaned up when
                                  // they're popped).
            }
        }
        headVertexState->color = black;
        nsCStringKey *cur = (nsCStringKey*)grayQ.PopFront();
        delete cur;
        cur = nsnull;
    }
    // The shortest path (if any) has been generated and is represetned by the chain of 
    // BFSState->predecessor keys. Start at the bottom and work our way up.

    // first parse out the FROM and TO MIME-types being registered.

    nsCString fromStr, toStr;
    rv = ParseFromTo(aContractID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // get the root CONTRACTID
    nsCString ContractIDPrefix(NS_ISTREAMCONVERTER_KEY);
    nsCStringArray *shortestPath = new nsCStringArray();
    nsCStringKey toMIMEType(toStr);
    data = (SCTableData*)lBFSTable.Get(&toMIMEType);

    if (!data) {
        // If this vertex isn't in the BFSTable, then no-one has registered for it,
        // therefore we can't do the conversion.
        *aEdgeList = nsnull;
        return NS_ERROR_FAILURE;
    }

    while (data) {
        BFSState *curState = data->data.state;

        nsCStringKey *key = (nsCStringKey*)data->key;
        
        if (fromStr.Equals(key->GetString())) {
            // found it. We're done here.
            *aEdgeList = shortestPath;
            return NS_OK;
        }

        // reconstruct the CONTRACTID.
        // Get the predecessor.
        if (!curState->predecessor) break; // no predecessor
        SCTableData *predecessorData = (SCTableData*)lBFSTable.Get(curState->predecessor);
        if (!predecessorData) break; // no predecessor, chain doesn't exist.

        // build out the CONTRACTID.
        nsCString newContractID(ContractIDPrefix);
        newContractID.Append("?from=");

        nsCStringKey *predecessorKey = (nsCStringKey*)predecessorData->key;
        newContractID.Append(predecessorKey->GetString());

        newContractID.Append("&to=");
        newContractID.Append(key->GetString());
    
        // Add this CONTRACTID to the chain.
        rv = shortestPath->AppendCString(newContractID) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");

        // move up the tree.
        data = predecessorData;
    }

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
    const char *cContractID = contractID.get();

    nsIComponentManager *comMgr;
    rv = NS_GetGlobalComponentManager(&comMgr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamConverter> converter(do_CreateInstance(cContractID, &rv));
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
        nsCOMPtr<nsIInputStream> dataToConvert = aFromStream;
        nsCOMPtr<nsIInputStream> convertedData;

        for (PRInt32 i = edgeCount-1; i >= 0; i--) {
            nsCString *contractIDStr = converterChain->CStringAt(i);
            if (!contractIDStr) {
                delete converterChain;
                return NS_ERROR_FAILURE;
            }
            const char *lContractID = contractIDStr->get();

            converter = do_CreateInstance(lContractID, &rv);

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

            PRUnichar *fromUni = ToNewUnicode(fromStr);
            if (!fromUni) {
                delete converterChain;
                return NS_ERROR_OUT_OF_MEMORY;
            }
            PRUnichar *toUni   = ToNewUnicode(toStr);
            if (!toUni) {
                delete fromUni;
                delete converterChain;
                return NS_ERROR_OUT_OF_MEMORY;
            }
            rv = converter->Convert(dataToConvert, fromUni, toUni, aContext, getter_AddRefs(convertedData));
            nsMemory::Free(fromUni);
            nsMemory::Free(toUni);
            dataToConvert = convertedData;
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }
        }

        delete converterChain;
        *_retval = convertedData;
        NS_ADDREF(*_retval);

    } else {
        // we're going direct.
        rv = converter->Convert(aFromStream, aFromType, aToType, aContext, _retval);
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
    const char *cContractID = contractID.get();

    nsIComponentManager *comMgr;
    rv = NS_GetGlobalComponentManager(&comMgr);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIStreamConverter> listener(do_CreateInstance(cContractID, &rv));
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

        // aListener is the listener that wants the final, converted, data.
        // we initialize finalListener w/ aListener so it gets put at the 
        // tail end of the chain, which in the loop below, means the *first*
        // converter created.
        nsCOMPtr<nsIStreamListener> finalListener = aListener;

        // convert the stream using each edge of the graph as a step.
        // this is our stream conversion traversal.
        PRInt32 edgeCount = converterChain->Count();
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");
        for (int i = 0; i < edgeCount; i++) {
            nsCString *contractIDStr = converterChain->CStringAt(i);
            if (!contractIDStr) {
                delete converterChain;
                return NS_ERROR_FAILURE;
            }
            const char *lContractID = contractIDStr->get();

            // create the converter for this from/to pair
            nsCOMPtr<nsIStreamConverter> converter(do_CreateInstance(lContractID, &rv));
            NS_ASSERTION(NS_SUCCEEDED(rv), "graph construction problem, built a contractid that wasn't registered");

            nsCString fromStr, toStr;
            rv = ParseFromTo(lContractID, fromStr, toStr);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            PRUnichar *fromStrUni = ToNewUnicode(fromStr);
            if (!fromStrUni) {
                delete converterChain;
                return NS_ERROR_OUT_OF_MEMORY;
            }

            PRUnichar *toStrUni   = ToNewUnicode(toStr);
            if (!toStrUni) {
                delete fromStrUni;
                delete converterChain;
                return NS_ERROR_OUT_OF_MEMORY;
            }

            // connect the converter w/ the listener that should get the converted data.
            rv = converter->AsyncConvertData(fromStrUni, toStrUni, finalListener, aContext);
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

            // the last iteration of this loop will result in finalListener
            // pointing to the converter that "starts" the conversion chain.
            // this converter's "from" type is the original "from" type. Prior
            // to the last iteration, finalListener will continuously be wedged
            // into the next listener in the chain, then be updated.
            finalListener = chainListener;
        }
        delete converterChain;
        // return the first listener in the chain.
        *_retval = finalListener;
        NS_ADDREF(*_retval);

    } else {
        // we're going direct.
        *_retval = listener;
        NS_ADDREF(*_retval);

        rv = listener->AsyncConvertData(aFromType, aToType, aListener, aContext);
    }
    
    return rv;

}

nsresult
NS_NewStreamConv(nsStreamConverterService** aStreamConv)
{
    NS_PRECONDITION(aStreamConv != nsnull, "null ptr");
    if (!aStreamConv) return NS_ERROR_NULL_POINTER;

    *aStreamConv = new nsStreamConverterService();
    if (!*aStreamConv) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aStreamConv);
    return (*aStreamConv)->Init();
}
