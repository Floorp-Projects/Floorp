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

/* Routines that query things from the database. */

#include "tdbtypes.h"

/* Argh, this same static array appears in record.c.  Don't do that. ### */

static int key[NUMTREES][3] = {
    {0, 1, 2},
    {1, 0, 2},
    {2, 1, 0},
    {1, 2, 0}
};


static void
freeParentChain(TDBCursor* cursor)
{
    TDBParentChain* tmp;
    while (cursor->parent) {
        tmp = cursor->parent->next;
        cursor->parent->record->refcnt--;
        PR_ASSERT(cursor->parent->record->refcnt >= 0);
        PR_Free(cursor->parent);
        cursor->parent = tmp;
    }
}


static PRStatus
moveCursorForward(TDBCursor* cursor)
{
    TDBRecord* cur;
    PRBool found;
    PRInt32 fwd;
    PRInt32 rev;
    TDBPtr ptr;
    PRInt32 tree = cursor->tree;
    TDBParentChain* parent;
    TDBParentChain* tmp;

    cur = cursor->cur;

    PR_ASSERT(cur != NULL);
    if (cur == NULL) {
        return PR_FAILURE;
    }
    cur->refcnt--;
    PR_ASSERT(cur->refcnt >= 0);
    cursor->cur = NULL;
    fwd = cursor->reverse ? 0 : 1;
    rev = cursor->reverse ? 1 : 0;
    ptr = cur->entry[tree].child[fwd];
    if (ptr != 0) {
        do {
            cur->refcnt++;
            parent = PR_NEWZAP(TDBParentChain);
            parent->record = cur;
            parent->next = cursor->parent;
            cursor->parent = parent;
            cur = tdbLoadRecord(cursor->db, ptr);
            PR_ASSERT(cur);
            if (!cur) {
                return PR_FAILURE;
            }
            ptr = cur->entry[tree].child[rev];
        } while (ptr != 0);
    } else {
        found = PR_FALSE;
        while (cursor->parent) {
            ptr = cur->position;
            cur = cursor->parent->record;
            cur->refcnt--;
            PR_ASSERT(cur->refcnt >= 0);
            tmp = cursor->parent->next;
            PR_Free(cursor->parent);
            cursor->parent = tmp;
            if (cur->entry[tree].child[rev] == ptr) {
                found = PR_TRUE;
                break;
            }
            if (cur->entry[tree].child[fwd] != ptr) {
                tdbMarkCorrupted(cursor->db);
                return PR_FAILURE;
            }
        }
        if (!found) cur = NULL;
    }
    if (cur) cur->refcnt++;
    cursor->cur = cur;
    return PR_SUCCESS;
}


static PRStatus findFirstNode(TDBCursor* cursor, TDBNodeRange range[3])
{
    PRInt32 fwd;
    PRInt32 rev;
    PRBool reverse;
    PRInt32 tree;
    TDBParentChain* parent;
    TDBPtr curptr;
    TDBRecord* cur;
    PRInt64 cmp;

    reverse = cursor->reverse;
    fwd = reverse ? 0 : 1;
    rev = reverse ? 1 : 0;
    tree = cursor->tree;

    freeParentChain(cursor);

    curptr = cursor->db->roots[tree];
    cur = NULL;
    while (curptr) {
        if (cur) {
            cur->refcnt++;
            parent = PR_NEWZAP(TDBParentChain);
            parent->record = cur;
            parent->next = cursor->parent;
            cursor->parent = parent;
        }
        cur = tdbLoadRecord(cursor->db, curptr);
        PR_ASSERT(cur);
        if (!cur) return PR_FAILURE;
        cmp = tdbCompareToRange(cur, range, tree);
        if (reverse) cmp = -cmp;
        if (cmp >= 0) {
            curptr = cur->entry[tree].child[rev];
        } else {
            curptr = cur->entry[tree].child[fwd];
        }
    }
    if (cursor->cur) {
        cursor->cur->refcnt--;
        PR_ASSERT(cursor->cur->refcnt >= 0);
    }
    cursor->cur = cur;
    if (cursor->cur) {
        cursor->cur->refcnt++;
    }
    while (cursor->cur != NULL &&
           tdbCompareToRange(cursor->cur, range, tree) < 0) {
        moveCursorForward(cursor);
    }
    return PR_SUCCESS;
}


