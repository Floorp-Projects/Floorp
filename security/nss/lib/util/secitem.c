/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Support routines for SECItem data structure.
 *
 * $Id: secitem.c,v 1.17 2012/03/23 03:12:16 wtc%google.com Exp $
 */

#include "seccomon.h"
#include "secitem.h"
#include "base64.h"
#include "secerr.h"

SECItem *
SECITEM_AllocItem(PRArenaPool *arena, SECItem *item, unsigned int len)
{
    SECItem *result = NULL;
    void *mark = NULL;

    if (arena != NULL) {
	mark = PORT_ArenaMark(arena);
    }

    if (item == NULL) {
	if (arena != NULL) {
	    result = PORT_ArenaZAlloc(arena, sizeof(SECItem));
	} else {
	    result = PORT_ZAlloc(sizeof(SECItem));
	}
	if (result == NULL) {
	    goto loser;
	}
    } else {
	PORT_Assert(item->data == NULL);
	result = item;
    }

    result->len = len;
    if (len) {
	if (arena != NULL) {
	    result->data = PORT_ArenaAlloc(arena, len);
	} else {
	    result->data = PORT_Alloc(len);
	}
	if (result->data == NULL) {
	    goto loser;
	}
    } else {
	result->data = NULL;
    }

    if (mark) {
	PORT_ArenaUnmark(arena, mark);
    }
    return(result);

loser:
    if ( arena != NULL ) {
	if (mark) {
	    PORT_ArenaRelease(arena, mark);
	}
	if (item != NULL) {
	    item->data = NULL;
	    item->len = 0;
	}
    } else {
	if (result != NULL) {
	    SECITEM_FreeItem(result, (item == NULL) ? PR_TRUE : PR_FALSE);
	}
	/*
	 * If item is not NULL, the above has set item->data and
	 * item->len to 0.
	 */
    }
    return(NULL);
}

SECStatus
SECITEM_ReallocItem(PRArenaPool *arena, SECItem *item, unsigned int oldlen,
		    unsigned int newlen)
{
    PORT_Assert(item != NULL);
    if (item == NULL) {
	/* XXX Set error.  But to what? */
	return SECFailure;
    }

    /*
     * If no old length, degenerate to just plain alloc.
     */
    if (oldlen == 0) {
	PORT_Assert(item->data == NULL || item->len == 0);
	if (newlen == 0) {
	    /* Nothing to do.  Weird, but not a failure.  */
	    return SECSuccess;
	}
	item->len = newlen;
	if (arena != NULL) {
	    item->data = PORT_ArenaAlloc(arena, newlen);
	} else {
	    item->data = PORT_Alloc(newlen);
	}
    } else {
	if (arena != NULL) {
	    item->data = PORT_ArenaGrow(arena, item->data, oldlen, newlen);
	} else {
	    item->data = PORT_Realloc(item->data, newlen);
	}
    }

    if (item->data == NULL) {
	return SECFailure;
    }

    return SECSuccess;
}

SECComparison
SECITEM_CompareItem(const SECItem *a, const SECItem *b)
{
    unsigned m;
    int rv;

    if (a == b)
    	return SECEqual;
    if (!a || !a->len || !a->data) 
        return (!b || !b->len || !b->data) ? SECEqual : SECLessThan;
    if (!b || !b->len || !b->data) 
    	return SECGreaterThan;

    m = ( ( a->len < b->len ) ? a->len : b->len );
    
    rv = PORT_Memcmp(a->data, b->data, m);
    if (rv) {
	return rv < 0 ? SECLessThan : SECGreaterThan;
    }
    if (a->len < b->len) {
	return SECLessThan;
    }
    if (a->len == b->len) {
	return SECEqual;
    }
    return SECGreaterThan;
}

