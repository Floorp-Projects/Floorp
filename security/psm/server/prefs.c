/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
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
 * The Original Code is the Netscape security libraries.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 1994-2000 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License Version 2 or later (the
 * "GPL"), in which case the provisions of the GPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the GPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the GPL.  If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */
#if 0
#include "prtypes.h"
#include "prlock.h"
#include "prmem.h"
#include "prio.h"
#include "prlog.h"
#include "prerror.h"
#include "prclist.h"
#include "plstr.h"

#include "prefs.h"

/* Number of hash buckets.  As we add prefs, we should raise this. */
#define NUM_BUCKETS 64

static struct {
    PRLock *lock;
    PRCList list;
} pref_file_list = { NULL };

struct PrefFileStr {
    PRCList link;
    PRLock *lock;
    int ref_cnt;
    PrefFile *next;
    char *filename;
    PRBool modified;
    PLHashTable *hash;
};

typedef struct DefaultPrefs {
    char *key;
    char *value;
} DefaultPrefs;

DefaultPrefs pref_defaults[] = {
    { "enable_ssl2", "true" },
    { "enable_ssl3", "true" },
	{ "enable_tls",	 "true" },
    { NULL, NULL }
};

static PLHashNumber
pref_HashString(const void *key)
{
    PLHashNumber result;
    char *string;

    result = 0;
    string = (char *)key;
    while(*string != '\0') {
	result = (result << 4) + (result >> 28);
	result += *string;
	string++;
    }
    return result;
}

static PRIntn
pref_HashCompareKey(const void *v1, const void *v2)
{
    if (strcmp(v1, v2) == 0)
	return 1;
    else
	return 0;
}

static PRIntn
pref_HashCompareValue(const void *v1, const void *v2)
{
    if (v1 == v2)
	return 1;
    else
	return 0;
}

static void *
pref_allocTable(void *pool, PRSize size)
{
    return PR_Malloc(size);
}

static void
pref_freeTable(void *pool, void *item)
{
    PR_Free(item);
}

static PLHashEntry *
pref_allocEntry(void *pool, const void *key)
{
    return PR_NEW(PLHashEntry);
}

static void
pref_freeEntry(void *pool, PLHashEntry *he, PRUintn flag)
{
    PR_Free(he->value);

    if (flag == HT_FREE_ENTRY)
        PR_DELETE(he);
}

static PLHashAllocOps pref_HashAllocOps = {
    pref_allocTable, pref_freeTable, pref_allocEntry, pref_freeEntry
};

struct StringBufStr {
    char *str;
    int len;
    int space;
};

typedef struct StringBufStr StringBuf;

static StringBuf *
str_create(int space)
{
    StringBuf *buf;

    buf = PR_NEW(StringBuf);
    if (buf == NULL)
	goto loser;

    buf->str = PR_Malloc(space + 1);
    if (buf->str == NULL)
	goto loser;

    buf->str[0] = '\0';
    buf->len = 0;
    buf->space = space;
    return buf;

loser:
    if (buf != NULL) {
	if (buf->str != NULL)
	    PR_Free(buf->str);
	PR_DELETE(buf);
    }
    return NULL;
}

static SSMStatus
str_addchar(StringBuf *buf, char c)
{
    int len, space;

    PR_ASSERT(buf->len <= buf->space);

    /* If we had a previous allocation failure, fail immediately. */
    if (buf->space == 0)
	goto loser;

    if (buf->len == buf->space) {
	buf->space *= 2;
	buf->str = PR_Realloc(buf->str, buf->space + 1);
	if (buf->str == NULL)
	    goto loser;
    }

    buf->str[len] = c;
    len++;
    buf->str[len] = '\0';
    return PR_SUCCESS;

loser:
    buf->space = 0;
    buf->len = 0;
    return PR_FAILURE;
}

static void
str_clear(StringBuf *buf)
{
    buf->len = 0;
    buf->str[0] = '\0';
}

static char *
str_dup(StringBuf *buf)
{
    return PL_strdup(buf->str);
}

