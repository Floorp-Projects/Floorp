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
#include "mknewsgr.h"
#include "msgcom.h"
#include "mkreg.h"
#include "xp_qsort.h"

#define DYN_ARRAY_INIT_SIZE 2
#define DYN_ARRAY_GROW_BY   2

#define BUFFER_SIZE 2048  /* make sure that this is at least 2048
						   * since I hard coded a reference to it
						   * in an sprintf below :)
						   */

typedef struct _SubgroupList SubgroupList;

typedef struct _NewsGroupStruct {
	SubgroupList  *children;  /* pointer to longer names */
	char          *name;      /* pointer to subname or NULL 
							   * if it completes there
							   */
} NewsGroupStruct;

/* dynamic array struct
 */
struct _SubgroupList {
	int               size;      /* size of */
	NewsGroupStruct  *data;  /* pointer to array of structs */
};

PRIVATE char * NET_CurrentlyLoadedHostname = 0;
PRIVATE XP_Bool NET_CurrentlyLoadedListingIsSecure = 0;
PRIVATE Bool NET_NewsgroupsListDirty = FALSE;
PRIVATE time_t NET_NewsGroupsLastUpdate = 0;
PRIVATE SubgroupList *BaseSGStruct=0;
PRIVATE char *large_buffer=0;

#if 0	/* unused */
/* Given a group name prefix, like "alt.fan", this will return
   a SubgroupList which points to the groups under that level
   ("addams", "authors", "blues-brothers", etc.)
   If the prefix has no children, 0 is returned.
 */
static SubgroupList *
net_find_group_level (MWContext *context, const char *parent_name)
{
  SubgroupList *sl = BaseSGStruct;
  char *name, *base_name, *dot;
  if (!parent_name || !*parent_name)
	return sl;
  name = PL_strdup (parent_name);
  base_name = name;
  if (!name)
	return 0;

  while (name && *name)
	{
	  /* #### If we can assume the group names are sorted, this could
		 do a binary search... */
	  int i;
	  NewsGroupStruct *ng;
	  XP_Bool found = FALSE;
	  dot = PL_strchr (name, '.');
	  if (dot) *dot = 0;

	  for (i = 0, ng = sl->data; i < sl->size; i++, ng++)
		{
		  if (! ng->name)
			break;
		  else if (!PL_strcmp (ng->name, name))
			{
			  sl = ng->children;
			  found = TRUE;
			  break;
			}
		}
	  if (! found)
		{
		  PR_Free (base_name);
		  return 0;
		}
	  name = (dot ? dot + 1 : 0);
	}
  PR_Free (base_name);
  return sl;
}
#endif /* 0 */

static int32
msg_count_descendants (NewsGroupStruct *ng)
{
  int32 kids = 0;
  int32 i;
  if (!ng->children) return 0;
  for (i = 0; i < ng->children->size; i++)
	{
	  if (!ng->children->data[i].name) break;
	  if (ng->children->data[i].name[0] == 0)
		continue;
	  if (ng->children->data[i].children)
		kids += msg_count_descendants (&ng->children->data[i]);
	  else
		kids++;
	}
  return kids;
}

static NewsGroupStruct *
msg_optimize_single_kid (NewsGroupStruct *ng, char **nameP)
{
  int i, count;
  char *new_name;
  NewsGroupStruct *pkid, *kid;
  if (!ng->children)
	return ng;

  for (i = 0, count = 0, pkid = 0, kid = ng->children->data;
	   i < ng->children->size && kid->name;
	   i++, pkid = kid, kid++)
	{
	  if (i == 0 && !*kid->name)
		continue;
	  if (++count > 1)
		return ng;
	}

  if (!pkid || !pkid->name)
	return ng;

  new_name = (char *) PR_Malloc (PL_strlen (*nameP) + PL_strlen (pkid->name)
								+ 3);
  if (!new_name)
	return ng;
  PL_strcpy (new_name, *nameP);
  PL_strcat (new_name, ".");
  PL_strcat (new_name, pkid->name);
  *nameP = new_name;

  /* We have now taken a newsgroup with a single child and promoted that
	 child to this level, prepending to its name.  Now do it again, in
	 case that child also had but a single child. */
  {
	char *name2 = new_name;
	NewsGroupStruct *ng2 = msg_optimize_single_kid (pkid, &name2);
	if (ng2 != pkid)
	  {
		PR_Free (new_name);
		*nameP = name2;
		return ng2;
	  }
  }

  return pkid;
}


/* #### duplicated in msg.h */
typedef int (*msg_NewsgroupNameMapper) (MWContext *context,
										const char *parent_name,
										const char *child_name,
										XP_Bool group_p,
										int32 descendant_group_count,
										void *closure);

#if 0	/* unused */
/* Given a group name prefix, like "alt.fan", this will map the given
   function over each of the groups under that level ("addams", "authors",
   "blues-brothers", etc.)  If the prefix has no children, it fails, and
   never calls the mapper.  If the mapper returns negative, this function
   fails with that result.
 */
