/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
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
extern void NET_DoSort(SortStruct * sort_struct, int (*compar) (const void *, const void *));
extern void * NET_SortUnloadNext(SortStruct * sort_struct);
extern void * NET_SortRetrieveNumber(SortStruct * sort_struct, int number);
extern int    NET_SortCount(SortStruct * sort_struct);
extern Bool NET_SortInsert(SortStruct * sort_struct, void * insert_before, void * new_object);



extern void NET_SortFree(SortStruct * sort_struct);

#endif /* MKSORT_H */
