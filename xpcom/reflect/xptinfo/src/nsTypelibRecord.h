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

/* For keeping track of typelibs and their interface directory tables. */

#ifndef nsTypelibRecord_h___
#define nsTypelibRecord_h___

/*  class nsInterfaceInfoManager; */


struct XPTHeader;
class nsInterfaceInfoManager;
class nsInterfaceRecord;

class nsTypelibRecord {
    // XXX accessors?  Could conceal 1-based offset here.
public:
    friend class nsInterfaceInfoManager;

    nsTypelibRecord(int size, nsTypelibRecord *in_next, XPTHeader *in_header);

    static void DestroyList(nsTypelibRecord* aList);

    ~nsTypelibRecord();

    // array of pointers to (potentially shared) interface records,
    // NULL terminated.
    nsInterfaceRecord **interfaceRecords;
    nsTypelibRecord *next;
    XPTHeader *header;
#ifdef XPT_INFO_STATS
    char *filename;
    uint32 useCount;
#endif
};    

#endif /* nsTypelibRecord_h___ */