PRIVATE
int
msg_MapNewsgroupNames (MWContext *context,
					   const char *news_host_and_port,
					   XP_Bool     is_secure,
					   const char *name_prefix,
					   msg_NewsgroupNameMapper mapper,
					   void *closure)
{
  int i;
  NewsGroupStruct *ng;
  SubgroupList *sl;

  PR_ASSERT (news_host_and_port);
  if (news_host_and_port)
	NET_NewsgroupsLastUpdatedTime ((char*) news_host_and_port, is_secure);

  sl = net_find_group_level (context, name_prefix);
  if (!sl)
	return -1;

  for (i = 0, ng = sl->data; i < sl->size && ng->name; i++, ng++)
	{
	  NewsGroupStruct *ng2 = ng;
	  XP_Bool group_p;
	  int32 descendant_count;
	  int status;
	  char *name;
	  /* If this is the first time through, and this child has the name "",
		 then it indicates that the parent is itself a group - which we
		 already know.  So skip it. */
	  if (i == 0 && !*ng->name)
		continue;

	  name = ng->name;
	  ng2 = msg_optimize_single_kid (ng, &name);

	  /* If we found only a single kid, but we are also a group, then
		 register us first. */
	  if (ng != ng2 && ng->children && ng->children->size > 0 &&
		  ng->children->data[0].name && ng->children->data[0].name[0] == 0) {
		status = (*mapper) (context, name_prefix, ng->name, TRUE, 0, closure);
	  }

	  /* This child is itself a group as well as a container if its first
		 child has the name "", or if it has no children at all. */
	  group_p = (!ng2->children ||
				 (ng2->children->size > 0 &&
				  ng2->children->data[0].name &&
				  ng2->children->data[0].name[0] == 0));
	  descendant_count = msg_count_descendants (ng2);

	  PR_ASSERT(descendant_count != 1); /* because of msg_optimize_single_kid*/

	  status = (*mapper) (context, name_prefix, name,
						  group_p, descendant_count, closure);
	  if (name != ng->name)
		PR_Free (name);
	  if (status < 0) return status;
	}
  return 0;
}
#endif

/* this relies on the fact that strtok
 * keeps a copy of the pointer in static memory
 */
PRIVATE
SubgroupList *
net_recursiveNewsGroupStore(char *subname, 
							Bool  stop_here,
							SubgroupList * SGptr)
{
	SubgroupList * rv;
	NewsGroupStruct *NGptr;
	int cur_num;
	int len;

	if(!SGptr)
	  {
		SGptr = PR_NEW(SubgroupList);	/* FIX ME check for NULL here */
		
		if ( SGptr == NULL)
			return NULL;

		SGptr->size = DYN_ARRAY_INIT_SIZE;
		SGptr->data = (NewsGroupStruct *)
							PR_Malloc(SGptr->size*sizeof(NewsGroupStruct));

		if(!SGptr->data)
		{
			SGptr->size = 0;
           	return(SGptr);
        }
	
		memset(SGptr->data, 0, SGptr->size*sizeof(NewsGroupStruct));
	  }

	rv = SGptr;
	cur_num = 0;
	NGptr = &(SGptr->data)[cur_num];
	len = PL_strlen(subname);

	while(NGptr &&NGptr->name)
	  {

		if(subname 
			&& !PL_strncmp(NGptr->name, subname, len)
				&& (NGptr->name[len] == '\0' || NGptr->name[len] == '.'))
		  {
			/* move comp.infosystems.www
			 * to comp -> infosystems.www
			 */
			if(NGptr->name[len] == '.')
			  {
				PL_strcpy(large_buffer, NGptr->name);
				NGptr->children = net_recursiveNewsGroupStore(
														(&large_buffer[len])+1,	
														TRUE,	
														NGptr->children);
				large_buffer[len] = '\0';
				StrAllocCopy(NGptr->name, large_buffer);
			  }

			break;
		  }
		else
		  {
			if(cur_num < SGptr->size-1)
			  {
			  	NGptr = &(SGptr->data)[++cur_num];
			  }
			else
			  {
				if(NGptr->name != NULL)
					cur_num++; /* increment since the entry is used */
			    NGptr = NULL;
			  }
		  }
	  }

    if(!NGptr || !NGptr->name)
      {
		/* need to create a new one
		 */
		if(cur_num >= SGptr->size)
		  {
			/* grow the dynamic array
			 */	
			int prev_size = SGptr->size;
			SGptr->size += DYN_ARRAY_GROW_BY;
			SGptr->data = (NewsGroupStruct *)
							 PR_Realloc(SGptr->data, 
									    SGptr->size*sizeof(NewsGroupStruct));
			if (SGptr->data == NULL)
			{
				SGptr->size = 0;
            	return(SGptr);
            }

			/* zero out new fields 
			 */
			memset(&(SGptr->data)[prev_size],
					  0,
					  (SGptr->size-prev_size)*sizeof(NewsGroupStruct));

		  }

		NGptr = &(SGptr->data)[cur_num];

		if(stop_here)
		  {
			/* this is a special case to add a single subname
			 * we want to exit here so that we dont
			 * call strtok and mess things up.
			 */
			StrAllocCopy(NGptr->name, subname);
			return(rv);
		  }

		/* copy in the whole name not just the sub name
		 */
		StrAllocCopy(NGptr->name, subname);
	    subname = strtok(NULL, "\0");  /* get the rest of the string */
		if(subname)
		  {
			StrAllocCat(NGptr->name, ".");
			StrAllocCat(NGptr->name, subname);
			subname = NULL;
		  }
      }
	else
      {
	    /* otherwise we found an existing one
	     */
	    subname = strtok(NULL, ".");  /* search for more of the string */

		if((subname && !NGptr->children)
			|| (!subname && NGptr->children) )
			
		  {
			/* this means that we have an existing
			 * complete group name with the same
			 * name as a some of it's childrens heirarcy.
			 * we will represent that as a child
			 * node with a NULL subname
			 */
			NGptr->children = net_recursiveNewsGroupStore("", TRUE, NGptr->children);
		  }
		
      }

	if(subname)
		NGptr->children = net_recursiveNewsGroupStore(subname, FALSE, NGptr->children);
	
	return(rv);

}

