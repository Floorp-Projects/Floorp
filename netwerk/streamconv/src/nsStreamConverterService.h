/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef __nsstreamconverterservice__h___
#define __nsstreamconverterservice__h___

#include "nsIStreamConverterService.h"
#include "nsIStreamListener.h"
#include "nsHashtable.h"
#include "nsISupportsArray.h"
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

private:
    // Responsible for finding a converter for the given MIME-type.
    nsresult FindConverter(const char *aContractID, nsCStringArray **aEdgeList);
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

typedef struct _BFSState {
    BFScolors   color;
    PRInt32     distance;
    nsHashKey  *predecessor;
} BFSState;

// adjacency list and BFS hashtable data class.
typedef struct _tableData {
    nsCStringKey *key;
    union _data {
        BFSState *state;
        nsISupportsArray *edges;
    } data;
} SCTableData;

#endif // __nsstreamconverterservice__h___
