/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * Copyright (C) 1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* Library-private header for nsInterfaceInfoManager implementation. */

#ifndef nsInterfaceInfoManager_h___
#define nsInterfaceInfoManager_h___

#include "plhash.h"
#include "nsIInterfaceInfo.h"
#include "nsIInterfaceInfoManager.h"

#include "nsHashtable.h"

#include "nsCOMPtr.h"

#include "xpt_struct.h"
#include "nsInterfaceInfo.h"

#include "nsInterfaceRecord.h"
#include "nsTypelibRecord.h"

class nsFileSpec;

class nsInterfaceInfoManager : public nsIInterfaceInfoManager
{
    NS_DECL_ISUPPORTS
    NS_DECL_NSIINTERFACEINFOMANAGER

public:
    virtual ~nsInterfaceInfoManager();
    static nsInterfaceInfoManager* GetInterfaceInfoManager();
    static void FreeInterfaceInfoManager();

#ifdef XPT_INFO_STATS
    void DumpStats();
#endif

private:
    nsInterfaceInfoManager();
    nsresult initInterfaceTables();

    // Should this be public?
    nsresult indexify_file(const nsFileSpec *fileSpec);

    // list of typelib records for enumeration.  (freeing?)
    nsTypelibRecord *typelibRecords;

    // mapping between names and records
    PLHashTable *nameTable;

    // mapping between IIDs and records.
    nsHashtable *IIDTable;

    PRBool ctor_succeeded;

    XPTArena* arena;
};

#endif /* nsInterfaceInfoManager_h___ */