/* save a newsgroup name
 *
 * will munge the newsgroup passed in for efficiency
 *
 */
MODULE_PRIVATE void
NET_StoreNewsGroup(char * hostname, XP_Bool is_secure, char * newsgroup)
{
	char * subname;

	if(!newsgroup)
	   return;

	if(!large_buffer)
	   large_buffer = (char *) PR_Malloc(BUFFER_SIZE);
	if(!large_buffer)
		return;

	subname = strtok(newsgroup, ".");

	if(subname)
		BaseSGStruct = net_recursiveNewsGroupStore(subname, FALSE, BaseSGStruct);

	if(!NET_NewsgroupsListDirty)
	  {
		/* if it wasn't dirty before assume this is an update
		 */
		NET_NewsGroupsLastUpdate = time(NULL);
		NET_NewsgroupsListDirty = TRUE;
		StrAllocCopy(NET_CurrentlyLoadedHostname, hostname);
		NET_CurrentlyLoadedListingIsSecure = is_secure;
	  }

}

PRIVATE void
net_recursiveFreeNewsGroups(SubgroupList *SGptr)
{
    NewsGroupStruct *NGptr;
	int cur_num=0;

    while(cur_num < SGptr->size)
      {
    	NGptr = &(SGptr->data)[cur_num++];

		FREEIF(NGptr->name);

        if(NGptr->children)
            net_recursiveFreeNewsGroups(NGptr->children);
      }

	FREE(SGptr);
}

MODULE_PRIVATE void
NET_FreeNewsgroups(char *hostname, XP_Bool is_secure)
{
	if(BaseSGStruct)
	  {
    	net_recursiveFreeNewsGroups(BaseSGStruct);
		BaseSGStruct = 0;
	  }

	FREEIF(large_buffer);

	FREEIF(NET_CurrentlyLoadedHostname);
	NET_CurrentlyLoadedListingIsSecure = 0;

	TRACEMSG(("Freeing Newsgroups"));

}

PRIVATE int
net_NewsGroupsCompar(const void *one,
					 const void *two)
{
	return(PL_strcmp(((const NewsGroupStruct *)one)->name,
					 ((const NewsGroupStruct *)two)->name));
}

PRIVATE void
net_recursiveSortNewsGroups(SubgroupList *SGptr)
{
    NewsGroupStruct *NGptr;
    int total_num=0;

    while(total_num < SGptr->size)
      {
        NGptr = &(SGptr->data)[total_num];

		if(!NGptr->name)
			break;

		total_num++;
      }

/*#ifdef XP_MAC*/
#if 0
    qsort(SGptr->data, 
		  total_num, 
		  sizeof(NewsGroupStruct), 
		  (_Cmpfun*)net_NewsGroupsCompar
		);
#else
    XP_QSORT(SGptr->data, 
		  total_num, 
		  sizeof(NewsGroupStruct), 
		 net_NewsGroupsCompar
		);
#endif

    while(total_num)
      {
        NGptr = &(SGptr->data)[--total_num];

        if(NGptr->children)
			net_recursiveSortNewsGroups(NGptr->children);
      }
}

/* sort the newsgroups
 */
MODULE_PRIVATE void
NET_SortNewsgroups(char *hostname, XP_Bool is_secure)
{
    TRACEMSG(("Freeing Newsgroups"));

    if(BaseSGStruct)
        net_recursiveSortNewsGroups(BaseSGStruct);
}


#if 0
PRIVATE
int net_NumberOfGroupsInLevel(SubgroupList *SGptr, 
							  char * search_string,
							  char * name_so_far,
							  Bool   print_all)
{
	int num_at_this_level=0;
	int cur_num=0;
    NewsGroupStruct *NGptr;

    /* go through this level once to
     * figure out how many there are
     */
    NGptr = &(SGptr->data)[cur_num++];

    while(NGptr->name)
      {

		if(print_all)
		  {
            num_at_this_level++;
		  }
		else
		  {
			if(NGptr->children)
		 	  {
				if(name_so_far)
					PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.%.100s.", name_so_far, NGptr->name);
				else
					PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.", NGptr->name);
		 	  }
			else
		 	  {
				if(name_so_far)
					PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.%.100s", name_so_far, NGptr->name);
				else
					PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", NGptr->name);
		 	  }

TRACEMSG(("Comparing %s and %s", large_buffer, search_string));

			if(!NET_RegExpSearch(large_buffer, search_string))
            	num_at_this_level++;
		  }

        if(cur_num < SGptr->size)
            NGptr = &(SGptr->data)[cur_num++];
        else
            break;
      }

TRACEMSG(("Found %d matches", num_at_this_level));

	return(num_at_this_level);
}

