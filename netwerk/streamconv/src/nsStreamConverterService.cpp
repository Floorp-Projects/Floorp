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

////////////////////////////////////////////////////////////
// nsISupports methods
NS_IMPL_ISUPPORTS(nsStreamConverterService, nsCOMTypeInfo<nsIStreamConverterService>::GetIID());


////////////////////////////////////////////////////////////
// nsIStreamConverterService methods

// Builds the graph represented as an adjacency list (and built up in 
// memory using an nsHashtable and nsVoidArray combination).
//
// RegisterConverter takes the given PROGID and parses a single edge from it.
// An edge in this case is comprised of a FROM and TO MIME type combination. The
// edge is then inserted into the service's adjacency list. 
// It then turns around and registers the PROGID and the converter with the service
// manager; the true registration mechanism.
// 
// PROGID format:
// component://netscape/strmconv?from=text/html?to=text/plain
// XXX curently we only handle a single from and to combo, we should repeat the 
// XXX registration process for any series of from-to combos.
// XXX can use nsTokenizer for this.
//

typedef struct _tableData {
    nsHashKey *key;
    nsString2 *keyString;
    void      *data;
} tableData;

NS_IMETHODIMP
nsStreamConverterService::RegisterConverter(const char *aProgID, nsISupports *aConverter) {
    NS_ASSERTION(aProgID, "progid required");
    nsresult rv;

    // first parse out the FROM and TO MIME-types being registered.

    nsString2 ProgIDStr(aProgID, eOneByte);
    PRInt32 fromLoc = ProgIDStr.Find("from=") + 5;
    PRInt32 toLoc   = ProgIDStr.Find("to=") + 3;
    if (-1 == fromLoc || -1 == toLoc ) return NS_ERROR_FAILURE;

    nsString2 fromStr(eOneByte), toStr(eOneByte);
    PRInt32 fromLen = toLoc - 4 - fromLoc;
    PRInt32 toLen = ProgIDStr.Length() - toLoc;

    ProgIDStr.Mid(fromStr, fromLoc, fromLen);
    ProgIDStr.Mid(toStr, toLoc, toLen);


    // Each MIME-type is a vertex in the graph, so first lets make sure
    // each MIME-type is represented as a key in our hashtable.

    nsStringKey *fromKey = new nsStringKey(fromStr.GetBuffer());
    nsStringKey *toKey = new nsStringKey(toStr.GetBuffer());

    tableData *edges = (tableData*)mAdjacencyList->Get(fromKey);
    if (!edges) {
        // There is no fromStr vertex, create one.
        tableData *data = new tableData;
        if (!data) return NS_ERROR_OUT_OF_MEMORY;
        data->key = fromKey;
        data->keyString = new nsString2(fromStr.GetBuffer());
        data->data = new nsVoidArray();
        if (!data->data) return NS_ERROR_OUT_OF_MEMORY;
        mAdjacencyList->Put(fromKey, data);
    }

    edges = (tableData*)mAdjacencyList->Get(toKey);
    if (!edges) {
        // There is no toStr vertex, create one.
        tableData *data = new tableData;
        if (!data) return NS_ERROR_OUT_OF_MEMORY;
        data->key = toKey;
        data->keyString = new nsString2(toStr.GetBuffer());
        data->data = new nsVoidArray();
        if (!data->data) return NS_ERROR_OUT_OF_MEMORY;
        mAdjacencyList->Put(toKey, data);
    }

    
    // Now we know the FROM and TO types are represented as keys in the hashtable.
    // Let's "connect" the verticies, making an edge.

    edges = (tableData*)mAdjacencyList->Get(fromKey);
    NS_ASSERTION(edges, "something wrong in adjacency list construction");
    nsIAtom *vertex = NS_NewAtom(toStr);
    if (!vertex) return NS_ERROR_OUT_OF_MEMORY;
    nsVoidArray *adjacencyList = (nsVoidArray*)edges->data;
    adjacencyList->AppendElement(vertex);


    // We're done building our adjacency list, let's move onto the service 
    // manager registration.

    rv = nsServiceManager::RegisterService(aProgID, aConverter);
    return rv;
}

////////////////////////////////////////////////////////////
// nsStreamConverterService methods
nsStreamConverterService::nsStreamConverterService() {
    NS_INIT_REFCNT();
}

nsStreamConverterService::~nsStreamConverterService() {
    //mAdjacencyList->Enumerate(DeleteEntry, nsnull);
    //mAdjacencyList->Reset();
    delete mAdjacencyList;
}

nsresult
nsStreamConverterService::Init() {
    mAdjacencyList = new nsHashtable();
    if (!mAdjacencyList) return NS_ERROR_OUT_OF_MEMORY;
    return NS_OK;
}


enum colors {white, gray, black};

typedef struct _BFSState {
    colors      color;
    PRInt32     distance;
    nsHashKey  *predecessor;
} BFSState;

// nsHashtable enumerator functions.

// Initializes the color table.
PRBool InitBFSTable(nsHashKey *aKey, void *aData, void* closure) {
    nsHashtable *BFSTable = (nsHashtable*)closure;
    if (!BFSTable) return PR_FALSE;

    BFSState *state = new BFSState;
    if (!state) return NS_ERROR_OUT_OF_MEMORY;

    state->color = white;
    state->distance = -1;
    state->predecessor = nsnull;

    tableData *data = new tableData;
    if (!data) return NS_ERROR_OUT_OF_MEMORY;
    data->key = aKey->Clone();

    tableData *origData = (tableData*)aData;
    NS_ASSERTION(origData, "no data in the table enumeration");
    data->keyString = new nsString2(*origData->keyString);
    data->data = state;

    BFSTable->Put(aKey, data);
    return PR_TRUE;   
};