TDBCursor* tdbQueryNolock(TDB* db, TDBNodeRange range[3],
                          TDBSortSpecification* sortspec)
{
    PRInt32 rangescore[3];
    PRInt32 tree;
    PRInt32 curscore;
    PRInt32 bestscore = -1;
    PRInt32 i;
    TDBCursor* result;
    PRBool reverse;
    PRStatus status;

    result = PR_NEWZAP(TDBCursor);
    if (!result) return NULL;

    for (i=0 ; i<3 ; i++) {
        if (range[i].min) {
            result->range[i].min = tdbNodeDup(range[i].min);
            if (result->range[i].min == NULL) goto FAIL;
        }
        if (range[i].max) {
            result->range[i].max = tdbNodeDup(range[i].max);
            if (result->range[i].max == NULL) goto FAIL;
        }
    }

    for (tree = 0 ; tree < NUMTREES ; tree++) {
        if (key[tree][0] == sortspec->keyorder[0] &&
            key[tree][1] == sortspec->keyorder[1] &&
            key[tree][2] == sortspec->keyorder[2]) {
            break;
        }
    }
    
    if (tree >= NUMTREES) {
        /* The passed in keyorder was not valid (which, in fact, is the usual
           case).  Go find the best tree to use. */
        for (i=0 ; i<3 ; i++) {
            rangescore[i] = 0;
            if (range[i].min != NULL || range[i].max != NULL) {
                /* Hey, some limitations were specified, we like this key
                   some. */
                rangescore[i]++;
                if (range[i].min != NULL && range[i].max != NULL) {
                    /* Ooh, we were given both minimum and maximum, that's
                       better than only getting one.*/
                    rangescore[i]++;
                    if (tdbCompareNodes(range[i].min, range[i].max) == 0) {
                        /* Say!  This key was exactly specified.  We like it
                           best. */
                        rangescore[i]++;
                    }
                }
            }
        }

        for (i=0 ; i<NUMTREES ; i++) {
            curscore = rangescore[key[i][0]] * 100 +
                rangescore[key[i][1]]  * 10 +
                rangescore[key[i][2]];
            if (bestscore < curscore) {
                bestscore = curscore;
                tree = i;
            }
        }
    }

    reverse = sortspec != NULL && sortspec->reverse;

    result->reverse = reverse;
    result->db = db;
    result->tree = tree;
    status = findFirstNode(result, range);
    if (status != PR_SUCCESS) goto FAIL;

    result->nextcursor = db->firstcursor;
    if (result->nextcursor) {
        result->nextcursor->prevcursor = result;
    }
    db->firstcursor = result;
    
    tdbFlush(db);
    return result;


 FAIL:
    tdbFreeCursorNolock(result);
    return NULL;
}

TDBCursor* TDBQuery(TDB* db, TDBNodeRange range[3],
                    TDBSortSpecification* sortspec)
{
    TDBCursor* result;
    PR_Lock(db->mutex);
    result = tdbQueryNolock(db, range, sortspec);
    PR_Unlock(db->mutex);
    return result;
}




PRStatus tdbFreeCursorNolock(TDBCursor* cursor)
{
    PRInt32 i;
    TDB* db;
    PR_ASSERT(cursor);
    if (!cursor) return PR_FAILURE;
    db = cursor->db;
    if (cursor->cur) {
        cursor->cur->refcnt--;
        PR_ASSERT(cursor->cur->refcnt >= 0);
    }
    if (cursor->lasthit) {
        cursor->lasthit->refcnt--;
        PR_ASSERT(cursor->lasthit->refcnt >= 0);
    }
    freeParentChain(cursor);
    if (db) tdbFlush(db);
    for (i=0 ; i<3 ; i++) {
        PR_FREEIF(cursor->range[i].min);
        PR_FREEIF(cursor->range[i].max);
    }
    if (cursor->prevcursor) {
        cursor->prevcursor->nextcursor = cursor->nextcursor;
    } else {
        if (db) db->firstcursor = cursor->nextcursor;
    }
    if (cursor->nextcursor) {
        cursor->nextcursor->prevcursor = cursor->prevcursor;
    }
    PR_Free(cursor);
    return PR_SUCCESS;
}


PRStatus TDBFreeCursor(TDBCursor* cursor)
{
    PRStatus status;
    TDB* db;
    PR_ASSERT(cursor);
    if (!cursor) return PR_FAILURE;
    db = cursor->db;
    PR_Lock(db->mutex);
    status = tdbFreeCursorNolock(cursor);
#ifdef DEBUG
    if (db->firstcursor == 0) {
        /* There are no more cursors.  No other thread can be in the middle of
           writing stuff, because we have the mutex. And so, there shouldn't
           be anything left in our cache of records; they should all be
           flushed out.  So... */
        PR_ASSERT(db->firstrecord == NULL);
    }
#endif
    PR_Unlock(db->mutex);
    return status;
}


