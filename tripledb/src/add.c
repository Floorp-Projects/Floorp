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

/* Routines that add or remove things to the database. */

#include "tdbtypes.h"


#ifdef DEBUG
#include "tdbdebug.h"
#include "prprf.h"
static PRBool makedots = PR_FALSE; /* If true, print out dot-graphs of
                                      every step of balancing, to help my
                                      poor mind debug. */
static int dotcount = 0;
#endif

PRStatus TDBAdd(TDB* db, TDBNodePtr triple[3])
{
    PRStatus status = PR_FAILURE;
    TDBRecord* record;
    TDBPtr position;
    PRInt32 tree;
    PRInt32 i;
    PRInt64 cmp;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_FAILURE;
    PR_Lock(db->mutex);

    /* First, see if we already have this triple around... */
    tree = 0;                   /* Hard-coded knowledge that tree zero does
                                   things in [0], [1], [2] order. ### */

    position = db->roots[tree];
    while (position) {
        record = tdbLoadRecord(db, position);
        PR_ASSERT(record);
        if (!record) {
            goto DONE;
        }
        PR_ASSERT(record->position == position);
        if (record->position != position) {
            goto DONE;
        }
        for (i=0 ; i<3 ; i++) {
            cmp = tdbCompareNodes(triple[i], record->data[i]);
            if (cmp < 0) {
                position = record->entry[tree].child[0];
                break;
            } else if (cmp > 0) {
                position = record->entry[tree].child[1];
                break;
            }
        }
        if (position == record->position) {
            /* This means that our new entry exactly matches this one.  So,
               we're done. */
            status = PR_SUCCESS;
            goto DONE;
        }
    }

    tdbThrowOutCursorCaches(db);
    record = tdbAllocateRecord(db, triple);
    if (record == NULL) {
        goto DONE;
    }
    for (tree=0 ; tree<NUMTREES ; tree++) {
        status = tdbAddToTree(db, record, tree);
        if (status != PR_SUCCESS) goto DONE;
    }
    status = tdbQueueMatchingCallbacks(db, record, TDBACTION_ADDED);


 DONE:
    if (status == PR_SUCCESS) {
        tdbFlush(db);
    } else {
        tdbThrowAwayUnflushedChanges(db);
    }
    PR_Unlock(db->mutex);
    return status;
}


PRStatus TDBReplace(TDB* db, TDBNodePtr triple[3])
{
    /* Write me correctly!!!  This works, but is inefficient. ### */

    PRStatus status;
    TDBNodeRange range[3];
    range[0].min = triple[0];
    range[0].max = triple[0];
    range[1].min = triple[1];
    range[1].max = triple[1];
    range[2].min = NULL;
    range[2].max = NULL;
    status = TDBRemove(db, range);
    if (status == PR_SUCCESS) {
        status = TDBAdd(db, triple);
    }
    return status;
}

PRStatus TDBRemove(TDB* db, TDBNodeRange range[3])
{
    /* This could definitely be faster.  We're querying the database for
       a matching item, then we go search for it again when we delete it.
       The two operations probably ought to be merged. ### */
    PRStatus status;
    TDBCursor* cursor;
    TDBTriple* triple;
    TDBPtr position;
    TDBRecord* record;
    PRInt32 tree;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_FAILURE;
    PR_Lock(db->mutex);
    tdbThrowOutCursorCaches(db);
    for (;;) {
        cursor = tdbQueryNolock(db, range, NULL);
        if (!cursor) goto FAIL;
        triple = tdbGetResultNolock(cursor);
        if (triple) {
            /* Probably ought to play refcnt games with this to prevent it
               from being removed from the cache. ### */
            position = cursor->lasthit->position;
        }
        tdbFreeCursorNolock(cursor);
        cursor = NULL;
        if (!triple) {
            /* No more hits; all done. */
            break;
        }
        record = tdbLoadRecord(db, position);
        if (!record) goto FAIL;

        for (tree=0 ; tree<NUMTREES ; tree++) {
            status = tdbRemoveFromTree(db, record, tree);
            if (status != PR_SUCCESS) goto FAIL;
        }
        status = tdbAddToTree(db, record, -1);
        if (status != PR_SUCCESS) goto FAIL;

        status = tdbQueueMatchingCallbacks(db, record, TDBACTION_REMOVED);
        if (status != PR_SUCCESS) goto FAIL;

        tdbFlush(db);
    }
    PR_Unlock(db->mutex);
    return PR_SUCCESS;
 FAIL:
    tdbThrowAwayUnflushedChanges(db);
    PR_Unlock(db->mutex);
    return PR_FAILURE;
}




