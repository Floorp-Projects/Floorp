/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "nsStreamConverterService.h"
#include "nsIServiceManager.h"
#include "nsString2.h"
#include "nsIAtom.h"
#include "nsDeque.h"
#include "nsIRegistry.h"
#include "nsIEnumerator.h"
#include "nsIBufferInputStream.h"
#include "nsIBufferOutputStream.h"
#include "nsIStreamConverter.h"
#include "nsIComponentManager.h"

////////////////////////////////////////////////////////////
// nsISupports methods
NS_IMPL_ISUPPORTS(nsStreamConverterService, nsCOMTypeInfo<nsIStreamConverterService>::GetIID());


////////////////////////////////////////////////////////////
// nsIStreamConverterService methods

////////////////////////////////////////////////////////////
// nsStreamConverterService methods
nsStreamConverterService::nsStreamConverterService() {
    NS_INIT_REFCNT();
    mAdjacencyList = nsnull;
}

// Delete all the entries in the adjacency list
PRBool DeleteAdjacencyEntry(nsHashKey *aKey, void *aData, void* closure) {
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
    delete entry->data;
    delete entry;
    return PR_TRUE;   
};

nsStreamConverterService::~nsStreamConverterService() {
    // Clean up the adjacency list table.
    mAdjacencyList->Enumerate(DeleteAdjacencyEntry, nsnull);
    mAdjacencyList->Reset();
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
// :BuildGraph() consults the registry for all stream converter PROGIDS then fills the
// adjacency list with edges.
// An edge in this case is comprised of a FROM and TO MIME type combination.
// 
// PROGID format:
// component://netscape/strmconv?from=text/html?to=text/plain
// XXX curently we only handle a single from and to combo, we should repeat the 
// XXX registration process for any series of from-to combos.
// XXX can use nsTokenizer for this.
//

nsresult
nsStreamConverterService::BuildGraph() {

    nsresult rv;
    // enumerate the registry subkeys
    nsIRegistry *registry = nsnull;
    nsIRegistry::Key key;
    nsIEnumerator *components = nsnull;
    rv = nsServiceManager::GetService(NS_REGISTRY_PROGID,
                                      nsCOMTypeInfo<nsIRegistry>::GetIID(),
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
    while (NS_SUCCEEDED(rv) && !components->IsDone()) {
        nsISupports *base = nsnull;

        rv = components->CurrentItem(&base);
        if (NS_FAILED(rv)) return rv;

        nsIRegistryNode *node = nsnull;
        nsIID nodeIID = NS_IREGISTRYNODE_IID;
        rv = base->QueryInterface(nodeIID, (void**)&node);
        if (NS_FAILED(rv)) return rv;

        char *name = nsnull;
        rv = node->GetName(&name);
        if (NS_FAILED(rv)) return rv;

        nsString2 actualProgID(NS_ISTREAMCONVERTER_KEY, eOneByte);
        actualProgID.Append(name);

        // now we've got the PROGID, let's parse it up.
        rv = AddAdjacency(actualProgID.GetBuffer());

        // cleanup
        nsCRT::free(name);
        NS_RELEASE(node);
        NS_RELEASE(base);
        rv = components->Next();
    }

    registry->Close();
    NS_IF_RELEASE( components );
    nsServiceManager::ReleaseService( NS_REGISTRY_PROGID, registry );

    return NS_OK;
}


// XXX currently you can not add the same adjacency (i.e. you can't have multiple
// XXX stream converters registering to handle the same from-to combination. It's
// XXX not programatically prohibited, it's just that results are un-predictable
// XXX right now.
nsresult
nsStreamConverterService::AddAdjacency(const char *aProgID) {
    nsresult rv;
    // first parse out the FROM and TO MIME-types.

    nsString2 fromStr(eOneByte), toStr(eOneByte);
    rv = ParseFromTo(aProgID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // Each MIME-type is a vertex in the graph, so first lets make sure
    // each MIME-type is represented as a key in our hashtable.

    PRBool delFrom = PR_TRUE, delTo = PR_TRUE;
    nsStringKey *fromKey = new nsStringKey(fromStr.GetBuffer());
    if (!fromKey) return NS_ERROR_OUT_OF_MEMORY;
    nsStringKey *toKey = new nsStringKey(toStr.GetBuffer());
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
        data->keyString = new nsString2(fromStr.GetBuffer(), eOneByte);
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
        data->keyString = new nsString2(toStr.GetBuffer(), eOneByte);
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
    adjacencyList->AppendElement(vertex);

    return NS_OK;
}

nsresult
nsStreamConverterService::ParseFromTo(const char *aProgID, nsString2 &aFromRes, nsString2 &aToRes) {

    nsString2 ProgIDStr(aProgID, eOneByte);

    PRInt32 fromLoc = ProgIDStr.Find("from=") + 5;
    PRInt32 toLoc   = ProgIDStr.Find("to=") + 3;
    if (-1 == fromLoc || -1 == toLoc ) return NS_ERROR_FAILURE;

    nsString2 fromStr(eOneByte), toStr(eOneByte);

    ProgIDStr.Mid(fromStr, fromLoc, toLoc - 4 - fromLoc);
    ProgIDStr.Mid(toStr, toLoc, ProgIDStr.Length() - toLoc);

    aFromRes.SetString(fromStr);
    aToRes.SetString(toStr);

    return NS_OK;
}

// nsHashtable enumerator functions.

// Initializes the BFS state table.
PRBool InitBFSTable(nsHashKey *aKey, void *aData, void* closure) {
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
    data->keyString = new nsString2(*origData->keyString, eOneByte);
    data->data = state;

    BFSTable->Put(aKey, data);
    return PR_TRUE;   
};

// cleans up the BFS state table
PRBool DeleteBFSEntry(nsHashKey *aKey, void *aData, void *closure) {
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
// and each link in the chain is added to an nsVoidArray. A direct lookup for the given
// PROGID should be made prior to calling this method in an attempt to find a direct
// converter rather than walking the graph.
nsresult
nsStreamConverterService::FindConverter(const char *aProgID, nsVoidArray **aEdgeList) {
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
    nsString2 from(eOneByte), to(eOneByte);
    rv = ParseFromTo(aProgID, from, to);
    if (NS_FAILED(rv)) return rv;

    nsStringKey *source = new nsStringKey(from.GetBuffer());
    if (!source) return NS_ERROR_OUT_OF_MEMORY;

    SCTableData *data = (SCTableData*)lBFSTable.Get(source);
    BFSState *state = (BFSState*)data->data;
    // XXX probably don't need this check.
    if (!state)
        return NS_ERROR_FAILURE;

    state->color = gray;
    state->distance = 0;
    nsDeque *grayQ = new nsDeque(0);

    // Now generate the shortest path tree.
    grayQ->Push(source);
    while (0 < grayQ->GetSize()) {
        nsHashKey *currentHead = (nsHashKey*)grayQ->Peek();
        SCTableData *data = (SCTableData*)mAdjacencyList->Get(currentHead);
        nsVoidArray *edges = (nsVoidArray*)data->data;
        NS_ASSERTION(edges, "something went wrong with BFS strmconv algorithm");

        // Get the state of the current head to calculate the distance of each
        // reachable vertex in the loop.
        data = (SCTableData*)lBFSTable.Get(currentHead);
        BFSState *headVertexState = (BFSState*)data->data;
        NS_ASSERTION(headVertexState, "problem with the BFS strmconv algorithm");

        PRInt32 edgeCount = edges->Count();
        for (int i = 0; i < edgeCount; i++) {
            
            nsIAtom *curVertexAtom = (nsIAtom*)edges->ElementAt(i);
            nsString2 curVertexStr(eOneByte);
            curVertexAtom->ToString(curVertexStr);
            char * curVertexCString = curVertexStr.ToNewCString();
            nsStringKey *curVertex = new nsStringKey(curVertexCString);
            nsAllocator::Free(curVertexCString);

            SCTableData *data = (SCTableData*)lBFSTable.Get(curVertex);
            BFSState *curVertexState = (BFSState*)data->data;
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
        nsStringKey *cur = (nsStringKey*)grayQ->PopFront();
        delete cur;
        cur = nsnull;
    }
    delete grayQ;
    // The shortest path (if any) has been generated and is represetned by the chain of 
    // BFSState->predecessor keys. Start at the bottom and work our way up.

    // first parse out the FROM and TO MIME-types being registered.

    nsString2 fromStr(eOneByte), toStr(eOneByte);
    rv = ParseFromTo(aProgID, fromStr, toStr);
    if (NS_FAILED(rv)) return rv;

    // get the root PROGID
    nsString2 ProgIDPrefix(NS_ISTREAMCONVERTER_KEY, eOneByte);
    nsVoidArray *shortestPath = new nsVoidArray();
    nsStringKey *toMIMEType = new nsStringKey(toStr);
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
            lBFSTable.Enumerate(DeleteBFSEntry, nsnull);
            lBFSTable.Reset();
            return NS_OK;
        }

        // reconstruct the PROGID.
        // Get the predecessor.
        SCTableData *predecessorData = (SCTableData*)lBFSTable.Get(curState->predecessor);
        if (!predecessorData) break; // no predecessor, chain doesn't exist.

        // build out the PROGID.
        nsString2 *newProgID = new nsString2(ProgIDPrefix, eOneByte);
        newProgID->Append("?from=");

        char *from = predecessorData->keyString->ToNewCString();
        newProgID->Append(from);
        nsAllocator::Free(from);

        newProgID->Append("?to=");
        char *to = data->keyString->ToNewCString();
        newProgID->Append(to);
        nsAllocator::Free(to);
    
        // Add this PROGID to the chain.
        shortestPath->AppendElement(newProgID);

        // move up the tree.
        data = predecessorData;
    }

    lBFSTable.Enumerate(DeleteBFSEntry, nsnull);
    lBFSTable.Reset();
    *aEdgeList = nsnull;
    return NS_ERROR_FAILURE; // couldn't find a stream converter or chain.

}


/////////////////////////////////////////////////////
// nsIStreamConverter methods
NS_IMETHODIMP
nsStreamConverterService::Convert(nsIInputStream *aFromStream,
                                  const PRUnichar *aFromType, 
                                  const PRUnichar *aToType, 
                                  nsIInputStream **_retval) {
    if (!aFromStream || !aFromType || !aToType || !_retval) return NS_ERROR_NULL_POINTER;
    nsresult rv;

    // first determine whether we can even handle this covnversion
    // build a PROGID
    nsString2 progID(NS_ISTREAMCONVERTER_KEY);
    progID.Append("?from=");
    progID.Append(aFromType);
    progID.Append("?to=");
    progID.Append(aToType);
    char * cProgID = progID.ToNewCString();
    if (!cProgID) return NS_ERROR_OUT_OF_MEMORY;

    nsISupports *converter = nsnull;
    rv = nsServiceManager::GetService(cProgID, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), &converter);
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) {
            nsAllocator::Free(cProgID);
            return rv;
        }

        nsVoidArray *converterChain = nsnull;

        rv = FindConverter(cProgID, &converterChain);
        nsAllocator::Free(cProgID);
        cProgID = nsnull;
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

        for (PRInt32 i = edgeCount-1; i > 0; i--) {
            nsString2 *progIDStr = (nsString2*)converterChain->ElementAt(i);
            char * lProgID = progIDStr->ToNewCString();
            const char *x = lProgID;

            nsIComponentManager *comMgr;
            rv = NS_GetGlobalComponentManager(&comMgr);
            if (NS_FAILED(rv)) return rv;


            nsCID cid;
            rv = comMgr->ProgIDToCLSID(x, &cid);
            if (!lProgID) return NS_ERROR_OUT_OF_MEMORY;
//            rv = nsComponentManager::GetService(lProgID, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), &converter);
            rv = comMgr->CreateInstance(cid,
                                                    nsnull,
                                                    nsCOMTypeInfo<nsIStreamConverter>::GetIID(),
                                                    (void**)&converter);

            //NS_ASSERTION(NS_SUCCEEDED(rv), "registration problem. someone registered a progid w/ the registry, but didn/'t register it with the component manager");
            if (NS_FAILED(rv)) {
                // clean up the array.
                nsString2 *progID;
                while ( (progID = (nsString2*)converterChain->ElementAt(0)) ) {
                    delete progID;
                    converterChain->RemoveElementAt(0);
                }
                delete converterChain;                
                return rv;
            }

            nsString2 fromStr(eOneByte), toStr(eOneByte);
            rv = ParseFromTo(lProgID, fromStr, toStr);
            nsAllocator::Free(lProgID);
            if (NS_FAILED(rv)) return rv;

            nsIStreamConverter *conv = nsnull;
            rv = converter->QueryInterface(nsCOMTypeInfo<nsIStreamConverter>::GetIID(), (void**)&conv);
            NS_RELEASE(converter);
            if (NS_FAILED(rv)) return rv;

            PRUnichar *fromUni = fromStr.ToNewUnicode();
            PRUnichar *toUni   = toStr.ToNewUnicode();
            rv = conv->Convert(dataToConvert, fromUni, toUni, nsnull, &convertedData);
            nsAllocator::Free(fromUni);
            nsAllocator::Free(toUni);
            NS_RELEASE(conv);
            NS_RELEASE(dataToConvert);
            dataToConvert = convertedData;
            if (NS_FAILED(rv)) return rv;
        }

        // clean up the array.
        nsString2 *progID;
        while ( (progID = (nsString2*)converterChain->ElementAt(0)) ) {
            delete progID;
            converterChain->RemoveElementAt(0);
        }
        delete converterChain;
        *_retval = convertedData;

    } else {
        // we're going direct.
        nsIStreamConverter *conv = nsnull;
        rv = converter->QueryInterface(nsCOMTypeInfo<nsIStreamConverter>::GetIID(), (void**)&conv);
        NS_RELEASE(converter);
        if (NS_FAILED(rv)) return rv;
        rv = conv->Convert(aFromStream, aFromType, aToType, nsnull, _retval);
        NS_RELEASE(conv);
    }
    
    return rv;
}