TDBTriple* tdbGetResultNolock(TDBCursor* cursor)
{
    PRStatus status;
    PRInt32 i;
    PRInt64 cmp;
    TDBNodeRange range[3];
    PR_ASSERT(cursor);
    if (!cursor) return NULL;
    if (cursor->cur == NULL && cursor->lasthit == NULL &&
        cursor->triple.data[0] != NULL) {
        /* Looks like someone did a write to the database since we last were
           here, and therefore threw away all our cached information about
           where we were.  Go find our place again. */
        for (i=0 ; i<3 ; i++) {
            range[i].min = cursor->triple.data[i];
            range[i].max = NULL;
            if (cursor->reverse) {
                range[i].max = range[i].min;
                range[i].min = NULL;
            }
        }
        status = findFirstNode(cursor, range);
        if (status != PR_SUCCESS) return NULL;
        if (cursor->cur) {
            PRBool match = PR_TRUE;
            for (i=0 ; i<3 ; i++) {
                if (tdbCompareNodes(cursor->cur->data[i],
                                    cursor->triple.data[i]) != 0) {
                    match = PR_FALSE;
                    break;
                }
            }
            if (match) {
                /* OK, this node we found was exactly the one we were at
                   last time.  Bump it up one. */
                moveCursorForward(cursor);
            }
        }
    }

    for (i=0 ; i<3 ; i++) {
        PR_FREEIF(cursor->triple.data[i]);
    }
    if (cursor->lasthit) {
        cursor->lasthit->refcnt--;
        cursor->lasthit = NULL;
    }
    if (cursor->cur == NULL) return NULL;
    while (cursor->cur != NULL) {
        if (tdbMatchesRange(cursor->cur, cursor->range)) {
            break;
        }
        cmp = tdbCompareToRange(cursor->cur, cursor->range, cursor->tree);
        if (cursor->reverse ? (cmp < 0) : (cmp > 0)) {
            /* We're off the end of the range, all done. */
            cursor->cur->refcnt--;
            PR_ASSERT(cursor->cur->refcnt >= 0);
            cursor->cur = NULL;
            break;
        }       
        cursor->misses++;
        moveCursorForward(cursor);
    }
    if (cursor->cur == NULL) {
        tdbFlush(cursor->db);
        return NULL;
    }
    for (i=0 ; i<3 ; i++) {
        cursor->triple.data[i] = tdbNodeDup(cursor->cur->data[i]);
    }
    cursor->lasthit = cursor->cur;
    cursor->lasthit->refcnt++;
    moveCursorForward(cursor);
    cursor->hits++;
#ifdef DEBUG
    {
        TDBParentChain* tmp;
        PR_ASSERT(cursor->cur == NULL || cursor->cur->refcnt > 0);
        for (tmp = cursor->parent ; tmp ; tmp = tmp->next) {
            PR_ASSERT(tmp->record->refcnt > 0);
        }
    }
#endif
    tdbFlush(cursor->db);
    return &(cursor->triple);
}


TDBTriple* TDBGetResult(TDBCursor* cursor)
{
    TDBTriple* result;
    PR_ASSERT(cursor && cursor->db);
    if (!cursor || !cursor->db) return NULL;
    PR_Lock(cursor->db->mutex);
    result = tdbGetResultNolock(cursor);
    PR_Unlock(cursor->db->mutex);
    return result;
}



/* Determines where this item falls within the range of items defined.
   Negative means this item is too early, and positive means too late.  
   Zero means that it seems to fall within the range, but you need to do
   a call to tdbMatchesRange() to really make sure.  The idea here is that
   as you slowly walk along the appropriate tree in the DB, this routine
   will always return a nondecreasing result. */

PRInt64 tdbCompareToRange(TDBRecord* record, TDBNodeRange* range,
                          PRInt32 comparerule)
{
    int i;
    int k;
    PR_ASSERT(record != NULL && range != NULL);
    if (record == NULL || range == NULL) return PR_FALSE;
    PR_ASSERT(comparerule >= 0 && comparerule < NUMTREES);
    if (comparerule < 0 || comparerule >= NUMTREES) {
        return 0;
    }
    for (i=0 ; i<3 ; i++) {
        k = key[comparerule][i];
        if (range[k].min != NULL &&
            tdbCompareNodes(record->data[k], range[k].min) < 0) {
            return -1;
        }
        if (range[k].max != NULL &&
            tdbCompareNodes(record->data[k], range[k].max) > 0) {
            return 1;
        }
        if (range[k].min == NULL || range[k].max == NULL ||
            tdbCompareNodes(range[k].min, range[k].max) != 0) {
            return 0;
        }
    }
    return 0;
}


PRBool tdbMatchesRange(TDBRecord* record, TDBNodeRange* range)
{
    PRInt32 i;
    PR_ASSERT(record != NULL && range != NULL);
    if (record == NULL || range == NULL) return PR_FALSE;
    for (i=0 ; i<3 ; i++) {
        if (range[i].min != NULL &&
            tdbCompareNodes(record->data[i], range[i].min) < 0) {
            return PR_FALSE;
        }
        if (range[i].max != NULL &&
            tdbCompareNodes(record->data[i], range[i].max) > 0) {
            return PR_FALSE;
        }
    }
    return PR_TRUE;
}


void tdbThrowOutCursorCaches(TDB* db)
{
    TDBCursor* cursor;
    for (cursor = db->firstcursor ; cursor ; cursor = cursor->nextcursor) {
        freeParentChain(cursor);
        if (cursor->cur) {
            cursor->cur->refcnt--;
            PR_ASSERT(cursor->cur->refcnt >= 0);
            cursor->cur = NULL;
        }
        if (cursor->lasthit) {
            cursor->lasthit->refcnt--;
            PR_ASSERT(cursor->lasthit->refcnt >= 0);
            cursor->lasthit = NULL;
        }
    }
}
