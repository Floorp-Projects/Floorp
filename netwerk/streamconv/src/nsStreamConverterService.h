/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nsstreamconverterservice__h___
#define __nsstreamconverterservice__h___

#include "nsIStreamConverterService.h"

#include "nsClassHashtable.h"
#include "nsCOMArray.h"
#include "nsTArrayForwardDeclare.h"

class nsCString;
class nsIAtom;

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

private:
    virtual ~nsStreamConverterService();

    // Responsible for finding a converter for the given MIME-type.
    nsresult FindConverter(const char *aContractID, nsTArray<nsCString> **aEdgeList);
    nsresult BuildGraph(void);
    nsresult AddAdjacency(const char *aContractID);
    nsresult ParseFromTo(const char *aContractID, nsCString &aFromRes, nsCString &aToRes);

    // member variables
    nsClassHashtable<nsCStringHashKey, nsCOMArray<nsIAtom>> mAdjacencyList;
};

#endif // __nsstreamconverterservice__h___
