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
 * pkix_list.h
 *
 * List Object Type Definition
 *
 */

#ifndef _PKIX_LIST_H
#define _PKIX_LIST_H

#include "pkix_tools.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef PKIX_Error *
(*PKIX_List_SortComparatorCallback)(
        PKIX_PL_Object *obj1,
        PKIX_PL_Object *obj2,
        PKIX_Int32 *pResult,
        void *plContext);

struct PKIX_ListStruct {
        PKIX_PL_Object *item;
        PKIX_List *next;
        PKIX_Boolean immutable;
        PKIX_UInt32 length;
        PKIX_Boolean isHeader;
};

/* see source file for function documentation */

PKIX_Error *pkix_List_RegisterSelf(void *plContext);

PKIX_Error *
pkix_List_Contains(
        PKIX_List *list,
        PKIX_PL_Object *object,
        PKIX_Boolean *pFound,
        void *plContext);

PKIX_Error *
pkix_List_Remove(
        PKIX_List *list,
        PKIX_PL_Object *target,
        void *plContext);

PKIX_Error *
pkix_List_MergeLists(
        PKIX_List *firstList,
        PKIX_List *secondList,
        PKIX_List **pMergedList,
        void *plContext);

PKIX_Error *
pkix_List_AppendList(
        PKIX_List *toList,
        PKIX_List *fromList,
        void *plContext);

PKIX_Error *
pkix_List_AppendUnique(
        PKIX_List *toList,
        PKIX_List *fromList,
        void *plContext);

PKIX_Error *
pkix_List_RemoveItems(
        PKIX_List *list,
        PKIX_List *deleteList,
        void *plContext);

PKIX_Error *
pkix_List_QuickSort(
        PKIX_List *fromList,
        PKIX_List_SortComparatorCallback comparator,
        PKIX_List **pSortedList,
        void *plContext);

PKIX_Error *
pkix_List_BubbleSort(
        PKIX_List *fromList,
        PKIX_List_SortComparatorCallback comparator,
        PKIX_List **pSortedList,
        void *plContext);

#ifdef __cplusplus
}
#endif

#endif /* _PKIX_LIST_H */
