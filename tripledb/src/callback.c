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

static void 
tdbFreeCallbackInfo(TDBCallbackInfo* info)
{
    PRInt32 i;
    for (i=0 ; i<3 ; i++) {
	if (info->range[i].min != NULL) TDBFreeNode(info->range[i].min);
	if (info->range[i].max != NULL) TDBFreeNode(info->range[i].max);
    }
    PR_Free(info);
}


static void
tdbFreePendingCall(TDBPendingCall* call)
{
    PRInt32 i;
    for (i=0 ; i<3 ; i++) {
	if (call->triple.data[i] != NULL) {
	    TDBFreeNode(call->triple.data[i]);
	}
    }
    PR_Free(call);
}


void
tdbCallbackThread(void* closure)
{
    PRStatus status;
    TDB* db = (TDB*) closure;

    PRLock* mutex = db->mutex;
    PRCondVar* cvar = db->callbackcvargo;

    TDBPendingCall* call;
    TDBPendingCall* tmp;
    PR_Lock(mutex);
    while (1) {
        while (db->firstpendingcall == NULL) {
            db->callbackidle = PR_TRUE;
            PR_NotifyAllCondVar(db->callbackcvaridle); /* Inform anyone who
                                                          cares that we are
                                                          idle. */
            if (db->killcallbackthread) {
                /* This db is being closed; go away now. */
		PR_Unlock(db->mutex);
                return;
            }
            status = PR_WaitCondVar(cvar, PR_INTERVAL_NO_TIMEOUT);
            db->callbackidle = PR_FALSE;
        }
	call = db->firstpendingcall;
	db->firstpendingcall = NULL;
	db->lastpendingcall = NULL;
	PR_Unlock(db->mutex);
	while (call) {
	    (*call->func)(db, call->closure, &(call->triple), call->action);
	    tmp = call;
	    call = call->next;
	    tdbFreePendingCall(tmp);
	}
        PR_Lock(db->mutex);
    }
}


PRStatus tdbQueueMatchingCallbacks(TDB* db, TDBRecord* record,
				   PRInt32 action)
{
    TDBCallbackInfo* info;
    TDBPendingCall* call;
    PRInt32 i;
    for (info = db->firstcallback ; info ; info = info->nextcallback) {
	if (tdbMatchesRange(record, info->range)) {
	    call = PR_NEWZAP(TDBPendingCall);
	    if (!call) return PR_FAILURE;
	    call->func = info->func;
	    call->closure = info->closure;
	    call->action = action;
	    for (i=0 ; i<3 ; i++) {
		call->triple.data[i] = tdbNodeDup(record->data[i]);
		if (call->triple.data[i] == NULL) {
		    tdbFreePendingCall(call);
		    return PR_FAILURE;
		}
	    }
	    if (db->lastpendingcall) {
		db->lastpendingcall->next = call;
	    }
	    db->lastpendingcall = call;
	    if (db->firstpendingcall == NULL) {
		db->firstpendingcall = call;
	    }
	}
    }
    if (db->firstpendingcall != NULL) {
        /* Kick the background thread.  */
	PR_NotifyAllCondVar(db->callbackcvargo);
    }
    return PR_SUCCESS;
}


PRStatus TDBAddCallback(TDB* db, TDBNodeRange range[3],
			TDBCallbackFunction func, void* closure)
{
    TDBCallbackInfo* info;
    PRInt32 i;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_FAILURE;
    info = PR_NEWZAP(TDBCallbackInfo);
    if (!info) return PR_FAILURE;
    for (i=0 ; i<3 ; i++) {
	if (range[i].min) {
	    info->range[i].min = tdbNodeDup(range[i].min);
	    if (info->range[i].min == NULL) goto FAIL;
	}
	if (range[i].max) {
	    info->range[i].max = tdbNodeDup(range[i].max);
	    if (info->range[i].max == NULL) goto FAIL;
	}
    }
    info->func = func;
    info->closure = closure;
    info->nextcallback = db->firstcallback;
    PR_Lock(db->mutex);
    db->firstcallback = info;
    PR_Unlock(db->mutex);
    return PR_SUCCESS;

 FAIL:
    tdbFreeCallbackInfo(info);
    return PR_FAILURE;
}


PRStatus TDBRemoveCallback(TDB* db, TDBNodeRange range[3],
                           TDBCallbackFunction func, void* closure)
{
    PRStatus status = PR_FAILURE;
    TDBCallbackInfo** ptr;
    TDBCallbackInfo* info;
    PRInt32 i;
    PRBool match;
    PR_ASSERT(db != NULL);
    if (db == NULL) return PR_FAILURE;
    PR_Lock(db->mutex);
    for (ptr = &(db->firstcallback) ; *ptr ; ptr = &((*ptr)->nextcallback)) {
	info = *ptr;
	if (info->func == func && info->closure == closure) {
	    match = PR_TRUE;
	    for (i=0 ; i<3 ; i++) {
		if (range[i].min != info->range[i].min &&
		    (range[i].min == NULL || info->range[i].min == NULL ||
		     tdbCompareNodes(range[i].min, info->range[i].min) != 0)) {
		    match = PR_FALSE;
		    break;
		}
		if (range[i].max != info->range[i].max &&
		    (range[i].max == NULL || info->range[i].max == NULL ||
		     tdbCompareNodes(range[i].max, info->range[i].max) != 0)) {
		    match = PR_FALSE;
		    break;
		}
	    }
	    if (match) {
		*ptr = info->nextcallback;
		tdbFreeCallbackInfo(info);
		status = PR_SUCCESS;

		/* We now make sure that we have no outstanding callbacks
		   queued up to the callback we just removed.  It would be
		   real bad to call that callback after we return.  So, we
		   make sure to call it now. */
		while (db->firstpendingcall) {
                    PR_NotifyAllCondVar(db->callbackcvargo);
                    PR_WaitCondVar(db->callbackcvaridle,
                                   PR_INTERVAL_NO_TIMEOUT);
		}

		break;
	    }
	}
    }
    PR_Unlock(db->mutex);
    return status;
}
