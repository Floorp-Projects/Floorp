/* -*- Mode: C; indent-tabs-mode: nil; -*-
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
 * The Original Code is the TripleDB database library.
 *
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are
 * Copyright (C) 1999 Geocast Network Systems. All
 * Rights Reserved.
 *
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

#ifndef _TDBtypes_h_
#define _TDBtypes_h_ 1

/* These are private, internal type definitions, */

#include <string.h>
#include "pratom.h"
#include "prcvar.h"
#include "prio.h"
#include "prlock.h"
#include "prlog.h"
#include "prmem.h"
#include "prthread.h"
#include "prtime.h"
#include "prtypes.h"

#include "tdbapi.h"

/* Magic number that appears as first four bytes of db files. */
#define TDB_MAGIC       "TDB\n"


/* Total number of bytes we reserve at the beginning of the db file for
   special stuff. */
#define TDB_FILEHEADER_SIZE     1024



/* Current version number for db files. */

#define TDB_VERSION     1


#define NUMTREES 4              /* How many binary trees we're keeping,
                                   indexing the data. */

/* The trees index things in the following orders:
   0:           0, 1, 2
   1:           1, 0, 2
   2:           2, 1, 0
   3:           1, 2, 0
*/

/* MINRECORDSIZE and MAXRECORDSIZE are the minimum and maximum possible record 
   size.  They're useful for sanity checking. */
#define BASERECORDSIZE (sizeof(PRInt32) + NUMTREES * (2*sizeof(TDBPtr)+sizeof(PRInt8)))
#define MINRECORDSIZE (BASERECORDSIZE + 3 * 2)
#define MAXRECORDSIZE (BASERECORDSIZE + 3 * 65540)


typedef PRInt32 TDBPtr;

typedef struct {
    TDBPtr child[2];
    PRInt8 balance;
} TDBTreeEntry;


typedef struct _TDBRecord {
    TDBPtr position;
    PRInt32 length;
    PRInt32 refcnt;
    PRBool dirty;
    struct _TDBRecord* next;
    PRUint8 flags;
    TDBTreeEntry entry[NUMTREES];
    TDBNode* data[3];
} TDBRecord;


typedef struct _TDBParentChain {
    TDBRecord* record;
    struct _TDBParentChain* next;
} TDBParentChain;



struct _TDBCursor {
    TDB* db;
    TDBNodeRange range[3];
    TDBRecord* cur;
    TDBRecord* lasthit;
    TDBParentChain* parent;
    PRInt32 tree;
    PRBool reverse;
    TDBTriple triple;
    PRInt32 hits;
    PRInt32 misses;
    struct _TDBCursor* nextcursor;
    struct _TDBCursor* prevcursor;
};


typedef struct _TDBCallbackInfo {
    struct _TDBCallbackInfo* nextcallback;
    TDBNodeRange range[3];
    TDBCallbackFunction func;
    void* closure;
} TDBCallbackInfo;

typedef struct _TDBPendingCall {
    struct _TDBPendingCall* next;
    TDBCallbackFunction func;
    void* closure;
    TDBTriple triple;
    PRInt32 action;
} TDBPendingCall;
    
    


struct _TDB {
    char* filename;
    PRInt32 filelength;
    PRFileDesc* fid;
    TDBPtr roots[NUMTREES];
    TDBPtr freeroot;
    PRBool dirty;
    PRBool rootdirty;
    PRLock* mutex;              /* Used to prevent more than one thread
                                   from doing anything in DB code at the
                                   same time. */
    PRThread* callbackthread;   /* Thread ID of the background
                                   callback-calling thread. */
    PRCondVar* callbackcvargo;  /* Used to wake up the callback-calling
                                   thread. */
    PRCondVar* callbackcvaridle; /* Used by the callback-calling to indicate
                                    that it is now idle. */
    PRBool killcallbackthread;
    PRBool callbackidle;
    char* iobuf;
    PRInt32 iobuflength;
    TDBRecord* firstrecord;
    TDBCursor* firstcursor;
    TDBCallbackInfo* firstcallback;
    TDBPendingCall* firstpendingcall;
    TDBPendingCall* lastpendingcall;
};


/* ----------------------------- From add.c: ----------------------------- */

extern PRStatus tdbAddToTree(TDB* db, TDBRecord* record, PRInt32 tree); 
extern PRStatus tdbRemoveFromTree(TDB* db, TDBRecord* record, PRInt32 tree);


/* ----------------------------- From callback.c: ------------------------- */
extern void tdbCallbackThread(void* closure);
extern PRStatus tdbQueueMatchingCallbacks(TDB* db, TDBRecord* record,
                                          PRInt32 action);


/* ----------------------------- From node.c: ----------------------------- */


extern TDBNode* tdbNodeDup(TDBNode* node);
extern PRInt32 tdbNodeSize(TDBNode* node);
extern PRInt64 tdbCompareNodes(TDBNode* n1, TDBNode* n2);
extern TDBNode* tdbGetNode(TDB* db, char** ptr);
extern void tdbPutNode(TDB* db, char** ptr, TDBNode* node);


/* ----------------------------- From query.c: ----------------------------- */

extern TDBCursor* tdbQueryNolock(TDB* db, TDBNodeRange range[3],
                                 TDBSortSpecification* sortspec);
extern PRStatus tdbFreeCursorNolock(TDBCursor* cursor);
extern TDBTriple* tdbGetResultNolock(TDBCursor* cursor);
extern PRInt64 tdbCompareToRange(TDBRecord* record, TDBNodeRange* range,
                                 PRInt32 comparerule);
extern PRBool tdbMatchesRange(TDBRecord* record, TDBNodeRange* range);
extern void tdbThrowOutCursorCaches(TDB* db);



/* ----------------------------- From record.c: ---------------------------- */

extern TDBRecord* tdbLoadRecord(TDB* db, TDBPtr position);
extern PRStatus tdbSaveRecord(TDB* db, TDBRecord* record);
extern PRStatus tdbFreeRecord(TDBRecord* record);
extern TDBRecord* tdbAllocateRecord(TDB* db, TDBNodePtr triple[3]);
extern PRInt64 tdbCompareRecords(TDBRecord* r1, TDBRecord* r2,
                                 PRInt32 comparerule);



/* ----------------------------- From tdb.c: ----------------------------- */

extern PRStatus tdbFlush(TDB* db);
extern PRStatus tdbThrowAwayUnflushedChanges(TDB* db);
extern PRStatus tdbGrowIobuf(TDB* db, PRInt32 length);
extern PRStatus tdbLoadRoots(TDB* db);
extern void tdbMarkCorrupted(TDB* db);

extern PRInt32 tdbGetInt32(char** ptr);
extern PRInt32 tdbGetInt16(char** ptr);
extern PRInt8 tdbGetInt8(char** ptr);
extern PRInt64 tdbGetInt64(char** ptr);
extern PRUint16 tdbGetUInt16(char** ptr);
extern void tdbPutInt32(char** ptr, PRInt32 value);
extern void tdbPutInt16(char** ptr, PRInt16 value) ;
extern void tdbPutUInt16(char** ptr, PRUint16 value) ;
extern void tdbPutInt8(char** ptr, PRInt8 value);
extern void tdbPutInt64(char** ptr, PRInt64 value);

#endif /* _TDBtypes_h_ */
