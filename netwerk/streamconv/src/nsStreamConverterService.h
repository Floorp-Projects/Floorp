/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsstreamconverterservice__h___
#define __nsstreamconverterservice__h___

#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsHashtable.h"
#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsIAtom.h"

class nsStreamConverterService : public nsIStreamConverterService {
public:    
    /////////////////////////////////////////////////////
    // nsISupports methods
    NS_DECL_ISUPPORTS


    /////////////////////////////////////////////////////
    // nsIStreamConverterService methods
    NS_DECL_NSISTREAMCONVERTERSERVICE

    /////////////////////////////////////////////////////
    // nsStreamConverterService methods
    nsStreamConverterService();
    virtual ~nsStreamConverterService();

    // Initialization routine. Must be called after this object is constructed.
    nsresult Init();

private:
    // Responsible for finding a converter for the given MIME-type.
    nsresult FindConverter(const char *aContractID, nsTArray<nsCString> **aEdgeList);
    nsresult BuildGraph(void);
    nsresult AddAdjacency(const char *aContractID);
    nsresult ParseFromTo(const char *aContractID, nsCString &aFromRes, nsCString &aToRes);

    // member variables
    nsObjectHashtable              *mAdjacencyList;
};

///////////////////////////////////////////////////////////////////
// Breadth-First-Search (BFS) algorithm state classes and types.

// used  to establish discovered vertecies.
enum BFScolors {white, gray, black};

struct BFSState {
    BFScolors   color;
    PRInt32     distance;
    nsCStringKey  *predecessor;
    ~BFSState() {
        delete predecessor;
    }
};

// adjacency list and BFS hashtable data class.
struct SCTableData {
    nsCStringKey *key;
    union _data {
        BFSState *state;
        nsCOMArray<nsIAtom> *edges;
    } data;

    SCTableData(nsCStringKey* aKey) : key(aKey) {
        data.state = nullptr;
    }
};
#endif // __nsstreamconverterservice__h___