PRBool
SECITEM_ItemsAreEqual(const SECItem *a, const SECItem *b)
{
    if (a->len != b->len)
        return PR_FALSE;
    if (!a->len)
    	return PR_TRUE;
    if (!a->data || !b->data) {
        /* avoid null pointer crash. */
	return (PRBool)(a->data == b->data);
    }
    return (PRBool)!PORT_Memcmp(a->data, b->data, a->len);
}

SECItem *
SECITEM_DupItem(const SECItem *from)
{
    return SECITEM_ArenaDupItem(NULL, from);
}

SECItem *
SECITEM_ArenaDupItem(PRArenaPool *arena, const SECItem *from)
{
    SECItem *to;
    
    if ( from == NULL ) {
	return(NULL);
    }
    
    if ( arena != NULL ) {
	to = (SECItem *)PORT_ArenaAlloc(arena, sizeof(SECItem));
    } else {
	to = (SECItem *)PORT_Alloc(sizeof(SECItem));
    }
    if ( to == NULL ) {
	return(NULL);
    }

    if ( arena != NULL ) {
	to->data = (unsigned char *)PORT_ArenaAlloc(arena, from->len);
    } else {
	to->data = (unsigned char *)PORT_Alloc(from->len);
    }
    if ( to->data == NULL ) {
	PORT_Free(to);
	return(NULL);
    }

    to->len = from->len;
    to->type = from->type;
    if ( to->len ) {
	PORT_Memcpy(to->data, from->data, to->len);
    }
    
    return(to);
}

SECStatus
SECITEM_CopyItem(PRArenaPool *arena, SECItem *to, const SECItem *from)
{
    to->type = from->type;
    if (from->data && from->len) {
	if ( arena ) {
	    to->data = (unsigned char*) PORT_ArenaAlloc(arena, from->len);
	} else {
	    to->data = (unsigned char*) PORT_Alloc(from->len);
	}
	
	if (!to->data) {
	    return SECFailure;
	}
	PORT_Memcpy(to->data, from->data, from->len);
	to->len = from->len;
    } else {
	/*
	 * If from->data is NULL but from->len is nonzero, this function
	 * will succeed.  Is this right?
	 */
	to->data = 0;
	to->len = 0;
    }
    return SECSuccess;
}

void
SECITEM_FreeItem(SECItem *zap, PRBool freeit)
{
    if (zap) {
	PORT_Free(zap->data);
	zap->data = 0;
	zap->len = 0;
	if (freeit) {
	    PORT_Free(zap);
	}
    }
}

void
SECITEM_ZfreeItem(SECItem *zap, PRBool freeit)
{
    if (zap) {
	PORT_ZFree(zap->data, zap->len);
	zap->data = 0;
	zap->len = 0;
	if (freeit) {
	    PORT_ZFree(zap, sizeof(SECItem));
	}
    }
}
/* these reroutines were taken from pkix oid.c, which is supposed to
 * replace this file some day */
/*
 * This is the hash function.  We simply XOR the encoded form with
 * itself in sizeof(PLHashNumber)-byte chunks.  Improving this
 * routine is left as an excercise for the more mathematically
 * inclined student.
 */
PLHashNumber PR_CALLBACK
SECITEM_Hash ( const void *key)
{
    const SECItem *item = (const SECItem *)key;
    PLHashNumber rv = 0;

    PRUint8 *data = (PRUint8 *)item->data;
    PRUint32 i;
    PRUint8 *rvc = (PRUint8 *)&rv;

    for( i = 0; i < item->len; i++ ) {
        rvc[ i % sizeof(rv) ] ^= *data;
        data++;
    }

    return rv;
}

/*
 * This is the key-compare function.  It simply does a lexical
 * comparison on the item data.  This does not result in
 * quite the same ordering as the "sequence of numbers" order,
 * but heck it's only used internally by the hash table anyway.
 */
PRIntn PR_CALLBACK
SECITEM_HashCompare ( const void *k1, const void *k2)
{
    const SECItem *i1 = (const SECItem *)k1;
    const SECItem *i2 = (const SECItem *)k2;

    return SECITEM_ItemsAreEqual(i1,i2);
}