static void
str_destroy(StringBuf *buf)
{
    if (buf != NULL) {
	if (buf->str != NULL)
	    PR_Free(buf->str);
	PR_DELETE(buf);
    }
}

typedef enum ParseState { parse_key, parse_value } ParseState;

static SSMStatus
pref_ReadPrefs(PrefFile *prefs)
{
    PRFileDesc *fd;
    StringBuf *keybuf, *valbuf;
    char *readbuf;
    int i, readlen;
    SSMStatus rv;
    char c;
    ParseState parse_state;

    fd = PR_Open(prefs->filename, PR_RDONLY, 0);
    if (fd == NULL) {
	/* No prefs file is okay.  Just don't do anything. */
	if (PR_GetError() == PR_FILE_NOT_FOUND_ERROR)
	    return PR_SUCCESS;
	else
	    return PR_FAILURE;
    }

    keybuf = str_create(128);
    if (keybuf == NULL)
	goto loser;
    valbuf = str_create(128);
    if (valbuf == NULL)
	goto loser;
    readbuf = PR_Malloc(1024);
    if (readbuf == NULL)
	goto loser;

    readlen = PR_Read(fd, readbuf, 1024);
    i = 0;
    while(readlen != 0) {
	c = readbuf[i];
	switch(parse_state) {
	  case parse_key:
	    if (c == ':') {
		parse_state = parse_value;
	    } else { 
		str_addchar(keybuf, c);
	    }
	    break;
	  case parse_value:
	    if (c == '\n') {
		PLHashEntry *entry;
		char *key, *val;

		key = str_dup(keybuf);
		val = str_dup(valbuf);
		entry = PL_HashTableAdd(prefs->hash, key, val);
		if (entry == NULL) {
		    PR_Free(key);
		    PR_Free(val);
		}
		parse_state = parse_key;
	    } else {
		str_addchar(valbuf, c);
	    }
	    break;
	}
	i++;

	if (i == readlen) {
	    readlen = PR_Read(fd, readbuf, 1024);
	    i = 0;
	}
    }
    rv = PR_SUCCESS;

done:
    if (fd != NULL)
	PR_Close(fd);
    if (keybuf != NULL)
	str_destroy(keybuf);
    if (valbuf != NULL)
	str_destroy(valbuf);
    if (readbuf != NULL)
	PR_Free(readbuf);
    return rv;
loser:
    rv = PR_FAILURE;
    goto done;
}

static PrefFile *
pref_OpenNewPrefFile(char *filename)
{
    PrefFile *prefs;
    PRFileDesc *fd;
    SSMStatus status;

    prefs = PR_NEW(PrefFile);
    if (prefs == NULL)
	goto loser;

    prefs->lock = PR_NewLock();
    if (prefs->lock == NULL)
	goto loser;

    prefs->filename = PL_strdup(filename);
    if (prefs->filename == NULL)
	goto loser;

    prefs->hash = PL_NewHashTable(NUM_BUCKETS, pref_HashString,
				  pref_HashCompareKey, pref_HashCompareValue,
				  NULL, NULL);
    if (prefs->hash == NULL)
	goto loser;

    status = pref_ReadPrefs(prefs);
    if (status != PR_SUCCESS)
	goto loser;

    prefs->ref_cnt = 1;

    return prefs;
loser:
    if (prefs != NULL) {
	if (prefs->lock != NULL)
	    PR_DestroyLock(prefs->lock);
	if (prefs->filename != NULL)
	    PR_Free(prefs->filename);
	if (prefs->hash != NULL) {
	    PL_HashTableDestroy(prefs->hash);
	}

	PR_DELETE(prefs);
    }
    return NULL;
}

