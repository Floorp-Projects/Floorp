/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is nsCacheMetaData.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */

#ifndef _nsCacheMetaData_h_
#define _nsCacheMetaData_h_

#include "nspr.h"
#include "pldhash.h"
#include "nscore.h"
// #include "nsCOMPtr.h"
#include "nsString.h"
// #include "nsAString.h"

class nsICacheMetaDataVisitor;

typedef struct {
    nsCString *  key;
    nsCString *  value;
} nsCacheMetaDataKeyValuePair;


typedef struct {
  PLDHashNumber  keyHash;
  nsCString *    key;
  nsCString *    value;
} nsCacheMetaDataHashTableEntry;


class nsCacheMetaData {
public:
    nsCacheMetaData();
    ~nsCacheMetaData();

    static
    nsCacheMetaData *     Create(void);

    nsresult              Init(void);

    const nsACString *    GetElement(const nsACString * key);

    nsresult              SetElement(const nsACString& key,
                                     const nsACString& value);

    PRUint32              Size(void);

    nsresult              FlattenMetaData(char * buffer, PRUint32 bufSize);
    
    nsresult              UnflattenMetaData(char * buffer, PRUint32 bufSize);

    nsresult              VisitElements(nsICacheMetaDataVisitor * visitor);

private:
    // PLDHashTable operation callbacks
    static const void *   PR_CALLBACK GetKey( PLDHashTable *table, PLDHashEntryHdr *entry);

    static PLDHashNumber  PR_CALLBACK HashKey( PLDHashTable *table, const void *key);

    static PRBool         PR_CALLBACK MatchEntry( PLDHashTable *           table,
                                                  const PLDHashEntryHdr *  entry,
                                                  const void *             key);

    static void           PR_CALLBACK MoveEntry( PLDHashTable *table,
                                                 const PLDHashEntryHdr *from,
                                                 PLDHashEntryHdr       *to);

    static void           PR_CALLBACK ClearEntry( PLDHashTable *table, PLDHashEntryHdr *entry);

    static void           PR_CALLBACK Finalize( PLDHashTable *table);

    static
    PLDHashOperator       PR_CALLBACK CalculateSize(PLDHashTable *table,
                                                    PLDHashEntryHdr *hdr,
                                                    PRUint32 number,
                                                    void *arg);

    static
    PLDHashOperator       PR_CALLBACK AccumulateElement(PLDHashTable *table,
                                                         PLDHashEntryHdr *hdr,
                                                         PRUint32 number,
                                                         void *arg);

    static
    PLDHashOperator       PR_CALLBACK FreeElement(PLDHashTable *table,
                                                   PLDHashEntryHdr *hdr,
                                                   PRUint32 number,
                                                   void *arg);
    static
    PLDHashOperator       PR_CALLBACK VisitElement(PLDHashTable *table,
                                                    PLDHashEntryHdr *hdr,
                                                    PRUint32 number,
                                                    void *arg);

    // member variables
    static PLDHashTableOps ops;
    PLDHashTable           table;
    PRBool                 initialized;
};

#endif // _nsCacheMetaData_h
