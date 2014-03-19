/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
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
#include "nsIComponentRegistrar.h"
#include "nsString.h"
#include "nsIAtom.h"
#include "nsDeque.h"
#include "nsIInputStream.h"
#include "nsIStreamConverter.h"
#include "nsICategoryManager.h"
#include "nsXPCOM.h"
#include "nsISupportsPrimitives.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsServiceManagerUtils.h"
#include "nsHashtable.h"
#include "nsISimpleEnumerator.h"

///////////////////////////////////////////////////////////////////
// Breadth-First-Search (BFS) algorithm state classes and types.

// used  to establish discovered vertecies.
enum BFScolors {white, gray, black};

struct BFSState {
    BFScolors   color;
    int32_t     distance;
    nsCStringKey  *predecessor;
    ~BFSState() {
        delete predecessor;
    }
};

// Adjacency list data class.
typedef nsCOMArray<nsIAtom> SCTableData;

// Delete all the entries in the adjacency list
static bool DeleteAdjacencyEntry(nsHashKey *aKey, void *aData, void* closure) {
    SCTableData *entry = (SCTableData*)aData;
    delete entry;
    return true;
}

// BFS hashtable data class.
struct BFSTableData {
    nsCStringKey *key;
    BFSState *state;

    BFSTableData(nsCStringKey* aKey) : key(aKey), state(nullptr)
    {
    }
};

////////////////////////////////////////////////////////////
// nsISupports methods
NS_IMPL_ISUPPORTS1(nsStreamConverterService, nsIStreamConverterService)


////////////////////////////////////////////////////////////
// nsIStreamConverterService methods

////////////////////////////////////////////////////////////
// nsStreamConverterService methods
nsStreamConverterService::nsStreamConverterService()
  : mAdjacencyList(nullptr, nullptr, DeleteAdjacencyEntry, nullptr)
{
}

