/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef MKSORT_H
#define MKSORT_H

typedef struct _SortStruct {
    int     cur_size;
    int     num_entries;
    void ** list;
} SortStruct;


extern SortStruct * NET_SortInit (void);
extern Bool NET_SortAdd (SortStruct * sort_struct, void * add_object);
extern void NET_DoSort(SortStruct * sort_struct, int (*compar) (const void *, const void *, void *));
extern void * NET_SortUnloadNext(SortStruct * sort_struct);
extern void * NET_SortRetrieveNumber(SortStruct * sort_struct, int number);
extern int    NET_SortCount(SortStruct * sort_struct);
extern Bool NET_SortInsert(SortStruct * sort_struct, void * insert_before, void * new_object);



extern void NET_SortFree(SortStruct * sort_struct);

#endif /* MKSORT_H */