PRIVATE 
int net_recursiveDisplayNewsgroups(MWContext *context,
								   SubgroupList    * SGptr,
								   char            * name_so_far,
                                   char            * search_string,
								   int             * num_to_display,
								   Bool              print_all)
{
    NewsGroupStruct *NGptr;
    int cur_num=0;
	int num_in_level=1;
	int rv=0;
	Bool some_printed=FALSE;
	Bool display_this_one=print_all;
	char *new_name_so_far=0;
	
   	NGptr = &(SGptr->data)[cur_num++];


    while(NGptr->name)
      {

        if(!print_all)
          {
			if(NGptr->children)
			  {
			    if(name_so_far)
            	    PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.%.100s.", name_so_far, NGptr->name);
		        else
            	    PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.", NGptr->name);
			  }
			else
			  {
			    if(name_so_far)
            	    PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s.%.100s", name_so_far, NGptr->name);
		        else
            	    PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", NGptr->name);
			  }

            if(!NET_RegExpSearch(large_buffer, search_string))
				display_this_one = TRUE;	
			else
		        display_this_one=FALSE;
          }

		if(display_this_one)
		  {
			if(!NGptr->children)
			  {

				if(*num_to_display > 0)
				    (*num_to_display)--;

				if(!some_printed)
				  {
					/* INDENT */
					some_printed = TRUE;
				  }

			    if(name_so_far)
				  {
					if(*NGptr->name)
                    	PR_snprintf(large_buffer, BUFFER_SIZE, "%.350s.%.100s", 
								   name_so_far, NGptr->name);
					else
                    	PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", name_so_far);
				  }
			    else
				  {
                   	PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", NGptr->name);
				  }
				rv = MSG_ProcessNewsgroupTree (context,
											   large_buffer, NGptr->name,
											   FALSE, FALSE /* #### modp */);
			  }
			else 
              {

			    if(name_so_far)
			      {
			        StrAllocCopy(new_name_so_far, name_so_far);
				    StrAllocCat(new_name_so_far, ".");
				    StrAllocCat(new_name_so_far, NGptr->name);
			      }
			    else
			      {
				    StrAllocCopy(new_name_so_far, NGptr->name);
			      }

    			if(*num_to_display > 0)
				  {
        			num_in_level = net_NumberOfGroupsInLevel(
												  NGptr->children,
                                                  search_string,
                                                  new_name_so_far,
                                                  print_all);

					/* take care of really big groups */
					if(num_in_level > 50)
	      			  {
						TRACEMSG(("Only showing one level"));
						*num_to_display = 1;
	      			  }
					else
	      			  {
						(*num_to_display) -= (num_in_level-1);
						if(*num_to_display < 0)
							*num_to_display = 1;
	      			  }
				  }

    			if(*num_to_display > 0 && num_in_level)
				  {
					(*num_to_display)--;

					if(!some_printed)
					  {
						/* #### INDENT */
						some_printed = TRUE;
					  }

					/* print out the folder */
                    if(name_so_far)
                        PR_snprintf(large_buffer, BUFFER_SIZE, "%.350s.%.100s",
								   name_so_far, NGptr->name);
                    else
                        PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", NGptr->name);

					rv = MSG_ProcessNewsgroupTree (context,
												   large_buffer, NGptr->name,
												   TRUE,
												   FALSE /* #### modp */);

					if(rv > -1)
                   		rv = net_recursiveDisplayNewsgroups(context,
															NGptr->children, 
											   new_name_so_far, 
											   search_string, 
											   num_to_display, 
											   display_this_one);
      			  }
				else
				  {
					if(*num_to_display > 0)
					    (*num_to_display)--;

					if(!some_printed)
					  {
						/* #### INDENT */
						some_printed = TRUE;
					  }

			    	if(name_so_far)
                   		PR_snprintf(large_buffer, BUFFER_SIZE, "%.350s.%.100s",
								   name_so_far, NGptr->name);
			    	else
                   		PR_snprintf(large_buffer, BUFFER_SIZE, "%.400s", NGptr->name);
					MSG_ProcessNewsgroupTree (context,
											  large_buffer, NGptr->name, TRUE,
											  FALSE /* #### modp */);
      			  }
              }
		  }
		else if(NGptr->children && *num_to_display > 0)
		  {
            if(name_so_far)
              {
                StrAllocCopy(new_name_so_far, name_so_far);
                StrAllocCat(new_name_so_far, ".");
                StrAllocCat(new_name_so_far, NGptr->name);
              }
            else
              {
                StrAllocCopy(new_name_so_far, NGptr->name);
              }

			 /* search for more groups
			  */
			 rv = net_recursiveDisplayNewsgroups(context, NGptr->children,
                                               new_name_so_far,
                                               search_string,
                                               num_to_display,
                                               display_this_one);
		  }

		if(rv < 0)
			return(rv);

        if(cur_num < SGptr->size)
            NGptr = &(SGptr->data)[cur_num++];
        else
            break;

      }

	if(some_printed)
	  {
		/* #### OUTDENT */
	  }

	return(rv);
}