PrefFile *
PREF_OpenPrefs(char *filename)
{
    PrefFile *prefs;
    PRCList *link, *list;

    /* Init the global file list if this is the first time. */
    if (pref_file_list.lock == NULL) {
	pref_file_list.lock = PR_NewLock();
	if (pref_file_list.lock == NULL)
	    return NULL;
	PR_INIT_CLIST(&pref_file_list.list);
    }

    /* First, check to see if we've already got this file open. */
    PR_Lock(pref_file_list.lock);
    list = &pref_file_list.list;
    for (link = PR_LIST_HEAD(list); link != list; link = PR_NEXT_LINK(link)) {
	prefs = (PrefFile *)link;
	if (PL_strcmp(filename, prefs->filename) == 0) {
	    PR_Lock(prefs->lock);
	    prefs->ref_cnt++;
	    PR_Unlock(prefs->lock);
	    PR_Unlock(pref_file_list.lock);
	    return prefs;
	}
    }

    /* We don't already have this prefs file open, so open it. */
    prefs = pref_OpenNewPrefFile(filename);
    if (prefs == NULL)
	goto loser;
    PR_INSERT_BEFORE(&prefs->link, list);
    PR_Unlock(pref_file_list.lock);
    return prefs;

loser:
    PR_Unlock(pref_file_list.lock);
    return NULL;
}

static PRIntn
pref_writeEnumerator(PLHashEntry *he, PRIntn i, void *arg)
{
    PRFileDesc *fd = (PRFileDesc *)arg;

    PR_Write(fd, he->key, PL_strlen(he->key));
    PR_Write(fd, ":", 1);
    PR_Write(fd, he->value, PL_strlen(he->value));
    PR_Write(fd, "\n", 1);

    return HT_ENUMERATE_NEXT;
}

SSMStatus
PREF_WritePrefs(PrefFile *prefs)
{
    PRFileDesc *fd;
    SSMStatus status = PR_SUCCESS;

    PR_Lock(prefs->lock);
    if (prefs->modified == PR_FALSE)
	goto done;

    fd = PR_Open(prefs->filename, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE, 0600);
    if (fd == NULL) {
	status = PR_FAILURE;
	goto done;
    }

    PL_HashTableEnumerateEntries(prefs->hash, pref_writeEnumerator, fd);
done:
    PR_Unlock(prefs->lock);
    PR_Close(fd);
    return status;
}

SSMStatus
PREF_ClosePrefs(PrefFile *prefs)
{
    SSMStatus status;

    PR_Lock(pref_file_list.lock);
    PR_Lock(prefs->lock);

    PR_ASSERT(prefs->ref_cnt >= 1);
    prefs->ref_cnt--;

    if (prefs->ref_cnt > 0) {
	PR_Unlock(prefs->lock);
	PR_Unlock(pref_file_list.lock);
	return PR_SUCCESS;
    }

    PR_REMOVE_LINK(&prefs->link);
    PR_Unlock(pref_file_list.lock);

    status = PREF_WritePrefs(prefs);
    PR_DestroyLock(prefs->lock);
    PR_Free(prefs->filename);
    PL_HashTableDestroy(prefs->hash);
    PR_Free(prefs);

    return status;
}

static SSMStatus
pref_SetStringPref_unlocked(PrefFile *prefs, char *name, char *value)
{
    PLHashEntry *entry;
    char *key, *val;

    key = PL_strdup(key);
    if (key == NULL)
	goto loser;
    val = PL_strdup(value);
    if (val == NULL)
	goto loser;

    entry = PL_HashTableAdd(prefs->hash, key, val);
    if (entry == NULL)
	goto loser;

    return PR_SUCCESS;

loser:
    if (key != NULL)
	PR_Free(key);
    if (val != NULL)
	PR_Free(val);

    return PR_FAILURE;
}

SSMStatus
PREF_SetStringPref(PrefFile *prefs, char *name, char *value)
{
    SSMStatus status;

    PR_Lock(prefs->lock);
    status = pref_SetStringPref_unlocked(prefs, name, value);
    prefs->modified = PR_TRUE;
    PR_Unlock(prefs->lock);
    return status;
}

