/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#ifndef __nsstreamconverterservice__h___
#define __nsstreamconverterservice__h___

#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsHashtable.h"
#include "nsVoidArray.h"

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

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult) {
        nsresult rv;
        if (aOuter)
            return NS_ERROR_NO_AGGREGATION;

        nsStreamConverterService* _s = new nsStreamConverterService();
        if (_s == nsnull)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(_s);
        rv = _s->Init();
        if (NS_FAILED(rv)) {
            delete _s;
            return rv;
        }
        rv = _s->QueryInterface(aIID, aResult);
        NS_RELEASE(_s);
        return rv;
    }

private:
    // Responsible for finding a converter for the given MIME-type.
    nsresult FindConverter(const char *aProgID, nsVoidArray **aEdgeList);
    nsresult BuildGraph(void);
    nsresult AddAdjacency(const char *aProgID);
    nsresult ParseFromTo(const char *aProgID, nsCString &aFromRes, nsCString &aToRes);

    // member variables
    nsHashtable              *mAdjacencyList;
};

///////////////////////////////////////////////////////////////////
// Breadth-First-Search (BFS) algorithm state classes and types.

// adjacency list and BFS hashtable data class.
typedef struct _tableData {
    nsHashKey *key;
    nsCString *keyString;
    void      *data;
} SCTableData;

// used  to establish discovered vertecies.
enum BFScolors {white, gray, black};

typedef struct _BFSState {
    BFScolors   color;
    PRInt32     distance;
    nsHashKey  *predecessor;
} BFSState;

#endif // __nsstreamconverterservice__h___