MODULE_PRIVATE int
NET_DisplayNewsgroups(MWContext *context,
					  char * hostname,
					  Bool   is_secure,
					  char            * search_string)
{
    int num_to_display = 300;
	int rv;
	char * new_search_string=0;

	if(!large_buffer)
	   large_buffer = (char *) PR_Malloc(BUFFER_SIZE);
	if(!large_buffer)
		return(0);

	if(!PL_strcmp(search_string, "*"))
	  {
		num_to_display = 0;
	  }
	else 
	  {
		char * star = PL_strchr(search_string, '*');

		if(star && (star-search_string+1 == PL_strlen(search_string)))
		  {
			/* this is a search of the form "comp.*"
		 	 */
			num_to_display = 300;
		  }
		else if(!star)
	  	  {
			/* assume substring match
		 	 * build a new search string
		 	 */
			StrAllocCopy(new_search_string, "*");
			StrAllocCat(new_search_string, search_string);
			StrAllocCat(new_search_string, "*");
			search_string = new_search_string;
	  	  }
	  }


    if(num_to_display > 1)
      {

        num_to_display -= net_NumberOfGroupsInLevel(
                                                  BaseSGStruct,
                                                  search_string,
                                                  NULL,
                                                  FALSE);
        if(num_to_display < 0)
            num_to_display = 0;
      }

	rv = MSG_InitNewsgroupTree (context);
	if (rv < 0) return rv;

	rv = net_recursiveDisplayNewsgroups(context,
										BaseSGStruct,
                                   		  NULL,
                                   		  search_string,
                                   		  &num_to_display,
                                   		  FALSE);

	rv = MSG_FinishNewsgroupTree (context);

	FREEIF(new_search_string);

	return(rv);

}
#endif

PRIVATE void
net_recursiveSaveNewsGroups(SubgroupList *SGptr, 
							 int           level,
							 XP_File       fp)
{
	NewsGroupStruct *NGptr;
	int cur_num=0;
	int32 len = 0, count = 0;

	NGptr = &(SGptr->data)[cur_num++];

	while(NGptr->name)
	  {

		PR_snprintf(large_buffer, BUFFER_SIZE,"%c%.500s%s", '0'+level, NGptr->name, LINEBREAK);
		count = PL_strlen(large_buffer);
		len = XP_FileWrite(large_buffer, count, fp);
		if (len != count)
			return;
			
		if(NGptr->children)
		  {
			net_recursiveSaveNewsGroups(NGptr->children, level+1, fp);
		  }

		if(cur_num < SGptr->size)
			NGptr = &(SGptr->data)[cur_num++];
		else
			break;
	  }
}

MODULE_PRIVATE void
NET_SaveNewsgroupsToDisk(char * hostname, XP_Bool is_secure)
{
	XP_File fp;
	int32 len = 0;

	if(!large_buffer)
	   large_buffer = (char *) PR_Malloc(BUFFER_SIZE);
	if(!large_buffer)
		return;

	if(!NET_NewsgroupsListDirty)
	  {
		TRACEMSG(("It was not neccessary to write the newsgroups to disk"));
		return;
	  }

	TRACEMSG(("Writing newsgroups to disk"));

	fp = XP_FileOpen(hostname, 
					 is_secure ? xpSNewsgroups : xpNewsgroups, 
					 XP_FILE_WRITE_BIN);

	if(!fp)
	  {
		TRACEMSG(("Could not open output file"));
		return;
	  }

	len = XP_FileWrite("# Netscape Newsgroups File (", -1, fp);
	XP_FileWrite(hostname, -1, fp);
	XP_FileWrite(")" LINEBREAK "# This is a generated file!  Do not edit."
				 LINEBREAK LINEBREAK, -1, fp);

	PR_snprintf(large_buffer, BUFFER_SIZE, "%ld%s", NET_NewsGroupsLastUpdate, LINEBREAK);
	len = XP_FileWrite(large_buffer, PL_strlen(large_buffer), fp);

	if((len >= 0) && BaseSGStruct)
    	net_recursiveSaveNewsGroups(BaseSGStruct, 0, fp);

	NET_NewsgroupsListDirty = FALSE;

	XP_FileClose(fp);
}