char *
PREF_GetStringPref(PrefFile *prefs, char *name)
{
    char *val;

    PR_Lock(prefs->lock);
    val = PL_HashTableLookup(prefs->hash, name);
    if (val != NULL)
	val = PL_strdup(val);
    PR_Unlock(prefs->lock);
    return val;
}

static SSMStatus
pref_SetDefaultPrefs(PrefFile *prefs)
{
    DefaultPrefs *dp;
    SSMStatus status = PR_SUCCESS;

    PR_Lock(prefs->lock);
    dp = pref_defaults;
    while(dp->key != NULL) {
	status = pref_SetStringPref_unlocked(prefs, dp->key, dp->value);
	if (status != PR_SUCCESS)
	    goto loser;
	dp++;
    }
loser:
    PR_Unlock(prefs->lock);
    return status;
}

#else    /* PREFS LITE (tm) */
/*------------------------------------------------------*/
#include "nspr.h"
#include "prtypes.h"
#include "prclist.h"
#include "prlock.h"
#include "prmem.h"
#include "prlog.h"
#include "plstr.h"

#include "prefs.h"


struct PrefSetStr {
    PRCList list;
    PRLock* lock;
    PRBool modified;
};

typedef struct PrefItemStr {
    char* key;
    char* value;
} PrefItem;

typedef struct PrefItemNodeStr {
    PRCList link;
    PrefItem* item;
} PrefItemNode;


/* Default values with which a PrefSet is initialized
 */ 
PrefItem pref_defaults[] = {
    {"security.enable_ssl2", "true"},
    {"security.enable_ssl3", "true"},
    {"security.enable_tls", "true"},
    {"security.default_personal_cert", "Ask Every Time"},
    {"security.default_mail_cert", NULL},
    {"security.ask_for_password", "0"},
    {"security.password_lifetime", "30"},
    {"security.OCSP.enabled", "false"},
    {"security.warn_entering_secure", "true"},
    {"security.warn_leaving_secure", "true"},
    {"security.warn_viewing_mixed", "true"},
    {"security.warn_submit_insecure", "true"},
    {"mail.encrypt_outgoing_mail", "false"},
    {"mail.crypto_sign_outgoing_mail", "false"},
    {"mail.crypto_sign_outgoing_news", "false"},
    {NULL, NULL}
};


char* STRING_TRUE = "true";
char* STRING_FALSE = "false";


static void pref_free_pref_item(PrefItem* pref)
{
    if (pref != NULL) {
        if (pref->key != NULL) {
            PR_Free(pref->key);
        }
        if (pref->value != NULL) {
            PR_Free(pref->value);
        }
        PR_Free(pref);
    }
}

static PrefItem* pref_new_pref_item(char* key, char* value)
{
    PrefItem* tmp = NULL;

    PR_ASSERT(key != NULL);
    tmp = (PrefItem*)PR_NEWZAP(PrefItem);
    if (tmp == NULL) {
        return tmp;
    }
    
    tmp->key = PL_strdup(key);
    if (tmp->key == NULL) {
        goto loser;
    }
    if (value != NULL) {
        tmp->value = PL_strdup(value);
        if (tmp->value == NULL) {
            goto loser;
        }
    }
    else {
        tmp->value = NULL;
    }

    return tmp;
loser:
    pref_free_pref_item(tmp);
    return NULL;
}

static PrefItemNode* pref_new_pref_item_node(PrefItem* item)
{
    PrefItemNode* node = NULL;

    PR_ASSERT(item != NULL);

    node = (PrefItemNode*)PR_NEWZAP(PrefItemNode);
    if (node == NULL) {
        /* failed: bail out */
        return node;
    }

    /* initialize elements to the right values */
    PR_INIT_CLIST(&node->link);
    node->item = item;

    return node;
}


/* Append a PrefItem to the list */
static SSMStatus pref_append_item_unlocked(PrefItem* pref, PRCList* list)
{
    PrefItemNode* node = NULL;

    PR_ASSERT((pref != NULL) && (list != NULL));

    node = pref_new_pref_item_node(pref);
    if (node == NULL) {
        return PR_FAILURE;
    }

    /* append the link */
    PR_APPEND_LINK(&node->link, list);
    return PR_SUCCESS;
}