nsresult
nsStreamConverterService::FindConverter(const char *aProgID, PRBool direct, nsCID *aCID, nsVoidArray **aEdgeList) {


    // FIRST. try a direct lookup.

    // Direct lookup didn't succeed, walk the graph.
    // walk the graph in search of the appropriate converter.

    PRInt32 vertexCount = mAdjacencyList->Count();
    if (0 >= vertexCount)
        return NS_ERROR_FAILURE;

    // Create a corresponding color table for each vertex in the graph.
    nsHashtable lBFSTable;
    mAdjacencyList->Enumerate(InitBFSTable, &lBFSTable);

    NS_ASSERTION(lBFSTable.Count() == vertexCount, "strmconv BFS table init problem");

    // This is our source vertex; our starting point.
    nsStringKey *source = new nsStringKey(aProgID);
    if (!source) return NS_ERROR_OUT_OF_MEMORY;

    tableData *data = (tableData*)lBFSTable.Get(source);
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
        nsHashKey *currentHead = (nsHashKey*)grayQ->Pop();
        tableData *data = (tableData*)mAdjacencyList->Get(currentHead);
        nsVoidArray *edges = (nsVoidArray*)data->data;
        NS_ASSERTION(edges, "something went wrong with BFS strmconv algorithm");

        // Get the state of the current head to calculate the distance of each
        // reachable vertex in the loop.
        data = (tableData*)lBFSTable.Get(currentHead);
        BFSState *headVertexState = (BFSState*)data->data;
        NS_ASSERTION(headVertexState, "problem with the BFS strmconv algorithm");

        PRInt32 edgeCount = edges->Count();
        for (int i = 0; i < edgeCount; i++) {
            
            nsIAtom *curVertexAtom = (nsIAtom*)edges->ElementAt(i);
            nsString2 curVertexStr;
            curVertexAtom->ToString(curVertexStr);
            char * curVertexCString = curVertexStr.ToNewCString();
            nsStringKey *curVertex = new nsStringKey(curVertexCString);
            delete [] curVertexCString;

            tableData *data = (tableData*)lBFSTable.Get(curVertex);
            BFSState *curVertexState = (BFSState*)data->data;
            NS_ASSERTION(curVertexState, "something went wrong with the BFS strmconv algorithm");
            if (white == curVertexState->color) {
                curVertexState->color = gray;
                curVertexState->distance = headVertexState->distance + 1;
                curVertexState->predecessor = currentHead->Clone();
                grayQ->Push(curVertex);
            }
        }
        headVertexState->color = black;
        grayQ->Pop();
    }
    // The shortest path (if any) has been generated and is represetned by the chain of 
    // BFSState->predecessor keys. Start at the bottom and work our way up.

    // first parse out the FROM and TO MIME-types being registered.

    nsString2 ProgIDStr(aProgID, eOneByte);
    PRInt32 fromLoc = ProgIDStr.Find("from=") + 5;
    PRInt32 toLoc   = ProgIDStr.Find("to=") + 3;
    if (-1 == fromLoc || -1 == toLoc ) return NS_ERROR_FAILURE;

    nsString2 fromStr(eOneByte), toStr(eOneByte);
    PRInt32 fromLen = toLoc - 4 - fromLoc;
    PRInt32 toLen = ProgIDStr.Length() - toLoc;

    ProgIDStr.Mid(fromStr, fromLoc, fromLen);
    ProgIDStr.Mid(toStr, toLoc, toLen);


    // get the root PROGID
    nsString2 ProgIDPrefix(ProgIDStr);
    ProgIDPrefix.Truncate(fromLoc - 5);
    nsVoidArray *shortestPath = new nsVoidArray();
    nsStringKey *toMIMEType = new nsStringKey(toStr);
    data = (tableData*)lBFSTable.Get(toMIMEType);
    NS_ASSERTION(data, "problem w/ strmconv BFS algorithm");

    while (data) {
        BFSState *curState = (BFSState*)data->data;

        if (data->keyString->Equals(fromStr)) {
            // found it. We're done here.
            direct = PR_FALSE;
            *aEdgeList = shortestPath;
            return NS_OK;
        }

        // reconstruct the PROGID.
        // Get the predecessor.
        tableData *predecessorData = (tableData*)lBFSTable.Get(curState->predecessor);
        NS_ASSERTION(predecessorData, "can't get predecessor data");

        // build out the PROGID.
        nsString2 *newProgID = new nsString2(ProgIDPrefix);
        newProgID->Append("from=");

        char *from = predecessorData->keyString->ToNewCString();
        newProgID->Append(from);
        delete [] from;

        newProgID->Append("?to=");
        char *to = data->keyString->ToNewCString();
        newProgID->Append(to);
        delete [] to;
    
        // Add this PROGID to the chain.
        shortestPath->AppendElement(newProgID);

        // move up the tree.
        data = predecessorData;
    }

    return NS_ERROR_FAILURE; // couldn't find a stream converter or chain.

}

NS_IMETHODIMP
nsStreamConverterService::OnDataAvailable(nsIChannel *channel, nsISupports *ctxt, nsIInputStream *inStr, PRUint32 sourceOffset, PRUint32 count) {
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsStreamConverterService::OnStartRequest(nsIChannel *channel, nsISupports *ctxt) {
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsStreamConverterService::OnStopRequest(nsIChannel *channel, nsISupports *ctxt, nsresult status, const PRUnichar *errorMsg) {
    return NS_ERROR_NOT_IMPLEMENTED;
}