PRIVATE
SubgroupList *
net_RecursiveNewsgroupsDiskRead(SubgroupList * SGptr,
								int            cur_level,
								int 			*cur_line,
								XP_File        fp)
{
	SubgroupList * rv = SGptr;
	SubgroupList * newSGptr;
	NewsGroupStruct *NGptr=NULL;
	int file_level;
	int cur_num;
	char * subname;

    if(!SGptr)
      {
        SGptr = PR_NEW(SubgroupList);

		if(!SGptr)
			return(NULL);

        SGptr->size = DYN_ARRAY_INIT_SIZE;
        SGptr->data = (NewsGroupStruct *)
                            PR_Malloc(SGptr->size*sizeof(NewsGroupStruct));

        if(!SGptr->data)
		{
			SGptr->size = 0;
        	return(SGptr);
        }
   
        memset(SGptr->data, 0, SGptr->size*sizeof(NewsGroupStruct));

        rv = SGptr;
      }

	/* assume an existing buffer of data 
	 */
	while(1)
	  {
		if(!*large_buffer)
			return rv;  /* buffer empty means file done */

		file_level = (*large_buffer)-'0';
	
		if(file_level < cur_level)
			return(rv);

		if(file_level > cur_level)
		  {
			newSGptr = net_RecursiveNewsgroupsDiskRead(
								NGptr ? NGptr->children : NULL,
								cur_level+1,
								cur_line,
								fp);

			if(NGptr)
			  {
				NGptr->children = newSGptr;
			  }
			else
			  {
				TRACEMSG(("ERROR in newsgroups file (1)"));
			  }

			file_level = (*large_buffer)-'0';
		    if(file_level < cur_level)
			  {
			    return(rv);
			  }
			else
			  {
				/* new line was gotten in recursion */
				continue; 
			  }
		  }

		/* add this subname to the current level
		 */
		subname = large_buffer+1;
		cur_num = 0;
		NGptr = &(SGptr->data)[cur_num];

		/* find last name and add to the bottom
		 */
		while(NGptr && NGptr->name)
	  	  {
			/* assume no duplicates
			 */
			if(cur_num < SGptr->size-1)
			  {
			  	 NGptr = &(SGptr->data)[++cur_num];
			  }
			else
			  {
				if(NGptr->name != NULL)
					cur_num++; /* increment since the entry is used */
			    NGptr = NULL;
			  }
	      }

		/* grow the dyn array if neccessary
		 */
		if(cur_num >= SGptr->size)
		  {
			/* grow the dynamic array
			 */	
			int prev_size = SGptr->size;
			SGptr->size += DYN_ARRAY_GROW_BY;
			SGptr->data = (NewsGroupStruct *)
							     PR_Realloc(SGptr->data, 
									    SGptr->size*sizeof(NewsGroupStruct));

			if(!SGptr->data)
			{
				SGptr->size = 0;
            	return(SGptr);
            }

			/* zero out new fields 
			 */
			memset(&(SGptr->data)[prev_size],
					      0,
					      (SGptr->size-prev_size)*sizeof(NewsGroupStruct));

		  }

		/* create a new name
		 */
		NGptr = &(SGptr->data)[cur_num];
		StrAllocCopy(NGptr->name, subname);

	    if(!XP_FileReadLine(large_buffer, BUFFER_SIZE, fp))
		  {
			*large_buffer = 0;
	    	return(rv);
		  }

	    XP_StripLine(large_buffer);
		(*cur_line)++;
	  }
 
	assert(0);
    return(rv);

}

MODULE_PRIVATE time_t
NET_ReadNewsgroupsFromDisk(char *hostname, XP_Bool is_secure)
{

	XP_File fp;
	int cur_line=3;

	/* probably won't do anything
	 */
	NET_SaveNewsgroupsToDisk(hostname, is_secure);
	NET_FreeNewsgroups(hostname, is_secure);

	if(!large_buffer)
	   large_buffer = (char *) PR_Malloc(BUFFER_SIZE);
	if(!large_buffer)
		return(0);

	TRACEMSG(("Reading Newsgroups from disk"));

	fp = XP_FileOpen(hostname, 
					 is_secure ? xpSNewsgroups : xpNewsgroups, 
					 XP_FILE_READ_BIN);

	if(!fp)
	  {
		TRACEMSG(("Could not open input file"));
		return(0);
	  }

	/* skip first line */
	if(!XP_FileReadLine(large_buffer, BUFFER_SIZE, fp))
	  return(0);
	TRACEMSG(("Read newsgroups cookie: %s", large_buffer));

	/* skip comment and blank lines at beginning. */
	while (1)
	  {
		if(!XP_FileReadLine(large_buffer, BUFFER_SIZE, fp))
		  return(0);
		if (large_buffer[0] != '#' && large_buffer[0] != CR &&
			large_buffer[0] != LF  && large_buffer[0] != 0)
		  break;
	  }

	TRACEMSG(("Read newsgroups last mod date: %s", large_buffer));

	NET_NewsGroupsLastUpdate = atol(large_buffer);

	/* get the first line to be passed in
	 */
	if(!XP_FileReadLine(large_buffer, BUFFER_SIZE, fp))
		return(0);

	XP_StripLine(large_buffer);

	BaseSGStruct = net_RecursiveNewsgroupsDiskRead(BaseSGStruct, 0, &cur_line, fp);

	XP_FileClose(fp);

	StrAllocCopy(NET_CurrentlyLoadedHostname, hostname);
	NET_CurrentlyLoadedListingIsSecure = is_secure;

	return(NET_NewsGroupsLastUpdate);
}

MODULE_PRIVATE time_t
NET_NewsgroupsLastUpdatedTime(char * hostname, XP_Bool is_secure)
{
	if(NET_CurrentlyLoadedHostname && hostname)
	  {
		 if(PL_strcasecmp(hostname, NET_CurrentlyLoadedHostname)
			|| is_secure != NET_CurrentlyLoadedListingIsSecure)
			NET_FreeNewsgroups(hostname, is_secure);
	  }

	if(NET_CurrentlyLoadedHostname)
		return(NET_NewsGroupsLastUpdate);
	else
		return(NET_ReadNewsgroupsFromDisk(hostname, is_secure));
}

/* UNIX doesn't need this stuff since it uses the file system
 * to store the mappings as filenames
 */

XP_List *HostMapList=0;
Bool NewsrcMapDirty=FALSE;     

typedef struct _NewsrcHostMap {
	char * filename;
	char * psuedo_name;
	Bool   is_newsgroups_file;	/* This was Bool * */
} NewsrcHostMap;

/* register a newsrc file mapping
 */