/* Append a PrefItem made from (key, value) into the list */
static SSMStatus pref_append_pref_unlocked(PrefSet* prefs, char* key, 
                                          char* value)
{
    PrefItem* pref = NULL;

    pref = pref_new_pref_item(key, value);
    if (pref == NULL) {
        return PR_FAILURE;
    }

    return pref_append_item_unlocked(pref, &prefs->list);
}


/* Modify (old) by comparing with (new)
 * - returns PR_SUCCESS if everything went right; PR_FAILURE if PL_strdup()
 *   failed
 */
static SSMStatus pref_reset_value(char** oldStr, char* newStr)
{
    PR_ASSERT(oldStr != NULL);

    if (newStr == NULL) {
        if (*oldStr != NULL) {
            PR_Free(*oldStr);
            *oldStr = newStr;
        }
        return PR_SUCCESS;
    }

    if (*oldStr != NULL) {
        if (PL_strcmp(*oldStr, newStr) != 0) {
            /* different: reset it */
            PR_Free(*oldStr);
            *oldStr = PL_strdup(newStr);
            if (*oldStr == NULL) {
                return PR_FAILURE;
            }
        }
    }
    else {
        *oldStr = PL_strdup(newStr);
        if (*oldStr == NULL) {
            return PR_FAILURE;
        }
    }
    return PR_SUCCESS;
}


/* Retrieve (value) that belongs to (key) in (prefs)
 * - return PR_SUCCESS if key exists; PR_FAILURE otherwise
 * - (value) may be NULL
 * - returned (value) is a pointer to an existing string, not a new string
 */

static SSMStatus pref_get_pref_unlocked(PrefSet* prefs, char* key, char** value)
{
    PRCList* link = NULL;

    PR_ASSERT((prefs != NULL) && (key != NULL) && (value != NULL));

    /* in case of failure */
    *value = NULL;

    /* first make sure the list is not empty */
    if (PR_CLIST_IS_EMPTY(&prefs->list)) {
        return PR_FAILURE;
    }

    /* walk the list to find the key */
    for (link = PR_LIST_HEAD(&prefs->list); link != &prefs->list;
         link = PR_NEXT_LINK(link)) {
        PrefItemNode* node = (PrefItemNode*)link;
        if (PL_strcmp(node->item->key, key) == 0) {
            /* found the key: return the value whatever it is */
            *value = node->item->value;
            return PR_SUCCESS;
        }
    }

    /* we couldn't find the key: return NULL */
    return PR_FAILURE;
}

/* Set (value) for (key) in (prefs)
 * - if (key) exists, reset (value) (if necessary)
 * - if (key) does not exist, insert this item into (prefs)
 * - we do not care if (value) is NULL
 * - when new elements are created, they are allocated on the heap
 */
static SSMStatus pref_set_pref_unlocked(PrefSet* prefs, char* key, char* value)
{
    PRCList* link = NULL;

    PR_ASSERT((prefs != NULL) && (key != NULL));

    if (!PR_CLIST_IS_EMPTY(&prefs->list)) {
        for (link = PR_LIST_HEAD(&prefs->list); link != &prefs->list;
             link = PR_NEXT_LINK(link)) {
            PrefItemNode* node = (PrefItemNode*)link;
            if (PL_strcmp(node->item->key, key) == 0) {
                /* found the key */
                return pref_reset_value(&node->item->value, value);
            }
        }
    }

    /* either the list was empty or the key was not found */
    return pref_append_pref_unlocked(prefs, key, value);
}

/* Compare (value) with the stored value under (key)
 * - returns PR_TRUE if the key does not exist or the values do not match
 * - returns PR_FALSE only if the key exists and the values match
 */
