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
 * The Original Code is the PKIX-C library.
 *
 * The Initial Developer of the Original Code is
 * Sun Microsystems, Inc.
 * Portions created by the Initial Developer are
 * Copyright 2004-2007 Sun Microsystems, Inc.  All Rights Reserved.
 *
 * Contributor(s):
 *   Sun Microsystems, Inc.
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
 * pkix_pl_primhash.h
 *
 * Primitive Hashtable Definition
 *
 */

#ifndef _PKIX_PL_PRIMHASH_H
#define _PKIX_PL_PRIMHASH_H

#include "pkix_pl_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pkix_pl_HT_Elem pkix_pl_HT_Elem;

typedef struct pkix_pl_PrimHashTable pkix_pl_PrimHashTable;

typedef struct pkix_pl_Integer pkix_pl_Integer;

struct pkix_pl_Integer{
        PKIX_UInt32 ht_int;
};

struct pkix_pl_HT_Elem {
        void *key;
        void *value;
        PKIX_UInt32 hashCode;
        pkix_pl_HT_Elem *next;
};

struct pkix_pl_PrimHashTable {
        pkix_pl_HT_Elem **buckets;
        PKIX_UInt32 size;
};

/* see source file for function documentation */

PKIX_Error *
pkix_pl_PrimHashTable_Create(
        PKIX_UInt32 numBuckets,
        pkix_pl_PrimHashTable **pResult,
        void *plContext);

PKIX_Error *
pkix_pl_PrimHashTable_Add(
        pkix_pl_PrimHashTable *ht,
        void *key,
        void *value,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void *plContext);

PKIX_Error *
pkix_pl_PrimHashTable_Remove(
        pkix_pl_PrimHashTable *ht,
        void *key,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void **pKey,
        void **pValue,
        void *plContext);

PKIX_Error *
pkix_pl_PrimHashTable_Lookup(
        pkix_pl_PrimHashTable *ht,
        void *key,
        PKIX_UInt32 hashCode,
        PKIX_PL_EqualsCallback keyComp,
        void **pResult,
        void *plContext);

PKIX_Error*
pkix_pl_PrimHashTable_Destroy(
        pkix_pl_PrimHashTable *ht,
        void *plContext);

PKIX_Error *
pkix_pl_PrimHashTable_GetBucketSize(
        pkix_pl_PrimHashTable *ht,
        PKIX_UInt32 hashCode,
        PKIX_UInt32 *pBucketSize,
        void *plContext);

PKIX_Error *
pkix_pl_PrimHashTable_RemoveFIFO(
        pkix_pl_PrimHashTable *ht,
        PKIX_UInt32 hashCode,
        void **pKey,
        void **pValue,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_PL_PRIMHASH_H */