PUBLIC Bool
NET_RegisterNewsrcFile(char * filename, 
					   char *hostname, 
					   Bool is_secure,
					   Bool is_newsgroups_file)
{
	NewsrcHostMap *host_map_struct = PR_NEW(NewsrcHostMap);
    XP_List * list_prt;
	NewsrcHostMap *tmp_host_map_struct;

	if(!host_map_struct)
		return(FALSE);

	memset(host_map_struct, 0, sizeof(NewsrcHostMap));
	
	/* you losers, there's no such thing as XP_Assert in xfe!! */
	assert(filename);
	assert(hostname);

	if(is_secure)
		StrAllocCopy(host_map_struct->psuedo_name, "snewsrc-");
	else
		StrAllocCopy(host_map_struct->psuedo_name, "newsrc-");

	StrAllocCat(host_map_struct->psuedo_name, hostname);

	StrAllocCopy(host_map_struct->filename, filename);

	host_map_struct->is_newsgroups_file = is_newsgroups_file;

	if(!HostMapList)
		HostMapList = XP_ListNew();

	if(!HostMapList)
		return(FALSE);

	NewsrcMapDirty = TRUE;

	/* search for duplicate mappings */
	list_prt = HostMapList;
    while((tmp_host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {
        if(tmp_host_map_struct->is_newsgroups_file == is_newsgroups_file
			&& !PL_strcasecmp(tmp_host_map_struct->psuedo_name, 
					   		host_map_struct->psuedo_name))
		  {
			XP_ListRemoveObject(HostMapList, tmp_host_map_struct);
			FREEIF(tmp_host_map_struct->psuedo_name);
			FREEIF(tmp_host_map_struct->filename);
			FREE(tmp_host_map_struct);

			break; /* end search */
		  }
      }

	XP_ListAddObject(HostMapList, host_map_struct);

	NET_SaveNewsrcFileMappings();

	return(TRUE);
}

/* removes a newshost to file mapping.  Removes both
 * the newsrc and the newsgroups mapping
 * this function does not remove the actual files,
 * that is left up to the caller
 */
PUBLIC void
NET_UnregisterNewsHost(const char *host, 
					   Bool is_secure)
{
    XP_List * list_prt = HostMapList;
    NewsrcHostMap *host_map_struct;
	char *hostname;

	PR_ASSERT(host);

	if(!host)
		return;

    /* Run through the entire list and remove all entries that
	 * match that hostname
	 */
    while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {

        if(is_secure && *host_map_struct->psuedo_name != 's')
            continue;

        if(!is_secure && *host_map_struct->psuedo_name == 's')
            continue;

        hostname = PL_strchr(host_map_struct->psuedo_name, '-');

        if(!hostname)
            continue;

        hostname++;

        if(PL_strcasecmp(hostname, host))
            continue;

		XP_ListRemoveObject(HostMapList, host_map_struct);

		NewsrcMapDirty = TRUE;

		/* free the data */
        FREEIF(host_map_struct->psuedo_name);
        FREEIF(host_map_struct->filename);
        FREE(host_map_struct);

		/* we must start over in the list because the
		 * act of removeing an item will cause purity
		 * errors if we don't start over
		 */
		list_prt = HostMapList;
      }
	NET_SaveNewsrcFileMappings();
}

/* map a filename and security status into a filename
 *
 * returns NULL on error or no mapping.
 */
PUBLIC char *
NET_MapNewsrcHostToFilename(char * host, 
							Bool is_secure, 
							Bool is_newsgroups_file)
{
    XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	char *hostname;

    while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {
		if(is_newsgroups_file != host_map_struct->is_newsgroups_file)
			continue;

		if(is_secure && *host_map_struct->psuedo_name != 's')
			continue;
		
		if(!is_secure && *host_map_struct->psuedo_name == 's')
			continue;

		hostname = PL_strchr(host_map_struct->psuedo_name, '-');
		
		if(!hostname)
			continue;

		hostname++;

		if(PL_strcasecmp(hostname, host))
			continue;

		return(host_map_struct->filename);

	  }

	return(NULL);
}

#define NEWSRC_MAP_FILE_COOKIE "netscape-newsrc-map-file"

/* save newsrc file mappings from memory onto disk
 */
PUBLIC Bool
NET_SaveNewsrcFileMappings()
{
	XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	XP_File fp;
	int32 len = 0;

	if(!NewsrcMapDirty)
		return(TRUE);

	if(!(fp = XP_FileOpen("", xpNewsrcFileMap, XP_FILE_WRITE_BIN)))
	  {
		return(FALSE);
	  }

	len = XP_FileWrite(NEWSRC_MAP_FILE_COOKIE, PL_strlen(NEWSRC_MAP_FILE_COOKIE), fp);
	if (len < 0)
	{
		XP_FileClose(fp);
		return FALSE;
	}
	XP_FileWrite(LINEBREAK, PL_strlen(LINEBREAK), fp);

	while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
	  {

		XP_FileWrite(host_map_struct->psuedo_name, 
					 PL_strlen(host_map_struct->psuedo_name),
					 fp);
		
		XP_FileWrite("\t", 1, fp);

		XP_FileWrite(host_map_struct->filename, 
					 PL_strlen(host_map_struct->filename),
					 fp);
		
		XP_FileWrite("\t", 1, fp);

		if(host_map_struct->is_newsgroups_file)
		    XP_FileWrite("TRUE", 4, fp);
		else
		    XP_FileWrite("FALSE", 5, fp);

		len = XP_FileWrite(LINEBREAK, PL_strlen(LINEBREAK), fp);
		if (len < 0)
		{
			XP_FileClose(fp);
			return FALSE;
		}
	  }

	XP_FileClose(fp);

	NewsrcMapDirty = FALSE;

	return(TRUE);
}

/* read newsrc file mappings from disk into memory
 */
PUBLIC Bool
NET_ReadNewsrcFileMappings()
{
	XP_File fp;
	char buffer[512];
	char psuedo_name[512];
	char filename[512];
	char is_newsgroup[512];
	NewsrcHostMap * hs_struct;

    if(!(fp = XP_FileOpen("", xpNewsrcFileMap, XP_FILE_READ_BIN)))
      {
        return(FALSE);
      }

    if(!HostMapList)
        HostMapList = XP_ListNew();

	/* get the cookie and ignore */
	XP_FileReadLine(buffer, sizeof(buffer), fp);

	while(XP_FileReadLine(buffer, sizeof(buffer), fp))
	  {
        char * p;
        int i;

		hs_struct = PR_NEW(NewsrcHostMap);

		if(!hs_struct)
			return(FALSE);

		memset(hs_struct, 0, sizeof(NewsrcHostMap));

        /*
            This used to be scanf() call which would incorrectly
            parse long filenames with spaces in them.  - JRE
        */

        filename[0] = '\0';
        is_newsgroup[0]='\0';

        for (i = 0, p = buffer; *p && *p != '\t' && i < 500; p++, i++)
            psuedo_name[i] = *p;
        psuedo_name[i] = '\0';
        if (*p) 
          {
            for (i = 0, p++; *p && *p != '\t' && i < 500; p++, i++)
                filename[i] = *p;
            filename[i]='\0';
            if (*p) 
              {
                for (i = 0, p++; *p && *p != '\r' && i < 500; p++, i++)
                    is_newsgroup[i] = *p;
                is_newsgroup[i]='\0';
              }
          }

		StrAllocCopy(hs_struct->psuedo_name, psuedo_name);
		StrAllocCopy(hs_struct->filename, filename);

		if(!PL_strncmp(is_newsgroup, "TRUE", 4))
			hs_struct->is_newsgroups_file = TRUE;

		XP_ListAddObject(HostMapList, hs_struct);		
	  }


    XP_FileClose(fp);

	return(TRUE);
}

PUBLIC void 
NET_FreeNewsrcFileMappings(void)
{
    NewsrcHostMap *host_map_struct;

    while((host_map_struct = (NewsrcHostMap *)XP_ListRemoveTopObject(HostMapList)) != NULL)
      {
        FREEIF(host_map_struct->psuedo_name);
        FREEIF(host_map_struct->filename);
        FREE(host_map_struct);
      }
}


#if defined(DEBUG) && defined(XP_UNIX)

PUBLIC void
TestNewsrcFileMappings(void)
{

  printf("secure flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", TRUE, FALSE));
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));

  NET_RegisterNewsrcFile("newsrc-flop", "flop", FALSE, FALSE);
  NET_RegisterNewsrcFile("snewsrc-flop", "flop", TRUE, FALSE);
  NET_RegisterNewsrcFile("newsrc-news", "news", FALSE, FALSE);

  printf("secure flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", TRUE, FALSE));
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));

	printf("saving mappings\n");
  NET_SaveNewsrcFileMappings();
	printf("freeing mappings\n");
  NET_FreeNewsrcFileMappings();
	printf("reading mappings\n");
  NET_ReadNewsrcFileMappings();
  
  printf("adding duplicates\n");
  NET_RegisterNewsrcFile("newsrc-flop", "flop", FALSE, FALSE);
  NET_RegisterNewsrcFile("snewsrc-flop", "flop", TRUE, FALSE);
  NET_RegisterNewsrcFile("newsrc-news", "news", FALSE, FALSE);

  printf("secure flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", TRUE, FALSE));
  printf("insec flop maps to: %s\n", NET_MapNewsrcHostToFilename("flop", FALSE, FALSE));
  printf("insec news maps to: %s\n", NET_MapNewsrcHostToFilename("news", FALSE, FALSE));
}
#endif