NS_IMETHODIMP
nsStreamConverterService::AsyncConvertData(const PRUnichar *aFromType, 
                                           const PRUnichar *aToType, 
                                           nsIStreamListener *aListener, 
                                           nsIStreamListener **_retval) {
    if (!aFromType || !aToType || !aListener || !_retval) return NS_ERROR_NULL_POINTER;

    nsresult rv;

    // first determine whether we can even handle this covnversion
    // build a PROGID
    nsString2 progID(NS_ISTREAMCONVERTER_KEY);
    progID.Append("?from=");
    progID.Append(aFromType);
    progID.Append("?to=");
    progID.Append(aToType);
    char * cProgID = progID.ToNewCString();
    if (!cProgID) return NS_ERROR_OUT_OF_MEMORY;

    nsISupports *converter = nsnull;
    rv = nsServiceManager::GetService(cProgID, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), &converter);
    if (NS_FAILED(rv)) {
        // couldn't go direct, let's try walking the graph of converters.
        rv = BuildGraph();
        if (NS_FAILED(rv)) {
            nsAllocator::Free(cProgID);
            return rv;
        }

        nsVoidArray *converterChain = nsnull;

        rv = FindConverter(cProgID, &converterChain);
        nsAllocator::Free(cProgID);
        cProgID = nsnull;
        if (NS_FAILED(rv)) {
            // can't make this conversion.
            // XXX should have a more descriptive error code.
            return NS_ERROR_FAILURE;
        }

        PRInt32 edgeCount = converterChain->Count();
        NS_ASSERTION(edgeCount > 0, "findConverter should have failed");

        // convert the stream using each edge of the graph as a step.
        // this is our stream conversion traversal.

        nsIStreamListener *forwardListener = aListener;
        nsIStreamListener *fromListener = nsnull;
        NS_ADDREF(forwardListener);

        for (int i = 0; i < edgeCount; i++) {
            nsString2 *progIDStr = (nsString2*)converterChain->ElementAt(i);
            char * lProgID = progIDStr->ToNewCString();
            if (!lProgID) return NS_ERROR_OUT_OF_MEMORY;
            rv = nsServiceManager::GetService(lProgID, nsCOMTypeInfo<nsIStreamConverter>::GetIID(), &converter);
            NS_ASSERTION(NS_SUCCEEDED(rv), "graph construction problem, built a progid that wasn't registered");

            nsString2 fromStr(eOneByte), toStr(eOneByte);
            rv = ParseFromTo(lProgID, fromStr, toStr);
            nsAllocator::Free(lProgID);
            if (NS_FAILED(rv)) return rv;

            nsIStreamConverter *conv = nsnull;
            rv = converter->QueryInterface(nsCOMTypeInfo<nsIStreamConverter>::GetIID(), (void**)&conv);
            NS_RELEASE(converter);
            if (NS_FAILED(rv)) return rv;
            rv = conv->AsyncConvertData(fromStr.GetUnicode(), toStr.GetUnicode(), nsnull, forwardListener);

            nsIStreamListener *listener = nsnull;
            rv = conv->QueryInterface(nsCOMTypeInfo<nsIStreamListener>::GetIID(), (void**)&listener);
            if (NS_FAILED(rv)) return rv;

            // store the listener of the first converter in the chain.
            if (!fromListener) {
                fromListener = listener;
                NS_ADDREF(fromListener);
            }
            NS_RELEASE(conv);
            NS_RELEASE(forwardListener);
            forwardListener = listener;
            if (NS_FAILED(rv)) return rv;
        }
        // return the first listener in the chain.
        *_retval = fromListener;

    } else {
        // we're going direct.
        nsIStreamListener *listener= nsnull;
        rv = converter->QueryInterface(nsCOMTypeInfo<nsIStreamListener>::GetIID(), (void**)&listener);
        if (NS_FAILED(rv)) return rv;
        *_retval = listener;

        nsIStreamConverter *conv = nsnull;
        rv = converter->QueryInterface(nsCOMTypeInfo<nsIStreamConverter>::GetIID(), (void**)&conv);
        NS_RELEASE(converter);
        if (NS_FAILED(rv)) return rv;

        rv = conv->AsyncConvertData(aFromType, aToType, nsnull, aListener);
        NS_RELEASE(conv);
    }
    
    return rv;

}