#ifdef MIN
#undef MIN
#endif
#ifdef MAX
#undef MAX
#endif

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static PRBool
rotateOnce(TDB* db, TDBPtr* rootptr, TDBRecord* oldroot, PRInt32 tree,
           PRInt32 dir)
{
    PRInt32 otherdir = 1 - dir;
    PRBool heightChanged;
    TDBPtr otherptr;
    TDBRecord* other;
    PRInt8 oldrootbal;
    PRInt8 otherbal;

#ifdef DEBUG
    if (makedots) {
        char* filename;
        filename = PR_smprintf("/tmp/balance%d-%d.dot", tree, dotcount++);
        TDBMakeDotGraph(db, filename, tree);
        PR_smprintf_free(filename);
    }
#endif

    PR_ASSERT(dir == 0 || dir == 1);
    if (dir != 0 && dir != 1) return PR_FALSE;

    otherptr = oldroot->entry[tree].child[otherdir];
    if (otherptr == 0) {
        tdbMarkCorrupted(db);
        return PR_FALSE;
    }

    other = tdbLoadRecord(db, otherptr);
    heightChanged = (other->entry[tree].balance != 0);

    *rootptr = otherptr;

    oldroot->entry[tree].child[otherdir] = other->entry[tree].child[dir];
    other->entry[tree].child[dir] = oldroot->position;

    /* update balances */
    oldrootbal = oldroot->entry[tree].balance;
    otherbal = other->entry[tree].balance;
    if (dir == 0) {
        oldrootbal -= 1 + MAX(otherbal, 0);
        otherbal -= 1 - MIN(oldrootbal, 0);
    } else {
        oldrootbal += 1 - MIN(otherbal, 0);
        otherbal += 1 + MAX(oldrootbal, 0);
    }
    oldroot->entry[tree].balance = oldrootbal;
    other->entry[tree].balance = otherbal;

    oldroot->dirty = PR_TRUE;
    other->dirty = PR_TRUE;

    return heightChanged;
}



static void
rotateTwice(TDB* db, TDBPtr* rootptr, TDBRecord* oldroot, PRInt32 tree,
            PRInt32 dir)
{
    PRInt32 otherdir = 1 - dir;
    TDBRecord* child;
    
    PR_ASSERT(dir == 0 || dir == 1);
    if (dir != 0 && dir != 1) return;

    child = tdbLoadRecord(db, oldroot->entry[tree].child[otherdir]);
    PR_ASSERT(child);
    if (child == NULL) return;

    rotateOnce(db, &(oldroot->entry[tree].child[otherdir]), child, tree,
               otherdir);
    oldroot->dirty = PR_TRUE;
    rotateOnce(db, rootptr, oldroot, tree, dir);
}



static PRStatus
balance(TDB* db, TDBPtr* rootptr, TDBRecord* oldroot, PRInt32 tree,
        PRBool* heightchange)
{
    PRInt8 oldbalance;
    TDBRecord* child;
    *heightchange = PR_FALSE;
    
    oldbalance = oldroot->entry[tree].balance;

    if (oldbalance < -1) {   /* need a right rotation */
        child = tdbLoadRecord(db, oldroot->entry[tree].child[0]);
        PR_ASSERT(child);
        if (child == NULL) return PR_FAILURE;
        if (child->entry[tree].balance == 1) {
            rotateTwice(db, rootptr, oldroot, tree, 1); /* double RL rotation
                                                           needed */
            *heightchange = PR_TRUE;
        } else {   /* single RR rotation needed */
            *heightchange = rotateOnce(db, rootptr, oldroot, tree, 1);
        }
    } else if (oldbalance > 1) {   /* need a left rotation */
        child = tdbLoadRecord(db, oldroot->entry[tree].child[1]);
        PR_ASSERT(child);
        if (child == NULL) return PR_FAILURE;
        if (child->entry[tree].balance == -1) {
            rotateTwice(db, rootptr, oldroot, tree, 0); /* double LR rotation
                                                           needed */
            *heightchange = PR_TRUE;
        } else {   /* single LL rotation needed */
            *heightchange = rotateOnce(db, rootptr, oldroot, tree, 0);
        }
    }

    return PR_SUCCESS;
}