/* XP_GetNewsRCFiles returns a null terminated array
 * of pointers to malloc'd filename's.  Each filename
 * represents a different newsrc file.
 *
 * return only the filename since the path is not needed
 * or wanted.
 *
 * Netlib is expecting a string of the form:
 *  [s]newsrc-host.name.domain[:port]
 *
 * examples:
 *    newsrc-news.mcom.com
 *    snewsrc-flop.mcom.com
 *    newsrc-news.mcom.com:118
 *    snewsrc-flop.mcom.com:1191
 *
 * the port number is optional and should only be
 * there when different from the default.
 * the "s" in front of newsrc signifies that
 * security is to be used.
 *
 * return NULL on critical error or no files
 */                                   
#if !defined(XP_UNIX) 
PUBLIC char ** XP_GetNewsRCFiles(void)
{
	/* @@@ max number of newsrc's is 512
	 */
	char ** rv = (char **)PR_Malloc(sizeof(char*)*512);
    XP_List * list_prt = HostMapList;
	NewsrcHostMap *host_map_struct;
	int count=0;

	if(!rv)
		return(NULL);

	memset(rv, 0, sizeof(char*)*512);

	while((host_map_struct = (NewsrcHostMap *)XP_ListNextObject(list_prt)) != NULL)
      {

        if(host_map_struct->is_newsgroups_file)
            continue;

		StrAllocCopy(rv[count++], host_map_struct->psuedo_name);

		if(count >= 510)
			break;
	  }

	return(rv);
}
#endif /* UNIX */


