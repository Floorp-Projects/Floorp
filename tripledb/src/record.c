/* -*- Mode: C; indent-tabs-mode: nil; -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * The Original Code is the TripleDB database library.
 * 
 * The Initial Developer of the Original Code is Geocast Network Systems.
 * Portions created by Geocast are Copyright (C) 1999 Geocast Network Systems.
 * All Rights Reserved.
 * 
 * Contributor(s): Terry Weissman <terry@mozilla.org>
 */

/* Operations that do things on file record. */

#include "tdbtypes.h"




TDBRecord* tdbLoadRecord(TDB* db, TDBPtr position)
{
    TDBRecord* result;
    char buf[sizeof(PRUint32)];
    char* ptr;
    PRInt32 i;
    PRInt32 num;
    PRInt32 numtoread;

    for (result = db->firstrecord ; result ; result = result->next) {
        if (result->position == position) {
            goto DONE;
        }
    }

    if (PR_Seek(db->fid, position, PR_SEEK_SET) < 0) {
        goto DONE;
    }
    num = PR_Read(db->fid, buf, sizeof(buf));
    if (num != sizeof(buf)) {
        goto DONE;
    }
    result = PR_NEWZAP(TDBRecord);
    if (result == NULL) goto DONE;
    result->position = position;
    ptr = buf;
    result->length = tdbGetInt32(&ptr);
    if (result->length < MINRECORDSIZE || result->length > MAXRECORDSIZE) {
        tdbMarkCorrupted(db);
        PR_Free(result);
        result = NULL;
        goto DONE;
    }   
    if (tdbGrowIobuf(db, result->length) != PR_SUCCESS) {
        PR_Free(result);
        result = NULL;
        goto DONE;
    }
    numtoread = result->length - sizeof(PRInt32);
    num = PR_Read(db->fid, db->iobuf, numtoread);
    if (num < numtoread) {
        PR_Free(result);
        result = NULL;
        goto DONE;
    }
    ptr = db->iobuf;
    for (i=0 ; i<NUMTREES ; i++) {
        result->entry[i].child[0] = tdbGetInt32(&ptr);
        result->entry[i].child[1] = tdbGetInt32(&ptr);
        result->entry[i].balance = tdbGetInt8(&ptr);
    }
    for (i=0 ; i<3 ; i++) {
        result->data[i] = tdbGetNode(db, &ptr);
        if (result->data[i] == NULL) {
            while (--i >= 0) {
                PR_Free(result->data[i]);
            }
            PR_Free(result);
            result = NULL;
            goto DONE;
        }
    }
            
    if (ptr - db->iobuf != numtoread) {
        tdbMarkCorrupted(db);
        for (i=0 ; i<3 ; i++) {
            PR_Free(result->data[i]);
        }
        PR_Free(result);
        result = NULL;
        goto DONE; 
    }

    PR_ASSERT(db->firstrecord != result);
    result->next = db->firstrecord;
    db->firstrecord = result;

DONE:
    return result;
}


PRStatus tdbSaveRecord(TDB* db, TDBRecord* record)
{
    PRStatus status;
    PRInt32 i;
    char* ptr;
    if (PR_Seek(db->fid, record->position, PR_SEEK_SET) < 0) {
        return PR_FAILURE;
    }
    status = tdbGrowIobuf(db, record->length);
    if (status != PR_SUCCESS) return status;
    ptr = db->iobuf;
    tdbPutInt32(&ptr, record->length);
    for (i=0 ; i<NUMTREES ; i++) {
        tdbPutInt32(&ptr, record->entry[i].child[0]);
        tdbPutInt32(&ptr, record->entry[i].child[1]);
        tdbPutInt8(&ptr, record->entry[i].balance);
    }
    for (i=0 ; i<3 ; i++) {
        tdbPutNode(db, &ptr, record->data[i]);
    }
    PR_ASSERT(ptr - db->iobuf == record->length);
    if (PR_Write(db->fid, db->iobuf, record->length) != record->length) {
        return PR_FAILURE;
    }
    return PR_SUCCESS;
}


PRStatus tdbFreeRecord(TDBRecord* record)
{
    PRInt32 i;
    for (i=0 ; i<3 ; i++) {
        PR_Free(record->data[i]);
    }
    PR_Free(record);
    return PR_SUCCESS;
}


TDBRecord* tdbAllocateRecord(TDB* db, TDBNodePtr triple[3])
{
    PRInt32 i;
    PRInt32 size;
    TDBRecord* result = NULL;
    TDBRecord* tmp;
    TDBPtr position;

    size = sizeof(PRInt32) +
        NUMTREES * (sizeof(PRInt32) + sizeof(PRInt32) + sizeof(PRInt8)) +
        tdbNodeSize(triple[0]) +
        tdbNodeSize(triple[1]) +
        tdbNodeSize(triple[2]);

    position = db->freeroot;
    if (position > 0) {
        do {
            result = tdbLoadRecord(db, position);
            if (result->length > size) {
                position = result->entry[0].child[0];
            } else if (result->length < size) {
                position = result->entry[0].child[1];
            } else {
                /* Hey, we found one! */
                if (tdbRemoveFromTree(db, result, -1) != PR_SUCCESS) {
                    tdbMarkCorrupted(db);
                    return NULL;
                }
                break;
            }
        } while (position != 0);
    }
    if (position == 0) {
        result = PR_NEWZAP(TDBRecord);
        if (!result) return NULL;
        position = db->filelength;
        db->filelength += size;
        PR_ASSERT(db->firstrecord != result);
        result->next = db->firstrecord;
        db->firstrecord = result;
    } else {
        for (i=0 ; i<3 ; i++) {
            PR_Free(result->data[i]);
        }
        tmp = result->next;
        memset(result, 0, sizeof(TDBRecord));
        result->next = tmp;
    }
    result->position = position;
    result->length = size;
    for (i=0 ; i<3 ; i++) {
        result->data[i] = tdbNodeDup(triple[i]);
        if (result->data[i] == NULL) {
            while (--i >= 0) {
                PR_Free(result->data[i]);
            }
            PR_Free(result);
            return NULL;
        }
    }
    result->dirty = PR_TRUE;
    return result;
}


/* Argh, this same static array appears in query.c.  Don't do that. ### */

static int key[4][3] = {
    {0, 1, 2},
    {1, 0, 2},
    {2, 1, 0},
    {1, 2, 0}
};

PRInt64 tdbCompareRecords(TDBRecord* r1, TDBRecord* r2, PRInt32 comparerule)
{
    int i, k;
    PRInt64 cmp;
    if (comparerule == -1) {
        cmp = r1->length - r2->length;
        if (cmp != 0) return cmp;
        return r1->position - r2->position;
    }
    PR_ASSERT(comparerule >= 0 && comparerule < NUMTREES);
    for (i=0 ; i<3 ; i++) {
        k = key[comparerule][i];
        cmp = tdbCompareNodes(r1->data[k], r2->data[k]);
        if (cmp != 0) return cmp;
    }
    return 0;
}