static PRStatus doAdd(TDB* db, TDBRecord* record, PRInt32 tree,
                      PRInt32 comparerule, TDBPtr* rootptr,
                      PRBool* heightchange)
{
    PRBool increase = PR_FALSE;
    PRInt64 cmp;
    PRInt32 kid;
    PRStatus status;
    TDBRecord* cur;
    TDBPtr origptr;
    if (*rootptr == 0) {
        *rootptr = record->position;
        *heightchange = PR_TRUE;
        return PR_SUCCESS;
    }
    cur = tdbLoadRecord(db, *rootptr);
    if (!cur) return PR_FAILURE;
    cmp = tdbCompareRecords(record, cur, comparerule);
    PR_ASSERT(cmp != 0);        /* We carefully should never insert a
                                   record that we already have. */
    if (cmp == 0) return PR_FAILURE;
    kid = (cmp < 0) ? 0 : 1;
    origptr = cur->entry[tree].child[kid];
    status = doAdd(db, record, tree, comparerule,
                   &(cur->entry[tree].child[kid]), &increase);
    if (origptr != cur->entry[tree].child[kid]) {
        cur->dirty = PR_TRUE;
    }
    if (increase) {
        cur->entry[tree].balance += (kid == 0 ? -1 : 1);
        cur->dirty = PR_TRUE;
    }
    if (status != PR_SUCCESS) return status;
    if (increase && cur->entry[tree].balance != 0) {
        status = balance(db, rootptr, cur, tree, &increase);
        *heightchange = ! increase; /* If we did a rotate that absorbed
                                       the height change, then we don't want
                                       to propagate it on up. */
    }
    return PR_SUCCESS;
}


PRStatus tdbAddToTree(TDB* db, TDBRecord* record, PRInt32 tree)
{
    TDBTreeEntry* entry;
    PRBool checklinks;
    PRBool ignore;
    PRStatus status = PR_SUCCESS;
    TDBPtr* rootptr;
    TDBPtr origroot;
    PRInt32 comparerule = tree;

    PR_ASSERT(record != NULL);
    if (record == NULL) return PR_FAILURE;

    if (tree == -1) {
        tree = 0;
        rootptr = &(db->freeroot);
    } else {
        PR_ASSERT(tree >= 0 && tree < NUMTREES);
        if (tree < 0 || tree >= NUMTREES) {
            return PR_FAILURE;
        }
        rootptr = &(db->roots[tree]);
    }

    /* Check that this record does not seem to be already in this tree. */
    entry = record->entry + tree;
    checklinks = (entry->child[0] == 0 &&
                  entry->child[1] == 0 &&
                  entry->balance == 0);
    PR_ASSERT(checklinks);
    if (!checklinks) return PR_FAILURE;
    
    origroot = *rootptr;
    if (origroot == 0) {
        *rootptr = record->position;
    } else {
        status = doAdd(db, record, tree, comparerule, rootptr, &ignore);
    }
    if (origroot != *rootptr) {
        db->rootdirty = PR_TRUE;
    }
    db->dirty = PR_TRUE;
    return status;
}


