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
#include "mkutils.h"
#include "mksort.h"
#include "xp_qsort.h"

#define CHUNK_SIZE 400

#ifdef PROFILE
#pragma profile on
#endif


MODULE_PRIVATE SortStruct *
NET_SortInit (void)
{

    SortStruct * sort_struct = XP_NEW(SortStruct);

    if(!sort_struct)
	return(0);

    sort_struct->list = (void **) XP_ALLOC((sizeof(void *) * CHUNK_SIZE));

    if(!sort_struct->list)
	return(0);

    sort_struct->cur_size = CHUNK_SIZE;
    sort_struct->num_entries = 0;

    return(sort_struct);
}

MODULE_PRIVATE Bool
NET_SortAdd (SortStruct * sort_struct, void * add_object)
{
	if(!sort_struct)
	  {
		TRACEMSG(("Trying to add to NULL sort_struct"));
		return(0);
	  }

    if(sort_struct->cur_size == sort_struct->num_entries)
      {  /* whoops the list was too small, expand it */

        sort_struct->cur_size += CHUNK_SIZE;
		sort_struct->list = (void **) XP_REALLOC(sort_struct->list, 
								(sizeof(void *) * sort_struct->cur_size));

		if(!sort_struct->list)
	    	return(0);  /* very bad! */
      }

    sort_struct->list[sort_struct->num_entries++] = add_object;

    return(1);

}

MODULE_PRIVATE Bool
NET_SortInsert(SortStruct * sort_struct, void * insert_before, void * new_object)
{
	int i;

    if(sort_struct->cur_size == sort_struct->num_entries)
      {  /* whoops the list was too small, expand it */

        sort_struct->cur_size += CHUNK_SIZE;
        sort_struct->list = (void **) XP_REALLOC(sort_struct->list,
                    			(sizeof(void *) * sort_struct->cur_size));

    	if(!sort_struct->list)
        	return(0);  /* very bad! */
      }

    /* find the insert_before object in the list
     */
	for(i=0; i < sort_struct->num_entries; i++)
      {
		if(sort_struct->list[i] == insert_before)
			break;
	  }

	if(sort_struct->list[i] == insert_before)
	  {
		void * tmp_ptr1;
		void * tmp_ptr2;

		tmp_ptr2 = new_object;

		for(; i < sort_struct->num_entries; i++)
		  {
		    tmp_ptr1 = sort_struct->list[i];
		    sort_struct->list[i] = tmp_ptr2;
			tmp_ptr2 = tmp_ptr1;
		  }

		sort_struct->list[i] = tmp_ptr2;
		sort_struct->num_entries++;
	  }
	else
	  {
		return(0);
	  }

    return(1);
}

MODULE_PRIVATE void
NET_DoSort(SortStruct * sort_struct, int (*compar) (const void *, const void *))
{
    XP_QSORT(sort_struct->list, sort_struct->num_entries, sizeof(void *), compar);
} 

/* unloads backwards :(
 */
MODULE_PRIVATE void *
NET_SortUnloadNext(SortStruct * sort_struct)
{
     if(sort_struct->num_entries < 1)
	return(0);

     return(sort_struct->list[--sort_struct->num_entries]);
}

MODULE_PRIVATE void *
NET_SortRetrieveNumber(SortStruct * sort_struct, int number)
{
     if(number >= sort_struct->num_entries)
	return(0);

     return(sort_struct->list[number]);
}

MODULE_PRIVATE int
NET_SortCount(SortStruct * sort_struct)
{
    return(sort_struct->num_entries);
}

MODULE_PRIVATE void 
NET_SortFree(SortStruct * sort_struct)
{
    XP_FREE(sort_struct->list);
    XP_FREE(sort_struct);
}

#ifdef PROFILE
#pragma profile off
#endif