nsStreamConverterService::~nsStreamConverterService() {
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
    nsCOMPtr<nsISupports> supports;
    nsCOMPtr<nsISupportsCString> entry;
    rv = entries->GetNext(getter_AddRefs(supports));
    while (NS_SUCCEEDED(rv)) {
        entry = do_QueryInterface(supports);

        // get the entry string
        nsAutoCString entryString;
        rv = entry->GetData(entryString);
        if (NS_FAILED(rv)) return rv;

        // cobble the entry string w/ the converter key to produce a full contractID.
        nsAutoCString contractID(NS_ISTREAMCONVERTER_KEY);
        contractID.Append(entryString);

        // now we've got the CONTRACTID, let's parse it up.
        rv = AddAdjacency(contractID.get());
        if (NS_FAILED(rv)) return rv;

        rv = entries->GetNext(getter_AddRefs(supports));
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

    nsAutoCString fromStr, toStr;
    rv = ParseFromTo(aContractID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // Each MIME-type is a vertex in the graph, so first lets make sure
    // each MIME-type is represented as a key in our hashtable.

    nsCStringKey fromKey(fromStr);
    SCTableData *fromEdges = (SCTableData*)mAdjacencyList.Get(&fromKey);
    if (!fromEdges) {
        // There is no fromStr vertex, create one.

        nsCStringKey *newFromKey = new nsCStringKey(ToNewCString(fromStr), fromStr.Length(), nsCStringKey::OWN);
        fromEdges = new SCTableData();
        mAdjacencyList.Put(newFromKey, fromEdges);
    }

    nsCStringKey toKey(toStr);
    if (!mAdjacencyList.Get(&toKey)) {
        // There is no toStr vertex, create one.
        nsCStringKey *newToKey = new nsCStringKey(ToNewCString(toStr), toStr.Length(), nsCStringKey::OWN);
        mAdjacencyList.Put(newToKey, new SCTableData());
    }

    // Now we know the FROM and TO types are represented as keys in the hashtable.
    // Let's "connect" the verticies, making an edge.

    nsCOMPtr<nsIAtom> vertex = do_GetAtom(toStr); 
    if (!vertex) return NS_ERROR_OUT_OF_MEMORY;

    NS_ASSERTION(fromEdges, "something wrong in adjacency list construction");
    if (!fromEdges)
        return NS_ERROR_FAILURE;

    return fromEdges->AppendObject(vertex) ? NS_OK : NS_ERROR_FAILURE;
}

nsresult
nsStreamConverterService::ParseFromTo(const char *aContractID, nsCString &aFromRes, nsCString &aToRes) {

    nsAutoCString ContractIDStr(aContractID);

    int32_t fromLoc = ContractIDStr.Find("from=");
    int32_t toLoc   = ContractIDStr.Find("to=");
    if (-1 == fromLoc || -1 == toLoc ) return NS_ERROR_FAILURE;

    fromLoc = fromLoc + 5;
    toLoc = toLoc + 3;

    nsAutoCString fromStr, toStr;

    ContractIDStr.Mid(fromStr, fromLoc, toLoc - 4 - fromLoc);
    ContractIDStr.Mid(toStr, toLoc, ContractIDStr.Length() - toLoc);

    aFromRes.Assign(fromStr);
    aToRes.Assign(toStr);

    return NS_OK;
}

// nsObjectHashtable enumerator functions.

// Initializes the BFS state table.
static bool InitBFSTable(nsHashKey *aKey, void *aData, void* closure) {
    NS_ASSERTION((SCTableData*)aData, "no data in the table enumeration");

    nsHashtable *BFSTable = (nsHashtable*)closure;
    if (!BFSTable) return false;

    BFSState *state = new BFSState;

    state->color = white;
    state->distance = -1;
    state->predecessor = nullptr;

    BFSTableData *data = new BFSTableData(static_cast<nsCStringKey*>(aKey));
    data->state = state;

    BFSTable->Put(aKey, data);
    return true;
}

// cleans up the BFS state table
static bool DeleteBFSEntry(nsHashKey *aKey, void *aData, void *closure) {
    BFSTableData *data = (BFSTableData*)aData;
    BFSState *state = data->state;
    delete state;
    data->key = nullptr;
    delete data;
    return true;
}

class CStreamConvDeallocator : public nsDequeFunctor {
public:
    virtual void* operator()(void* anObject) {
        nsCStringKey *key = (nsCStringKey*)anObject;
        delete key;
        return 0;
    }
};

// walks the graph using a breadth-first-search algorithm which generates a discovered
// verticies tree. This tree is then walked up (from destination vertex, to origin vertex)
// and each link in the chain is added to an nsStringArray. A direct lookup for the given
// CONTRACTID should be made prior to calling this method in an attempt to find a direct
// converter rather than walking the graph.
nsresult
nsStreamConverterService::FindConverter(const char *aContractID, nsTArray<nsCString> **aEdgeList) {
    nsresult rv;
    if (!aEdgeList) return NS_ERROR_NULL_POINTER;
    *aEdgeList = nullptr;

    // walk the graph in search of the appropriate converter.

    int32_t vertexCount = mAdjacencyList.Count();
    if (0 >= vertexCount) return NS_ERROR_FAILURE;

    // Create a corresponding color table for each vertex in the graph.
    nsObjectHashtable lBFSTable(nullptr, nullptr, DeleteBFSEntry, nullptr);
    mAdjacencyList.Enumerate(InitBFSTable, &lBFSTable);

    NS_ASSERTION(lBFSTable.Count() == vertexCount, "strmconv BFS table init problem");

    // This is our source vertex; our starting point.
    nsAutoCString fromC, toC;
    rv = ParseFromTo(aContractID, fromC, toC);
    if (NS_FAILED(rv)) return rv;

    nsCStringKey *source = new nsCStringKey(fromC.get());

    BFSTableData *data = (BFSTableData*)lBFSTable.Get(source);
    if (!data) {
        delete source;
        return NS_ERROR_FAILURE;
    }

    BFSState *state = data->state;

    state->color = gray;
    state->distance = 0;
    CStreamConvDeallocator *dtorFunc = new CStreamConvDeallocator();

    nsDeque grayQ(dtorFunc);

    // Now generate the shortest path tree.
    grayQ.Push(source);
    while (0 < grayQ.GetSize()) {
        nsCStringKey *currentHead = (nsCStringKey*)grayQ.PeekFront();
        SCTableData *data2 = (SCTableData*)mAdjacencyList.Get(currentHead);
        if (!data2) return NS_ERROR_FAILURE;

        // Get the state of the current head to calculate the distance of each
        // reachable vertex in the loop.
        BFSTableData *data2b = (BFSTableData*)lBFSTable.Get(currentHead);
        if (!data2b) return NS_ERROR_FAILURE;

        BFSState *headVertexState = data2b->state;
        NS_ASSERTION(headVertexState, "problem with the BFS strmconv algorithm");
        if (!headVertexState) return NS_ERROR_FAILURE;

        int32_t edgeCount = data2->Count();

        for (int32_t i = 0; i < edgeCount; i++) {
            nsIAtom* curVertexAtom = data2->ObjectAt(i);
            nsAutoString curVertexStr;
            curVertexAtom->ToString(curVertexStr);
            nsCStringKey *curVertex = new nsCStringKey(ToNewCString(curVertexStr), 
                                        curVertexStr.Length(), nsCStringKey::OWN);

            BFSTableData *data3 = (BFSTableData*)lBFSTable.Get(curVertex);
            if (!data3) {
                delete curVertex;
                return NS_ERROR_FAILURE;
            }
            BFSState *curVertexState = data3->state;
            NS_ASSERTION(curVertexState, "something went wrong with the BFS strmconv algorithm");
            if (!curVertexState) return NS_ERROR_FAILURE;

            if (white == curVertexState->color) {
                curVertexState->color = gray;
                curVertexState->distance = headVertexState->distance + 1;
                curVertexState->predecessor = (nsCStringKey*)currentHead->Clone();
                if (!curVertexState->predecessor) {
                    delete curVertex;
                    return NS_ERROR_OUT_OF_MEMORY;
                }
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
        cur = nullptr;
    }
    // The shortest path (if any) has been generated and is represetned by the chain of 
    // BFSState->predecessor keys. Start at the bottom and work our way up.

    // first parse out the FROM and TO MIME-types being registered.

    nsAutoCString fromStr, toStr;
    rv = ParseFromTo(aContractID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // get the root CONTRACTID
    nsAutoCString ContractIDPrefix(NS_ISTREAMCONVERTER_KEY);
    nsTArray<nsCString> *shortestPath = new nsTArray<nsCString>();

    nsCStringKey toMIMEType(toStr);
    data = (BFSTableData*)lBFSTable.Get(&toMIMEType);
    if (!data) {
        // If this vertex isn't in the BFSTable, then no-one has registered for it,
        // therefore we can't do the conversion.
        delete shortestPath;
        return NS_ERROR_FAILURE;
    }

    while (data) {
        BFSState *curState = data->state;

        nsCStringKey *key = data->key;

        if (fromStr.Equals(key->GetString())) {
            // found it. We're done here.
            *aEdgeList = shortestPath;
            return NS_OK;
        }

        // reconstruct the CONTRACTID.
        // Get the predecessor.
        if (!curState->predecessor) break; // no predecessor
        BFSTableData *predecessorData = (BFSTableData*)lBFSTable.Get(curState->predecessor);

        if (!predecessorData) break; // no predecessor, chain doesn't exist.

        // build out the CONTRACTID.
        nsAutoCString newContractID(ContractIDPrefix);
        newContractID.AppendLiteral("?from=");

        nsCStringKey *predecessorKey = predecessorData->key;
        newContractID.Append(predecessorKey->GetString());

        newContractID.AppendLiteral("&to=");
        newContractID.Append(key->GetString());

        // Add this CONTRACTID to the chain.
        rv = shortestPath->AppendElement(newContractID) ? NS_OK : NS_ERROR_FAILURE;  // XXX this method incorrectly returns a bool
        NS_ASSERTION(NS_SUCCEEDED(rv), "AppendElement failed");

        // move up the tree.
        data = predecessorData;
    }
    delete shortestPath;
    return NS_ERROR_FAILURE; // couldn't find a stream converter or chain.
}


/////////////////////////////////////////////////////
// nsIStreamConverterService methods
NS_IMETHODIMP
nsStreamConverterService::CanConvert(const char* aFromType,
                                     const char* aToType,
                                     bool* _retval) {
    nsCOMPtr<nsIComponentRegistrar> reg;
    nsresult rv = NS_GetComponentRegistrar(getter_AddRefs(reg));
    if (NS_FAILED(rv))
        return rv;

    nsAutoCString contractID;
    contractID.AssignLiteral(NS_ISTREAMCONVERTER_KEY "?from=");
    contractID.Append(aFromType);
    contractID.AppendLiteral("&to=");
    contractID.Append(aToType);

    // See if we have a direct match
    rv = reg->IsContractIDRegistered(contractID.get(), _retval);
    if (NS_FAILED(rv))
        return rv;
    if (*_retval)
        return NS_OK;

    // Otherwise try the graph.
    rv = BuildGraph();
    if (NS_FAILED(rv))
        return rv;

    nsTArray<nsCString> *converterChain = nullptr;
    rv = FindConverter(contractID.get(), &converterChain);
    *_retval = NS_SUCCEEDED(rv);

    delete converterChain;
    return NS_OK;
}

NS_IMETHODIMP
nsStreamConverterService::Convert(nsIInputStream *aFromStream,
                                  const char *aFromType, 
                                  const char *aToType,
                                  nsISupports *aContext,
                                  nsIInputStream **_retval) {
    if (!aFromStream || !aFromType || !aToType || !_retval) return NS_ERROR_NULL_POINTER;
    nsresult rv;

    // first determine whether we can even handle this conversion
    // build a CONTRACTID
    nsAutoCString contractID;
    contractID.AssignLiteral(NS_ISTREAMCONVERTER_KEY "?from=");
    contractID.Append(aFromType);
    contractID.AppendLiteral("&to=");
    contractID.Append(aToType);
    const char *cContractID = contractID.get();

    nsCOMPtr<nsIStreamConverter> converter(do_CreateInstance(cContractID, &rv));
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) return rv;

        nsTArray<nsCString> *converterChain = nullptr;

        rv = FindConverter(cContractID, &converterChain);
        if (NS_FAILED(rv)) {
            // can't make this conversion.
            // XXX should have a more descriptive error code.
            return NS_ERROR_FAILURE;
        }

        int32_t edgeCount = int32_t(converterChain->Length());
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");


        // convert the stream using each edge of the graph as a step.
        // this is our stream conversion traversal.
        nsCOMPtr<nsIInputStream> dataToConvert = aFromStream;
        nsCOMPtr<nsIInputStream> convertedData;

        for (int32_t i = edgeCount-1; i >= 0; i--) {
            const char *lContractID = converterChain->ElementAt(i).get();

            converter = do_CreateInstance(lContractID, &rv);

            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            nsAutoCString fromStr, toStr;
            rv = ParseFromTo(lContractID, fromStr, toStr);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            rv = converter->Convert(dataToConvert, fromStr.get(), toStr.get(), aContext, getter_AddRefs(convertedData));
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
nsStreamConverterService::AsyncConvertData(const char *aFromType, 
                                           const char *aToType, 
                                           nsIStreamListener *aListener,
                                           nsISupports *aContext,
                                           nsIStreamListener **_retval) {
    if (!aFromType || !aToType || !aListener || !_retval) return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // first determine whether we can even handle this conversion
    // build a CONTRACTID
    nsAutoCString contractID;
    contractID.AssignLiteral(NS_ISTREAMCONVERTER_KEY "?from=");
    contractID.Append(aFromType);
    contractID.AppendLiteral("&to=");
    contractID.Append(aToType);
    const char *cContractID = contractID.get();

    nsCOMPtr<nsIStreamConverter> listener(do_CreateInstance(cContractID, &rv));
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) return rv;

        nsTArray<nsCString> *converterChain = nullptr;

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
        int32_t edgeCount = int32_t(converterChain->Length());
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");
        for (int i = 0; i < edgeCount; i++) {
            const char *lContractID = converterChain->ElementAt(i).get();

            // create the converter for this from/to pair
            nsCOMPtr<nsIStreamConverter> converter(do_CreateInstance(lContractID));
            NS_ASSERTION(converter, "graph construction problem, built a contractid that wasn't registered");

            nsAutoCString fromStr, toStr;
            rv = ParseFromTo(lContractID, fromStr, toStr);
            if (NS_FAILED(rv)) {
                delete converterChain;
                return rv;
            }

            // connect the converter w/ the listener that should get the converted data.
            rv = converter->AsyncConvertData(fromStr.get(), toStr.get(), finalListener, aContext);
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
    NS_PRECONDITION(aStreamConv != nullptr, "null ptr");
    if (!aStreamConv) return NS_ERROR_NULL_POINTER;

    *aStreamConv = new nsStreamConverterService();
    NS_ADDREF(*aStreamConv);

    return NS_OK;
}