static PRStatus doRemove(TDB* db, TDBRecord* record, PRInt32 tree,
                         PRInt32 comparerule, TDBPtr* rootptr,
                         TDBPtr** foundleafptr, TDBRecord** foundleaf,
                         PRBool* heightchange)
{
    PRStatus status;
    TDBRecord* cur;
    TDBPtr* leafptr;
    TDBRecord* leaf;
    PRInt32 kid;
    TDBPtr origptr;
    TDBPtr kid0;
    TDBPtr kid1;
    PRInt64 cmp;
    PRBool decrease = PR_FALSE;
    PRInt32 tmp;
    PRInt8 tmpbal;
    PRInt32 i;
    PR_ASSERT(*rootptr != 0);
    if (*rootptr == 0) return PR_FAILURE;
    cur = tdbLoadRecord(db, *rootptr);
    if (!cur) return PR_FAILURE;
    if (record == NULL) {
        cur->dirty = PR_TRUE;   /* Oh, what a bad, bad hack.  Need a better
                                   way to make sure not to miss a parent
                                   pointer that we've changed.  ### */
    }
    kid0 = cur->entry[tree].child[0];
    kid1 = cur->entry[tree].child[1];
    if (record == NULL) {
        /* We're looking for the smallest possible leaf node.  */
        PR_ASSERT(foundleafptr != NULL && foundleaf != NULL);
        if (kid0) cmp = -1;
        else {
            cmp = 0;
            *foundleafptr = rootptr;
            *foundleaf = cur;
        }
    } else {
        PR_ASSERT(foundleafptr == NULL && foundleaf == NULL);
        cmp = tdbCompareRecords(record, cur, comparerule);
    }
    if (cmp == 0) {
        PR_ASSERT(record == cur || record == NULL);
        if (record != cur && record != NULL) return PR_FAILURE;
        if (kid0 == 0 && kid1 == 0) {
            /* We're a leaf node. */
            *rootptr = 0;
            *heightchange = PR_TRUE;
            return PR_SUCCESS;
        } else if (kid0 == 0 || kid1 == 0) {
            /* Replace us with our single child. */
            *rootptr = kid0 != 0 ? kid0 : kid1;
            cur->entry[tree].child[0] = 0;
            cur->entry[tree].child[1] = 0;
            cur->entry[tree].balance = 0;
            cur->dirty = PR_TRUE;
            *heightchange = PR_TRUE;
            return PR_SUCCESS;
        } else {
            /* Ick.  We're a node in the middle of the tree.  Find who to
               replace us with. */
            kid = 1;            /* Informs balancing code later that we are
                                   taking things from the right subtree. */
            status = doRemove(db, NULL, tree, comparerule,
                              &(cur->entry[tree].child[1]),
                              &leafptr, &leaf, &decrease);
            /* Swap us in the tree with leaf.  Don't use the kid0/kid1
               variables any more, as they may no longer be valid. */

            if (record != NULL) {
                leaf->entry[tree].child[0] = cur->entry[tree].child[0];
                leaf->entry[tree].child[1] = cur->entry[tree].child[1];
                leaf->entry[tree].balance = cur->entry[tree].balance;
                cur->entry[tree].child[0] = 0;
                cur->entry[tree].child[1] = 0;
                cur->entry[tree].balance = 0;
                cur->dirty = PR_TRUE;
                *rootptr = leaf->position;
                cur = leaf;
                cur->dirty = PR_TRUE;
            } else {
                *foundleaf = cur;
                *foundleafptr = leafptr;
                PR_ASSERT(**foundleafptr == leaf->position);
                *leafptr = cur->position;
                *rootptr = leaf->position;
                for (i=0 ; i<2 ; i++) {
                    tmp = leaf->entry[tree].child[i];
                    leaf->entry[tree].child[i] = cur->entry[tree].child[i];
                    cur->entry[tree].child[i] = tmp;
                }
                tmpbal = leaf->entry[tree].balance;
                leaf->entry[tree].balance = cur->entry[tree].balance;
                cur->entry[tree].balance = tmpbal;
                cur->dirty = PR_TRUE;
                leaf->dirty = PR_TRUE;
            }
        }
    } else {
        kid = (cmp < 0) ? 0 : 1;
        origptr = cur->entry[tree].child[kid];
        status = doRemove(db, record, tree, comparerule,
                          &(cur->entry[tree].child[kid]), foundleafptr,
                          foundleaf, &decrease);
        if (origptr != cur->entry[tree].child[kid]) {
            cur->dirty = PR_TRUE;
        }
    }
    if (decrease) {
        cur->entry[tree].balance += (kid == 0 ? 1 : -1);
        cur->dirty = PR_TRUE;
    }
    if (status != PR_SUCCESS) return status;
    if (decrease) {
        if (cur->entry[tree].balance != 0) {
            status = balance(db, rootptr, cur, tree, &decrease);
            *heightchange = decrease;
        } else {
            *heightchange = PR_TRUE;
        }
    }
    return PR_SUCCESS;
}

PRStatus tdbRemoveFromTree(TDB* db, TDBRecord* record, PRInt32 tree)
{
    PRStatus status;
    PRInt32 comparerule = tree;
    TDBPtr* rootptr;
    TDBPtr origroot;
    PRBool ignore;
    
    PR_ASSERT(record != NULL);
    if (record == NULL) return PR_FAILURE;
    if (tree == -1) {
        tree = 0;
        rootptr = &(db->freeroot);
    } else {
        PR_ASSERT(tree >= 0 && tree < NUMTREES);
        if (tree < 0 || tree >= NUMTREES) {
            return PR_FAILURE;
        }
        rootptr = &(db->roots[tree]);
    }
    origroot = *rootptr;
    PR_ASSERT(origroot != 0);
    if (origroot == 0) return PR_FAILURE;
    status = doRemove(db, record, tree, comparerule, rootptr, NULL, NULL,
                      &ignore);

    if (origroot != *rootptr) {
        db->rootdirty = PR_TRUE;
    }
    db->dirty = PR_TRUE;
    record->dirty = PR_TRUE;
    return status;
}