static PRBool pref_pref_changed_unlocked(PrefSet* prefs, char* key, 
                                         char* value)
{
    PRCList* link = NULL;
    PRBool rv = PR_TRUE;

    PR_ASSERT((prefs != NULL) && (key != NULL));

    if (!PR_CLIST_IS_EMPTY(&prefs->list)) {
        for (link = PR_LIST_HEAD(&prefs->list); link != &prefs->list;
             link = PR_NEXT_LINK(link)) {
            PrefItemNode* node = (PrefItemNode*)link;
            if (PL_strcmp(node->item->key, key) == 0) {
                /* found the key */

                if (((value == NULL) && (node->item->value == NULL)) ||
                    (value != NULL) && (node->item->value != NULL) &&
                    (PL_strcmp(node->item->value, value) == 0)) {
                    rv = PR_FALSE;
                }
                goto done;
            }
        }
        /* exhausted the list but could not find the key */
    }
done:
    return rv;
}

static void pref_clean_out_list(PRCList* list)
{
    PRCList* link = NULL;

    PR_ASSERT(list != NULL);

    if (PR_CLIST_IS_EMPTY(list)) {
        /* we're already done */
        return;
    }

    link = PR_LIST_HEAD(list); 
    while (link != list) {
        PrefItemNode* node = (PrefItemNode*)link;
        pref_free_pref_item(node->item);
        link = PR_NEXT_LINK(link);
        if (node != NULL) {
            PR_Free(node);
        }
    }
    return;
}


PrefSet* PREF_NewPrefs(void)
{
    SSMStatus status;
    PrefSet* prefs = NULL;
    int i;

    /* initialize the structure */
    prefs = (PrefSet*)PR_NEWZAP(PrefSet);
    if (prefs == NULL) {
        goto loser;
    }

    prefs->lock = PR_NewLock();
    if (prefs->lock == NULL) {
        goto loser;
    }

    PR_INIT_CLIST(&prefs->list);

    /* fill the list with default data */
    for (i = 0; pref_defaults[i].key != NULL; i++) {
        PR_Lock(prefs->lock);
        status = pref_append_pref_unlocked(prefs, pref_defaults[i].key,
                                           pref_defaults[i].value);
        PR_Unlock(prefs->lock);
        if (status != PR_SUCCESS) {
            goto loser;
        }
    }

    return prefs;
loser:
    if (prefs != NULL) {
        if (prefs->lock != NULL) {
            PR_DestroyLock(prefs->lock);
        }
        PR_Free(prefs);
    }
    return NULL;
}

SSMStatus PREF_ClosePrefs(PrefSet* prefs)
{
    if (prefs == NULL) {
        goto loser;
    }
    if (prefs->lock == NULL) {
        goto loser;
    }

    /* clean out the list */
    PR_Lock(prefs->lock);
    pref_clean_out_list(&prefs->list);
    PR_Unlock(prefs->lock);

    PR_DestroyLock(prefs->lock);

    PR_Free(prefs);

    return PR_SUCCESS;
loser:
    return PR_FAILURE;
}


SSMStatus PREF_GetStringPref(PrefSet* prefs, char* key, char** value)
{
    SSMStatus rv;

    /* in case of failure */
    *value = NULL;

    if ((prefs == NULL) || (key == NULL) || (value == NULL)) {
        return PR_FAILURE;
    }

    PR_Lock(prefs->lock);
    rv = pref_get_pref_unlocked(prefs, key, value);
    PR_Unlock(prefs->lock);

    return rv;
}

SSMStatus PREF_CopyStringPref(PrefSet* prefs, char* key, char** value)
{
    SSMStatus rv;
    char* tmp = NULL;

    *value = NULL;
    rv = PREF_GetStringPref(prefs, key, &tmp);
    if (tmp != NULL) {
        *value = PL_strdup(tmp);
        if (*value == NULL && rv == PR_SUCCESS) {
            rv = PR_FAILURE;
        }
    }

    return rv;
}

SSMStatus PREF_SetStringPref(PrefSet* prefs, char* key, char* value)
{
    SSMStatus rv;

    if ((prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    PR_Lock(prefs->lock);
    rv = pref_set_pref_unlocked(prefs, key, value);
    PR_Unlock(prefs->lock);

    return rv;
}

PRBool PREF_StringPrefChanged(PrefSet* prefs, char* key, char* value)
{
    PRBool rv;

    PR_ASSERT((prefs != NULL) && (key != NULL));
    if ((prefs == NULL) || (key == NULL)) {
        return PR_TRUE; /* this really is an error */
    }

    PR_Lock(prefs->lock);
    rv = pref_pref_changed_unlocked(prefs, key, value);
    PR_Unlock(prefs->lock);

    return rv;
}


SSMStatus PREF_GetBoolPref(PrefSet* prefs, char* key, PRBool* value)
{
    SSMStatus rv;
    char* tmp = NULL;

    if ((prefs == NULL) || (key == NULL) || (value == NULL)) {
        return PR_FAILURE;
    }

    PR_Lock(prefs->lock);
    rv = pref_get_pref_unlocked(prefs, key, &tmp);
    PR_Unlock(prefs->lock);

    if (rv != PR_SUCCESS) {
        /* pref item was not found */
        return rv;
    }

    if (PL_strcmp(tmp, STRING_TRUE) == 0) {
        *value = PR_TRUE;
    }
    else if (PL_strcmp(tmp, STRING_FALSE) == 0) {
        *value = PR_FALSE;
    }
    else {
        return PR_FAILURE;
    }

    return PR_SUCCESS;
}


SSMStatus PREF_SetBoolPref(PrefSet* prefs, char* key, PRBool value)
{
    SSMStatus rv;
    char* tmp = NULL;

    if ((prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    if (value == PR_FALSE) {
        tmp = STRING_FALSE;
    }
    else if (value == PR_TRUE) {
        tmp = STRING_TRUE;
    }
    else {
        PR_ASSERT(0);
    }

    PR_Lock(prefs->lock);
    rv = pref_set_pref_unlocked(prefs, key, tmp);
    PR_Unlock(prefs->lock);

    return rv;
}

PRBool PREF_BoolPrefChanged(PrefSet* prefs, char* key, PRBool value)
{
    PRBool rv;
    char* tmp = NULL;

    PR_ASSERT((prefs != NULL) && (key != NULL));
    if ((prefs == NULL) || (key == NULL)) {
        return PR_TRUE; /* this really is an error */
    }

    if (value == PR_FALSE) {
        tmp = STRING_FALSE;
    }
    else if (value == PR_TRUE) {
        tmp = STRING_TRUE;
    }
    else {
        PR_ASSERT(0);
    }

    PR_Lock(prefs->lock);
    rv = pref_pref_changed_unlocked(prefs, key, tmp);
    PR_Unlock(prefs->lock);

    return rv;
}


SSMStatus PREF_GetIntPref(PrefSet* prefs, char* key, PRIntn* value)
{
    SSMStatus rv;
    char* tmp = NULL;

    if ((prefs == NULL) || (key == NULL) || (value == NULL)) {
        return PR_FAILURE;
    }

    PR_Lock(prefs->lock);
    rv = pref_get_pref_unlocked(prefs, key, &tmp);
    PR_Unlock(prefs->lock);

    if (rv != PR_SUCCESS) {
        /* pref item was not found */
        return rv;
    }

    /* convert char* into integer */
    *value = atoi(tmp);

    return PR_SUCCESS;
}


SSMStatus PREF_SetIntPref(PrefSet* prefs, char* key, PRIntn value)
{
    SSMStatus rv;
    char* tmp = NULL;

    if ((prefs == NULL) || (key == NULL)) {
        return PR_FAILURE;
    }

    tmp = PR_smprintf("%d", value);
    if (tmp == NULL) {
        return PR_FAILURE;
    }

    PR_Lock(prefs->lock);
    rv = pref_set_pref_unlocked(prefs, key, tmp);
    PR_Unlock(prefs->lock);

    PR_Free(tmp);
    return rv;
}

PRBool PREF_IntPrefChanged(PrefSet* prefs, char* key, PRIntn value)
{
    PRBool rv;
    char* tmp = NULL;

    PR_ASSERT((prefs != NULL) && (key != NULL));
    if ((prefs == NULL) || (key == NULL)) {
        return PR_TRUE; /* this really is an error */
    }

    tmp = PR_smprintf("%d", value);
    if (tmp == NULL) {
        /* ouch... */
        return PR_TRUE;
    }

    PR_Lock(prefs->lock);
    rv = pref_pref_changed_unlocked(prefs, key, tmp);
    PR_Unlock(prefs->lock);

    PR_Free(tmp);
    return rv;
}

/******************
 * These functions allow to handle multiple options specified in 
 * prefs. 
 ******************/
#include "connect.h" /* don't know what to include to use PR_Zalloc */

SSMStatus pref_append_value(char *** list, char * value)
{
    SSMStatus rv = SSM_SUCCESS;
    static int size, cursize;
    if (!list || !value)
        return SSM_FAILURE;
    
    /* Quick and dirty check if we already have that value in the list - 
     * only check the value directly before. Preferences keys seem to go 
     * in clusters so that might actually work untill there is a better way
     */
    if (*list && **list && !PL_strcmp(value, (*list)[cursize-1]))
        return rv;
    
    if (!*list) {
        size = 12;
        cursize = 0;
        *list = (char **)PORT_ZAlloc(sizeof(char *)* size);
        if (!*list) {
            rv = SSM_ERR_OUT_OF_MEMORY;
            goto done;
        }
    }
    else if (cursize == size-2) { 
        size = size + 12;
        *list = (char **)PR_Realloc(*list, sizeof(char *)*size);
        if (!*list) {
            rv = SSM_ERR_OUT_OF_MEMORY;
            goto done;
        }
        /* clear the memory */
        memset(*list+cursize, 0, sizeof(char*)*(size-cursize));
    }
    (*list)[cursize] = PL_strdup(value);
    cursize++;
    
 done: 
    return rv;
}

/****** retrieve all the values for a parent key:
 * SSMStatus 
 * PREF_CreateChildList(PrefSet * prefs, 
 *                      const char* parent_key, 
 *                      char **child_list);
 * 
 * For parent key x.y retrieves key names directly below x.y :
 * x.y.a, x.y.b, x.y.c, etc.
 *
 * For parent_key == NULL returns all the preferences values. 
 * (Why would anyone want to do that? )
 *
 * Allocates and returns char * array with all the requested values. 
 * Caller is responsible for freeing memory.
 *
 * Used for LDAP server look-up for UI.
 *****/
SSMStatus PREF_CreateChildList(PrefSet * prefs, const char* parent_key, 
                               char ***child_list)
{
    SSMStatus rv = SSM_FAILURE;
    PRCList * link = NULL;
    char * child, * tmp;
    int parent_key_len = parent_key?strlen(parent_key):0;
    
    if (!prefs || !child_list) 
        goto loser;

    *child_list = NULL;
    
    /* walk the list untill find interesting key */
    if (!PR_CLIST_IS_EMPTY(&prefs->list)) {
        for (link = PR_LIST_HEAD(&prefs->list); link != &prefs->list;
             link = PR_NEXT_LINK(link)) {
            PrefItemNode* node = (PrefItemNode*)link;
            if (!parent_key || ((tmp = strstr(node->item->key, parent_key)) 
                                && tmp[parent_key_len] == '.')) {
                /* found the key */
                /* append "child" string: "child" in parent.child.more */
                child = PL_strdup(node->item->key + parent_key_len + 1);
                tmp = strtok(child, ".");
                rv = pref_append_value(child_list, tmp);
                PR_Free(child);
                if (rv != SSM_SUCCESS) 
                    goto loser;
            }
        }
    }
    rv = SSM_SUCCESS;
 loser:
    return rv;
}



#endif    /* PREFS LITE (tm) */
